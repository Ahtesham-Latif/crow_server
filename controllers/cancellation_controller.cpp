#include "cancellation_controller.h"
#include "../models/cancellation.h"
#include "../services/public_session.h"
#include <crow.h>
#include <sqlite3.h>
#include "../config/n8n_config.h"
#include <thread>
#include <iostream>
#include <string>

void registerCancellationRoutes(crow::SimpleApp& app, sqlite3* db) {
    CROW_ROUTE(app, "/cancel_appointment").methods("POST"_method)
    ([db](const crow::request& req) {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Please send a valid request.");

        // Fixed r_string incompatibility by explicitly creating std::string
        int appointment_id = body.has("appointment_id") ? body["appointment_id"].i() : -1;
        int patient_id     = body.has("patient_id")     ? body["patient_id"].i()     : -1;
        std::string name   = body.has("name")           ? std::string(body["name"].s())   : "";
        std::string email  = body.has("email")          ? std::string(body["email"].s())  : "";
        int age            = body.has("age")            ? body["age"].i()            : -1;

        if (appointment_id <= 0 || patient_id <= 0 || name.empty() || email.empty())
            return crow::response(400, "Please provide all required details.");

        // --- Fetch patient info & VERIFY identity ---
        PatientInfo info;
        bool verified = Cancellation::getPatientInfoForCancellation(db, patient_id, name, email, age, info);

        if (!verified) {
            return crow::response(403, "Sorry, we could not verify those details. Please check and try again.");
        }

        // --- Update status in DB (No deletion) ---
        bool ok = Cancellation::cancelAppointment(db, appointment_id, patient_id);

        // --- Respond to client ---
        crow::json::wvalue res;
        res["success"] = ok;
        res["message"] = ok ? "Your appointment has been cancelled successfully." 
                            : "Sorry, we could not cancel the appointment right now. Please try again.";
        auto response = crow::response(ok ? 200 : 500, res);

        // --- Send to N8N ---
        if (ok) {
            crow::json::wvalue payload;
            payload["patient_id"] = patient_id;
            payload["appointment_id"] = appointment_id;
            payload["status"] = "cancelled";
            payload["name"] = info.name;
            payload["age"] = info.age;
            payload["email"] = info.email;
            payload["request"] = "Cancelled the booking";

            std::thread([payload]() {
                sendToN8N(payload);
            }).detach();
        }

        return response;
    });
}
