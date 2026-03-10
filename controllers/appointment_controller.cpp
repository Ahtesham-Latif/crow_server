#include "appointment_controller.h"

#include "../models/patient.h"
#include "../models/doctor.h"
#include "../models/appointment.h"
#include "../config/n8n_config.h"
#include "../services/utils.h"

#include <iostream>
#include <thread>

static bool isSlotAlreadyBooked(sqlite3* db, int doctor_id, int schedule_id, const std::string& appointment_date)
{
    const char* sql =
        "SELECT 1 FROM Appointment "
        "WHERE doctor_id = ? AND schedule_id = ? AND appointment_date = ? AND status = 'BOOKED' "
        "LIMIT 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        // fail-safe: treat as booked
        return true;
    }

    sqlite3_bind_int(stmt, 1, doctor_id);
    sqlite3_bind_int(stmt, 2, schedule_id);
    sqlite3_bind_text(stmt, 3, appointment_date.c_str(), -1, SQLITE_TRANSIENT);

    bool booked = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return booked;
}

static bool isSlotBlocked(sqlite3* db, int doctor_id, int schedule_id, const std::string& appointment_date)
{
    const char* sql =
        "SELECT 1 FROM Doctor_Blocked_Slots "
        "WHERE doctor_id = ? AND schedule_id = ? AND appointment_date = ? "
        "LIMIT 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        // fail-safe: treat as blocked
        return true;
    }

    sqlite3_bind_int(stmt, 1, doctor_id);
    sqlite3_bind_int(stmt, 2, schedule_id);
    sqlite3_bind_text(stmt, 3, appointment_date.c_str(), -1, SQLITE_TRANSIENT);

    bool blocked = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return blocked;
}

void registerAppointmentRoutes(crow::SimpleApp& app, sqlite3* db)
{
    CROW_ROUTE(app, "/book_appointment").methods("POST"_method)
    ([db](const crow::request& req)
    {
        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "Please send a valid request.");
        }

        // --- Required fields ---
        if (!body.has("name") || !body.has("age") || !body.has("email") ||
            !body.has("gender") || !body.has("doctor_name") ||
            !body.has("date") || !body.has("time_slot"))
        {
            return crow::response(400, "Please fill in all required fields.");
        }

        std::string name              = body["name"].s();
        int         age               = body["age"].i();
        std::string email             = body["email"].s();
        std::string gender            = body["gender"].s();
        std::string doctor_name       = body["doctor_name"].s();
        std::string appointment_date  = body["date"].s();
        std::string time_slot         = body["time_slot"].s();
        std::string request  ;
if (body.has("request")) {
    request = std::string(body["request"].s());
} else {
    request = "";
}


        std::cout << "[DEBUG] Received request: "
                  << name << ", " << age << ", " << email << ", "
                  << gender << ", " << doctor_name << ", "
                  << appointment_date << ", " << time_slot << ", "
                  << request << "\n";

        sqlite3_stmt* stmt = nullptr;

        // --- Step 1: Get doctor_id ---
        int doctor_id = -1;
        const char* sql_doctor =
            "SELECT doctor_id FROM Doctor WHERE doctor_name = ?";

        if (sqlite3_prepare_v2(db, sql_doctor, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't complete your request right now. Please try again.");
        }

        sqlite3_bind_text(stmt, 1, doctor_name.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            doctor_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        if (doctor_id == -1) {
            return crow::response(400, "Sorry, we could not find that doctor. Please choose another.");
        }

        std::cout << "[DEBUG] Doctor ID found: " << doctor_id << "\n";

        // --- Step 2: Get schedule_id ---
        int schedule_id = -1;
        const char* sql_schedule =
            "SELECT schedule_id FROM Doctor_Schedule WHERE time_slot = ?";

        if (sqlite3_prepare_v2(db, sql_schedule, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't complete your request right now. Please try again.");
        }

        sqlite3_bind_text(stmt, 1, time_slot.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            schedule_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        if (schedule_id == -1) {
            return crow::response(400, "Please select a valid time slot.");
        }

        std::cout << "[DEBUG] Schedule ID found: " << schedule_id << "\n";

        // --- Step 3: Slot safety check ---
        if (isSlotBlocked(db, doctor_id, schedule_id, appointment_date)) {
            return crow::response(409, "Sorry, that slot is blocked by the doctor for this date.");
        }
        if (isSlotAlreadyBooked(db, doctor_id, schedule_id, appointment_date)) {
            return crow::response(409, "Sorry, that slot has already been booked.");
        }

        // --- Step 4: Generate unique patient_id ---
        int patient_id;
        do {
            patient_id = generateRandomID();
        } while (Patient::exists(db, patient_id));

        std::cout << "[DEBUG] Generated patient ID: " << patient_id << "\n";

        // --- Step 5: Insert patient ---
        if (!Patient::insert(db, patient_id, name, age, email, gender, request)) {
            return crow::response(500, "Sorry, we couldn't save your details right now. Please try again.");
        }

        std::cout << "[DEBUG] Patient inserted successfully\n";

        // --- Step 6: Generate unique appointment_id ---
        int appointment_id;
        do {
            appointment_id = generateRandomID();
        } while (Appointment::exists(db, appointment_id));

        std::cout << "[DEBUG] Generated appointment ID: " << appointment_id << "\n";

        // --- Step 7: Insert appointment ---
        if (!Appointment::insert(
                db,
                appointment_id,
                patient_id,
                doctor_id,
                schedule_id,
                appointment_date))
        {
            return crow::response(500, "Sorry, we couldn't finalize the appointment. Please try again.");
        }

        std::cout << "[DEBUG] Appointment inserted successfully\n";

        // --- Step 8: Response ---
        crow::json::wvalue res;
        res["success"]        = true;
        res["message"]        = "Appointment booked successfully.";
        res["patient_id"]     = patient_id;
        res["appointment_id"] = appointment_id;

        // --- Step 9: Async N8N ---
        crow::json::wvalue payload;
        payload["patient_id"]        = patient_id;
        payload["appointment_id"]    = appointment_id;
        payload["name"]              = name;
        payload["age"]               = age;
        payload["email"]             = email;
        payload["gender"]            = gender;
        payload["request"]           = request;
        payload["doctor_name"]       = doctor_name;
        payload["appointment_date"]  = appointment_date;
        payload["time_slot"]         = time_slot;

        std::thread([payload]() {
            sendToN8N(payload);
        }).detach();

        return crow::response(200, res);
    });
}
