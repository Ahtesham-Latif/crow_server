#include "appointment_controller.h"
#include "../models/patient.h"
#include "../models/doctor.h"
#include "../models/appointment.h"
#include <iostream>
#include <random>

int generateRandomID(int min = 100000, int max = 999999) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

void registerAppointmentRoutes(crow::SimpleApp& app, sqlite3* db) {

    // ---------------------------------
    // POST: Book an appointment
    // ---------------------------------
    CROW_ROUTE(app, "/book_appointment").methods("POST"_method)
    ([db](const crow::request& req) {

        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");

        // --- Extract form data ---
        std::string name = body["name"].s();
        int age = body["age"].i();
        std::string email = body["email"].s();
        std::string gender = body["gender"].s();
        std::string doctor_name = body["doctor_name"].s();
        std::string appointment_date = body["date"].s();
        std::string slot_time = body["slot"].s();

        if (name.empty() || doctor_name.empty() || appointment_date.empty() || slot_time.empty()) {
            return crow::response(400, "Missing required fields");
        }

        sqlite3_stmt* stmt;

        // --- Step 1: Verify doctor exists ---
        int doctor_id = -1;
        const char* sql_doctor = "SELECT doctor_id FROM Doctor WHERE doctor_name = ?";
        if (sqlite3_prepare_v2(db, sql_doctor, -1, &stmt, nullptr) != SQLITE_OK)
            return crow::response(500, "Database error");

        sqlite3_bind_text(stmt, 1, doctor_name.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) doctor_id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        if (doctor_id == -1) return crow::response(400, "Doctor not found");

        // --- Step 2: Verify schedule exists for this slot ---
        int schedule_id = -1;
        const char* sql_schedule = "SELECT schedule_id FROM Doctor_Schedule WHERE time_slot = ?";
        if (sqlite3_prepare_v2(db, sql_schedule, -1, &stmt, nullptr) != SQLITE_OK)
            return crow::response(500, "Database error");

        sqlite3_bind_text(stmt, 1, slot_time.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) schedule_id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        if (schedule_id == -1) return crow::response(400, "Schedule slot not found");

        // --- Step 3: Generate unique random patient ID ---
        int patient_id;
        do {
            patient_id = generateRandomID();
        } while (Patient::exists(db, patient_id)); // ensure uniqueness

        // --- Step 4: Insert patient ---
        if (!Patient::insert(db, patient_id, name, age, email, gender))
            return crow::response(500, "Failed to insert patient");

        // --- Step 5: Generate unique random appointment ID ---
        int appointment_id;
        do {
            appointment_id = generateRandomID();
        } while (Appointment::exists(db, appointment_id));

        // --- Step 6: Insert appointment ---
        if (!Appointment::insert(db, appointment_id, patient_id, doctor_id, schedule_id, appointment_date))
            return crow::response(500, "Failed to book appointment");

        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "Appointment booked successfully!";
        res["patient_id"] = patient_id;
        res["appointment_id"] = appointment_id; // return generated IDs
        return crow::response(200, res);
    });
}
