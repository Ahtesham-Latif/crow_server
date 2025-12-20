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
    std::string request;

    // Insert patient with pre-generated random ID
    static bool insert(
        sqlite3* db,
        int patient_id,
        const std::string& name,
        int age,
        const std::string& email,
        const std::string& gender,
        const std::string& request
    ) {
        // --- Validate required fields ---
        if (name.empty() || email.empty() || gender.empty()) {
            std::cerr << "[ERROR] Insert failed: required fields are empty\n";
            return false;
        }

        // --- Check if patient ID already exists ---
        if (exists(db, patient_id)) {
            std::cerr << "[ERROR] Insert failed: patient_id " << patient_id << " already exists\n";
            return false;
        }

        // --- Prepare SQL insert ---
        const char* sql =
            "INSERT INTO Patient "
            "(patient_id, name, age, email, gender, request) "
            "VALUES (?, ?, ?, ?, ?, ?)";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Prepare failed: " << sqlite3_errmsg(db) << "\n";
            return false;
        }

        sqlite3_bind_int(stmt, 1, patient_id);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, age);
        sqlite3_bind_text(stmt, 4, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, gender.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, request.c_str(), -1, SQLITE_TRANSIENT);

        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            std::cerr << "[ERROR] Insert failed: " << sqlite3_errmsg(db) << "\n";
        } else {
            std::cout << "[DEBUG] Patient inserted successfully: ID=" << patient_id << "\n";
        }

        sqlite3_finalize(stmt);
        return ok;
    }

    // Check if patient ID exists
    static bool exists(sqlite3* db, int patient_id) {
        const char* sql = "SELECT 1 FROM Patient WHERE patient_id = ?";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Exists check prepare failed: " << sqlite3_errmsg(db) << "\n";
            return false;
        }

        sqlite3_bind_int(stmt, 1, patient_id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return rc == SQLITE_ROW;
    }
};
