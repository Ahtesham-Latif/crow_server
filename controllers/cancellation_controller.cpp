#include "cancellation_controller.h"
#include "../models/cancellation.h"
#include <crow.h>
#include <sqlite3.h>

void registerCancellationRoutes(crow::SimpleApp& app, sqlite3* db) {
    // POST: /cancel_appointment
    CROW_ROUTE(app, "/cancel_appointment").methods("POST"_method)
    ([db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");

        int appointment_id = body["appointment_id"].i();
        int patient_id = body["patient_id"].i();

        if (appointment_id <= 0 || patient_id <= 0)
            return crow::response(400, "Invalid appointment_id or patient_id");

        bool ok = Cancellation::cancelAppointment(db, appointment_id, patient_id);

        crow::json::wvalue res;
        if (ok) {
            res["success"] = true;
            res["message"] = "Appointment and patient deleted successfully!";
            return crow::response(200, res);
        } else {
            res["success"] = false;
            res["message"] = "Failed to cancel appointment. Check IDs.";
            return crow::response(404, res);
        }
    });
}
