#include "cancellation_controller.h"
#include "../models/cancellation.h"
#include <crow.h>
#include <sqlite3.h>
#include "../config/n8n_config.h" // contains N8N_WEBHOOK and sendToN8N()
#include <thread>
#include <iostream>

void registerCancellationRoutes(crow::SimpleApp& app, sqlite3* db) {

    // POST: /cancel_appointment
    CROW_ROUTE(app, "/cancel_appointment").methods("POST"_method)
    ([db](const crow::request& req) {

        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "Invalid JSON");
        }

        // --- Extract IDs ---
        int appointment_id = body.has("appointment_id") ? body["appointment_id"].i() : -1;
        int patient_id = body.has("patient_id") ? body["patient_id"].i() : -1;

        std::cout << "[DEBUG] Cancel request received: appointment_id=" 
                  << appointment_id << ", patient_id=" << patient_id << "\n";

        if (appointment_id <= 0 || patient_id <= 0) {
            std::cerr << "[ERROR] Invalid appointment_id or patient_id\n";
            return crow::response(400, "Invalid appointment_id or patient_id");
        }

        // --- Attempt cancellation ---
        bool ok = Cancellation::cancelAppointment(db, appointment_id, patient_id);

        // --- Step 1: Respond to client immediately ---
        crow::json::wvalue res;
        res["success"] = ok;
        res["message"] = ok ? "Appointment and patient deleted successfully!" 
                            : "Failed to cancel appointment. Check IDs.";
        auto response = crow::response(ok ? 200 : 404, res);

        // --- Step 2: Send cancellation data to N8N asynchronously ---
        crow::json::wvalue payload;
        payload["patient_id"] = patient_id;
        payload["appointment_id"] = appointment_id;
        payload["status"] = ok ? "cancelled" : "failed";

        std::thread([payload]() {
            sendToN8N(payload);
        }).detach();

        return response;
    });
}
