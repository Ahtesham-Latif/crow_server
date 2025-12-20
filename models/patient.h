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
    std::string created_at;

    Patient() = default;
    Patient(int id, const std::string& n, int a, const std::string& e, const std::string& g, const std::string& c)
        : patient_id(id), name(n), age(a), email(e), gender(g), created_at(c) {}

    // Insert patient into DB and return patient_id
    static int insert(sqlite3* db, const std::string& name, int age, const std::string& email, const std::string& gender) {
        const char* sql = "INSERT INTO Patient(name, age, email, gender, created_at) VALUES (?, ?, ?, ?, datetime('now', 'localtime'));";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return -1;
        }

        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, age);
        sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, gender.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Insert failed: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return -1;
        }

        int patient_id = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        return patient_id;
    }
};
