#include "cancellation_controller.h"
#include "../models/cancellation.h"
#include <crow.h>
#include <sqlite3.h>
#include "../config/n8n_config.h"
#include <thread>
#include <iostream>

void registerCancellationRoutes(crow::SimpleApp& app, sqlite3* db) {
    CROW_ROUTE(app, "/cancel_appointment").methods("POST"_method)
    ([db](const crow::request& req) {

        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");

        int appointment_id = body.has("appointment_id") ? body["appointment_id"].i() : -1;
        int patient_id = body.has("patient_id") ? body["patient_id"].i() : -1;

        if (appointment_id <= 0 || patient_id <= 0)
            return crow::response(400, "Invalid appointment_id or patient_id");

        // --- Fetch patient info BEFORE deletion ---
        PatientInfo info;
        bool patientFound = Cancellation::getPatientInfo(db, patient_id, info);

        if (!patientFound) {
            return crow::response(404, "Patient not found");
        }

        // --- Cancel appointment & patient ---
        bool ok = Cancellation::cancelAppointment(db, appointment_id, patient_id);

        // --- Respond to client ---
        crow::json::wvalue res;
        res["success"] = ok;
        res["message"] = ok ? "Appointment and patient deleted successfully!" 
                            : "Failed to cancel appointment. Check IDs.";
        auto response = crow::response(ok ? 200 : 404, res);

        // --- Send full payload to N8N asynchronously ---
        crow::json::wvalue payload;
        payload["patient_id"] = patient_id;
        payload["appointment_id"] = appointment_id;
        payload["status"] = ok ? "cancelled" : "failed";
        payload["name"] = info.name;
        payload["age"] = info.age;
        payload["email"] = info.email;
        payload["request"] = info.request;

        std::thread([payload]() {
            sendToN8N(payload);
        }).detach();

        return response;
    });
}
