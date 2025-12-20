#pragma once
#include <sqlite3.h>
#include <iostream>

class Cancellation {
public:
    // Cancel an appointment and delete patient as well
    static bool cancelAppointment(sqlite3* db, int appointment_id, int patient_id) {
        if (!db || appointment_id <= 0 || patient_id <= 0) return false;

        sqlite3_stmt* stmt;

        // --- Step 1: Check if appointment exists ---
        const char* sql_check = "SELECT appointment_id FROM Appointment WHERE appointment_id = ? AND patient_id = ?";
        if (sqlite3_prepare_v2(db, sql_check, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed (check): " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, appointment_id);
        sqlite3_bind_int(stmt, 2, patient_id);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_ROW) {
            std::cerr << "Appointment not found" << std::endl;
            return false;
        }

        // --- Step 2: Delete appointment ---
        const char* sql_delete_appointment = "DELETE FROM Appointment WHERE appointment_id = ? AND patient_id = ?";
        if (sqlite3_prepare_v2(db, sql_delete_appointment, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed (delete appointment): " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, appointment_id);
        sqlite3_bind_int(stmt, 2, patient_id);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to delete appointment: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        // --- Step 3: Delete patient ---
        const char* sql_delete_patient = "DELETE FROM Patient WHERE patient_id = ?";
        if (sqlite3_prepare_v2(db, sql_delete_patient, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed (delete patient): " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, patient_id);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to delete patient: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        return true;
    }
};
