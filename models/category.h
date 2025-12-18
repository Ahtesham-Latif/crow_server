#pragma once

#include <sqlite3.h>
#include <string>
#include <iostream>
#include <vector>
#include <map>

class Category {
public:
    int category_id;
    std::string category_name;

    Category() = default;
    Category(int id, const std::string& name) : category_id(id), category_name(name) {}

    // Insert a new category into DB
    static bool insert(sqlite3* db, const std::string& name) {
        const char* sql = "INSERT INTO Category (category_name) VALUES (?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            std::cerr << "Insert failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        return true;
    }

    // Fetch all categories from DB
    static std::vector<Category> fetchAll(sqlite3* db) {
        std::vector<Category> categories;
        const char* sql = "SELECT category_id, category_name FROM Category;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return categories;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            std::string name = (const char*)sqlite3_column_text(stmt, 1);
            categories.emplace_back(id, name);
        }

        sqlite3_finalize(stmt);
        return categories;
    }
};
