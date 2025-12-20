#pragma once
#include <sqlite3.h>
#include <string>
#include <iostream>

class Patient {
public:
    int patient_id;
    std::string name;
    int age;
    std::string email;
    std::string gender;

    // Insert patient with pre-generated ID
    static bool insert(sqlite3* db, int patient_id, const std::string& name, int age, const std::string& email, const std::string& gender) {
        const char* sql = "INSERT INTO Patient(patient_id, patient_name, patient_age, patient_email, patient_gender) VALUES (?, ?, ?, ?, ?)";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, patient_id);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, age);
        sqlite3_bind_text(stmt, 4, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, gender.c_str(), -1, SQLITE_TRANSIENT);

        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    // Check if patient ID exists
    static bool exists(sqlite3* db, int patient_id) {
        const char* sql = "SELECT 1 FROM Patient WHERE patient_id = ?";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, patient_id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_ROW;
    }
};
