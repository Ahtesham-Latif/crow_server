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
    // 1. Fetch patient details and verify identity
    static bool getPatientInfoForCancellation(sqlite3* db, int patient_id, const std::string& name, const std::string& email, int age, PatientInfo& info) {
        if (!db || patient_id <= 0) return false;

        sqlite3_stmt* stmt;
        // Verify identity while fetching
        const char* sql = "SELECT patient_id, name, age, email, request FROM Patient WHERE patient_id = ? AND name = ? AND email = ? AND age = ?";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, patient_id);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, age);

        int rc = sqlite3_step(stmt);

        if (rc == SQLITE_ROW) {
            info.patient_id = sqlite3_column_int(stmt, 0);
            info.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            info.age = sqlite3_column_int(stmt, 2);
            info.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            info.request = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            sqlite3_finalize(stmt);
            return true;
        }
        
        sqlite3_finalize(stmt);
        return false;
    }

    // 2. Update status instead of deleting
    static bool cancelAppointment(sqlite3* db, int appointment_id, int patient_id) {
        if (!db || appointment_id <= 0 || patient_id <= 0) return false;

        sqlite3_stmt* stmt;

        // Update Appointment table status
        const char* sql_app = "UPDATE Appointment SET status = 'Cancelled' WHERE appointment_id = ? AND patient_id = ?";
        if (sqlite3_prepare_v2(db, sql_app, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, appointment_id);
        sqlite3_bind_int(stmt, 2, patient_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // Update Patient table request column
        const char* sql_pat = "UPDATE Patient SET request = 'Cancelled the booking' WHERE patient_id = ?";
        if (sqlite3_prepare_v2(db, sql_pat, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, patient_id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return (rc == SQLITE_DONE);
    }
};