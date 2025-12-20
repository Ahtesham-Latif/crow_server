#include "patient_controller.h"
#include "../models/patient.h"
#include <iostream>

void registerPatientRoutes(crow::SimpleApp& app, sqlite3* db) {

    // ---------------------------------
    // POST: Create new patient
    // ---------------------------------
    CROW_ROUTE(app, "/add_patient").methods("POST"_method)
    ([db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("name") || !body.has("age") || !body.has("email") || !body.has("gender")) {
            return crow::response(400, "Missing patient information");
        }

        std::string name = body["name"].s();
        int age = body["age"].i();
        std::string email = body["email"].s();
        std::string gender = body["gender"].s();

        int patient_id = Patient::insert(db, name, age, email, gender);
        if (patient_id <= 0) {
            return crow::response(500, "Failed to insert patient");
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["patient_id"] = patient_id;
        res["message"] = "Patient added successfully";
        return crow::response(200, res);
    });
}
