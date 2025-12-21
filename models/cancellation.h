#pragma once
#include <sqlite3.h>
#include <iostream>
#include <string>

struct PatientInfo {
    int patient_id;
    std::string name;
    int age;
    std::string email;
    std::string request;
};

class Cancellation {
public:
    // Fetch patient details before deletion
    static bool getPatientInfo(sqlite3* db, int patient_id, PatientInfo& info) {
        if (!db || patient_id <= 0) return false;

        sqlite3_stmt* stmt;
        const char* sql = "SELECT patient_id, name, age, email, request FROM Patient WHERE patient_id = ?";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed (fetch patient): " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, patient_id);
        int rc = sqlite3_step(stmt);

        if (rc == SQLITE_ROW) {
            info.patient_id = sqlite3_column_int(stmt, 0);
            info.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            info.age = sqlite3_column_int(stmt, 2);
            info.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            info.request = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            sqlite3_finalize(stmt);
            return true;
        } else {
            std::cerr << "Patient not found" << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
    }

    // Cancel appointment and patient
    static bool cancelAppointment(sqlite3* db, int appointment_id, int patient_id) {
        if (!db || appointment_id <= 0 || patient_id <= 0) return false;

        sqlite3_stmt* stmt;

        // --- Check appointment exists ---
        const char* sql_check = "SELECT appointment_id FROM Appointment WHERE appointment_id = ? AND patient_id = ?";
        if (sqlite3_prepare_v2(db, sql_check, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, appointment_id);
        sqlite3_bind_int(stmt, 2, patient_id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_ROW) return false;

        // --- Delete appointment ---
        const char* sql_delete_appointment = "DELETE FROM Appointment WHERE appointment_id = ? AND patient_id = ?";
        if (sqlite3_prepare_v2(db, sql_delete_appointment, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, appointment_id);
        sqlite3_bind_int(stmt, 2, patient_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) return false;

        // --- Delete patient ---
        const char* sql_delete_patient = "DELETE FROM Patient WHERE patient_id = ?";
        if (sqlite3_prepare_v2(db, sql_delete_patient, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, patient_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) return false;

        return true;
    }
};
