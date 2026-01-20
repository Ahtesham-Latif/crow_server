#pragma once

#include <sqlite3.h>
#include <string>
#include <iostream>
#include <vector>
using namespace std;
class Doctor {
public:
    int doctor_id;
    string doctor_name;
    string experience;  // maps to experience_years in DB
    string degree;      // maps to qualifications in DB
    double rating;           // maps to ratings in DB
    int category_id;

    Doctor() = default;
    Doctor(int id, const std::string& name, const std::string& exp, const std::string& deg, double rate, int cat_id)
        : doctor_id(id), doctor_name(name), experience(exp), degree(deg), rating(rate), category_id(cat_id) {}

    // Insert a doctor into the DB
    static bool insert(sqlite3* db, const std::string& name, const std::string& exp, const std::string& deg, double rate, int cat_id) {
        const char* sql = "INSERT INTO Doctor (name, experience_years, qualifications, ratings, category_id) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, exp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, deg.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, rate);
        sqlite3_bind_int(stmt, 5, cat_id);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            std::cerr << "Insert failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        return true;
    }

    // Fetch doctors by category
    static std::vector<Doctor> fetchByCategory(sqlite3* db, int cat_id) {
        std::vector<Doctor> doctors;
        const char* sql = "SELECT doctor_id, name, experience_years, qualifications, ratings, category_id "
                          "FROM Doctor WHERE category_id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return doctors;
        }

        sqlite3_bind_int(stmt, 1, cat_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            string name = (const char*)sqlite3_column_text(stmt, 1);
            string exp = (const char*)sqlite3_column_text(stmt, 2);
            string deg = (const char*)sqlite3_column_text(stmt, 3);
            double rate = sqlite3_column_double(stmt, 4);
            int category = sqlite3_column_int(stmt, 5);

            doctors.emplace_back(id, name, exp, deg, rate, category);
        }

        sqlite3_finalize(stmt);
        return doctors;
    }
};
