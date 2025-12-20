#pragma once
#include <sqlite3.h>
#include <string>
#include <iostream>

class Appointment {
public:
    int appointment_id;
    int patient_id;
    int doctor_id;
    int schedule_id;
    std::string date;

    // Insert with pre-generated appointment ID
    static bool insert(sqlite3* db, int appointment_id, int patient_id, int doctor_id, int schedule_id, const std::string& date) {
        const char* sql = "INSERT INTO Appointment(appointment_id, patient_id, doctor_id, schedule_id, appointmentDateTime) VALUES (?, ?, ?, ?, ?)";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, appointment_id);
        sqlite3_bind_int(stmt, 2, patient_id);
        sqlite3_bind_int(stmt, 3, doctor_id);
        sqlite3_bind_int(stmt, 4, schedule_id);
        sqlite3_bind_text(stmt, 5, date.c_str(), -1, SQLITE_TRANSIENT);

        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    // Check if appointment ID exists
    static bool exists(sqlite3* db, int appointment_id) {
        const char* sql = "SELECT 1 FROM Appointment WHERE appointment_id = ?";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, appointment_id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_ROW;
    }
};
