#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
#include <iostream>
#include "patient.h"        // Patient model
#include "schedule.h"       // DoctorSchedule model
#include "doctor.h"         // Doctor model
#include "../services/utils.h"  // instead of just "utils.h"
         // for generateRandomID()

class Appointment {
public:
    int appointment_id;
    int patient_id;
    int doctor_id;
    int schedule_id;
    std::string appointment_date; // YYYY-MM-DD
    std::string status;           // BOOKED, CANCELLED
    std::string created_at;       // default: datetime('now','localtime')

    Appointment() = default;

    Appointment(int patient, int doctor, int schedule, const std::string& date,
                const std::string& stat = "BOOKED")
        : patient_id(patient), doctor_id(doctor), schedule_id(schedule),
          appointment_date(date), status(stat) {}

    // Check if appointment ID exists
    static bool exists(sqlite3* db, int appointment_id) {
        const char* sql = "SELECT 1 FROM Appointment WHERE appointment_id = ?";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Exists check failed: " << sqlite3_errmsg(db) << "\n";
            return false;
        }

        sqlite3_bind_int(stmt, 1, appointment_id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_ROW;
    }

    // Insert appointment with random pre-generated ID
    static bool insert(sqlite3* db, int appointment_id, int patient_id, int doctor_id,
                       int schedule_id, const std::string& date) 
    {
        // --- Validate inputs ---
        if (patient_id <= 0 || doctor_id <= 0 || schedule_id <= 0 || date.empty()) {
            std::cerr << "[ERROR] Insert failed: invalid input values\n";
            return false;
        }

        // --- Check for appointment ID collision ---
        if (exists(db, appointment_id)) {
            std::cerr << "[ERROR] Insert failed: appointment_id " << appointment_id << " already exists\n";
            return false;
        }

        const char* sql =
            "INSERT INTO Appointment(appointment_id, patient_id, doctor_id, schedule_id, appointment_date, status, created_at) "
            "VALUES (?, ?, ?, ?, ?, 'BOOKED', datetime('now','localtime'));";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, appointment_id);
        sqlite3_bind_int(stmt, 2, patient_id);
        sqlite3_bind_int(stmt, 3, doctor_id);
        sqlite3_bind_int(stmt, 4, schedule_id);
        sqlite3_bind_text(stmt, 5, date.c_str(), -1, SQLITE_TRANSIENT);

        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            std::cerr << "[ERROR] Insert failed: " << sqlite3_errmsg(db) << std::endl;
        } else {
            std::cout << "[DEBUG] Appointment inserted successfully: ID=" << appointment_id << "\n";
        }

        sqlite3_finalize(stmt);
        return ok;
    }

    // Fetch appointments for a doctor on a specific date
    static std::vector<Appointment> fetchByDoctorAndDate(sqlite3* db, int doctor_id, const std::string& date) {
        std::vector<Appointment> appointments;
        const char* sql =
            "SELECT appointment_id, patient_id, doctor_id, schedule_id, appointment_date, status, created_at "
            "FROM Appointment "
            "WHERE doctor_id = ? AND appointment_date = ? "
            "ORDER BY schedule_id;";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return appointments;
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Appointment a;
            a.appointment_id = sqlite3_column_int(stmt, 0);
            a.patient_id = sqlite3_column_int(stmt, 1);
            a.doctor_id = sqlite3_column_int(stmt, 2);
            a.schedule_id = sqlite3_column_int(stmt, 3);
            a.appointment_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            a.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            a.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            appointments.push_back(a);
        }

        sqlite3_finalize(stmt);
        return appointments;
    }
};
