#include "patient_controller.h"
#include "../models/patient.h"
#include <iostream>
#include <cstdlib> // for rand()
#include <ctime>   // for time()

void registerPatientRoutes(crow::SimpleApp& app, sqlite3* db) {

    // Initialize random seed once (can also do this in main.cpp)
    std::srand(std::time(nullptr));

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

        // --- Generate random patient_id ---
        int patient_id = 100000 + std::rand() % 900000; // 6-digit ID

        // Optional: avoid collisions
        while (Patient::exists(db, patient_id)) {
            patient_id = 100000 + std::rand() % 900000;
        }

        // --- Insert patient ---
        bool ok = Patient::insert(db, patient_id, name, age, email, gender);
        if (!ok) {
            return crow::response(500, "Failed to insert patient");
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["patient_id"] = patient_id;
        res["message"] = "Patient added successfully";
        return crow::response(200, res);
    });
}
