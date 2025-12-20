#include "patient_controller.h"
#include "../models/patient.h"
#include <iostream>
#include <cstdlib>
#include <ctime>

void registerPatientRoutes(crow::SimpleApp& app, sqlite3* db) {

    // Seed random once (better in main.cpp ideally)
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }

    // ---------------------------------
    // POST: Create new patient
    // ---------------------------------
    CROW_ROUTE(app, "/add_patient").methods("POST"_method)
    ([db](const crow::request& req) {

        auto body = crow::json::load(req.body);
        if (!body || !body.has("name") || !body.has("age")
            || !body.has("email") || !body.has("gender")) {
            return crow::response(400, "Missing patient information");
        }

        std::string name   = body["name"].s();
        int age            = body["age"].i();
        std::string email  = body["email"].s();
        std::string gender = body["gender"].s();

        // OPTIONAL field
        std::string request =
            body.has("request") && body["request"].t() == crow::json::type::String
                ? std::string(body["request"].s())
                : "";

        // --- Generate unique random 6-digit patient_id ---
        int patient_id;
        do {
            patient_id = 100000 + std::rand() % 900000; // 100000–999999
        } while (Patient::exists(db, patient_id));

        // --- Insert patient ---
        if (!Patient::insert(db, patient_id, name, age, email, gender, request)) {
            std::cerr << "[ERROR] Failed to insert patient: " << patient_id << "\n";
            return crow::response(500, "Failed to insert patient");
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["patient_id"] = patient_id;
        res["message"] = "Patient added successfully";
        return crow::response(200, res);
    });
}
