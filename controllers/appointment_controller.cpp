#include "appointment_controller.h"
#include "../models/patient.h"
#include "../models/doctor.h"
#include "../models/appointment.h"
#include "../config/n8n_config.h"
#include "../services/utils.h"
#include <iostream>
#include <thread>

void registerAppointmentRoutes(crow::SimpleApp& app, sqlite3* db) {

    CROW_ROUTE(app, "/book_appointment").methods("POST"_method)
    ([db](const crow::request& req) {

        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "1. Invalid JSON");
        }

        // --- Required fields check ---
        if (!body.has("name") || !body.has("age") || !body.has("email") ||
            !body.has("gender") || !body.has("doctor_name") ||
            !body.has("date") || !body.has("time_slot")) 
        {
            return crow::response(400, "2. Missing required fields");
        }

        std::string name = body["name"].s();
        int age = body["age"].i();
        std::string email = body["email"].s();
        std::string gender = body["gender"].s();
        std::string doctor_name = body["doctor_name"].s();
        std::string appointment_date = body["date"].s();
        std::string time_slot = body["time_slot"].s();
        std::string request = body.has("request") ? std::string(body["request"].s()) : "";

        std::cout << "[DEBUG] Received request: "
                  << name << ", " << age << ", " << email << ", " 
                  << gender << ", " << doctor_name << ", " 
                  << appointment_date << ", " << time_slot << ", " 
                  << request << "\n";

        // --- Step 1: Verify doctor exists ---
        int doctor_id = -1;
        const char* sql_doctor = "SELECT doctor_id FROM Doctor WHERE doctor_name = ?";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql_doctor, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "3. Prepare doctor query failed: " + std::string(sqlite3_errmsg(db)));
        }

        sqlite3_bind_text(stmt, 1, doctor_name.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) doctor_id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        if (doctor_id == -1) {
            return crow::response(400, "4. Doctor not found: " + doctor_name);
        }
        std::cout << "[DEBUG] Doctor ID found: " << doctor_id << "\n";

       // --- Step 2: Get schedule_id for this slot ---
        int schedule_id = -1;
const char* sql_schedule = "SELECT schedule_id FROM Doctor_Schedule WHERE time_slot = ?";

if (sqlite3_prepare_v2(db, sql_schedule, -1, &stmt, nullptr) != SQLITE_OK) {
    return crow::response(500, "5. Prepare schedule query failed: " + std::string(sqlite3_errmsg(db)));
}

sqlite3_bind_text(stmt, 1, time_slot.c_str(), -1, SQLITE_STATIC);
if (sqlite3_step(stmt) == SQLITE_ROW) schedule_id = sqlite3_column_int(stmt, 0);
sqlite3_finalize(stmt);

if (schedule_id == -1) {
    return crow::response(400, "6. Schedule slot not found: " + time_slot);
}
std::cout << "[DEBUG] Schedule ID found: " << schedule_id << "\n";

        // --- Step 3: Generate unique random patient ID ---
        int patient_id;
        try {
            do { patient_id = generateRandomID(); } while (Patient::exists(db, patient_id));
        } catch (...) {
            return crow::response(500, "7. Error generating unique patient ID");
        }
        std::cout << "[DEBUG] Generated patient ID: " << patient_id << "\n";

        // --- Step 4: Insert patient ---
        if (!Patient::insert(db, patient_id, name, age, email, gender, request)) {
            return crow::response(500, "8. Failed to insert patient. Patient ID: " + std::to_string(patient_id));
        }
        std::cout << "[DEBUG] Patient inserted successfully\n";

        // --- Step 5: Generate unique random appointment ID ---
        int appointment_id;
        try {
            do { appointment_id = generateRandomID(); } while (Appointment::exists(db, appointment_id));
        } catch (...) {
            return crow::response(500, "9. Error generating unique appointment ID");
        }
        std::cout << "[DEBUG] Generated appointment ID: " << appointment_id << "\n";

        // --- Step 6: Insert appointment ---
        if (!Appointment::insert(db, appointment_id, patient_id, doctor_id, schedule_id, appointment_date)) {
            return crow::response(500, "10. Failed to insert appointment. Appointment ID: " + std::to_string(appointment_id));
        }
        std::cout << "[DEBUG] Appointment inserted successfully\n";

        // --- Step 7: Return response ---
        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "Appointment booked successfully!";
        res["patient_id"] = patient_id;
        res["appointment_id"] = appointment_id;

        // --- Step 8: Send data to N8N asynchronously ---
        crow::json::wvalue payload;
        payload["patient_id"] = patient_id;
        payload["appointment_id"] = appointment_id;
        payload["name"] = name;
        payload["age"] = age;
        payload["email"] = email;
        payload["gender"] = gender;
        payload["request"] = request;
        payload["doctor_name"] = doctor_name;
        payload["appointment_date"] = appointment_date;
        payload["time_slot"] = time_slot;

        std::thread([payload]() {
            sendToN8N(payload);
        }).detach();

        return crow::response(200, res);
    });
}
