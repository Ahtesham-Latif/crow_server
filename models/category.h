#pragma once
// header sqlite3.h used for database operations
#include <sqlite3.h>
#include <string>
#include <iostream>
// we can use vector and map from STL
#include <vector>
#include <map>

using namespace std;

class Category {
public:
    int category_id;
    string category_name;
    string description;

    Category() = default;
    Category(int id, const string& name, const string& description)
        : category_id(id), category_name(name), description(description) {}

    // Insert a new category into DB
    //static method because it does not depend on instance/object
    static bool insert(sqlite3* db, const string& name, const string& description) {
        const char* sql = "INSERT INTO Category (category_name, description) VALUES (?, ?);";
        sqlite3_stmt* stmt;
    // prepare the SQL statement
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
            return false;
        }
    // bind the category name parameter preventing SQL injection
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    // bind the category description parameter preventing SQL injection
        sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
    // execute the statement
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        // check if insertion was successful
        if (rc != SQLITE_DONE) {
            cerr << "Insert failed: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        return true;
    }

    // Fetch all categories from DB
    static vector<Category> fetchAll(sqlite3* db) {
        vector<Category> categories;
        const char* sql = "SELECT category_id, category_name, description FROM Category;";
        sqlite3_stmt* stmt;
        // prepare the SQL statement
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
            return categories;
        }
        // execute the statement and iterate through the results
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            string name = (const char*)sqlite3_column_text(stmt, 1);
            const char* desc = (const char*)sqlite3_column_text(stmt, 2);
            string description = desc ? desc : "";
            categories.emplace_back(id, name, description);
        }
        // clean up
        sqlite3_finalize(stmt);
        return categories;
    }
};
