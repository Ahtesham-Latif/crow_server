#pragma once

#include "crow.h"
#include "../models/category.h"
#include <sqlite3.h>
#include <iostream>

void registerCategoryRoutes(crow::SimpleApp& app, sqlite3* db) {

    // -----------------------------
    // GET all categories
    // -----------------------------
    CROW_ROUTE(app, "/get_categories").methods("GET"_method)
    ([db]() {
        sqlite3_stmt* stmt;
        const char* sql = "SELECT category_id, category_name FROM Category";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return crow::response(500, "Database error");
        }

        crow::json::wvalue result;
        int i = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result[i]["category_id"] = sqlite3_column_int(stmt, 0);
            result[i]["category_name"] = (const char*)sqlite3_column_text(stmt, 1);
            i++;
        }

        sqlite3_finalize(stmt);
        return crow::response(200, result);
    });

    // -----------------------------
    // POST new category
    // -----------------------------
    CROW_ROUTE(app, "/add_category").methods("POST"_method)
    ([db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("category_name")) {
            return crow::response(400, "Missing category_name");
        }

        std::string name = body["category_name"].s();
        bool inserted = Category::insert(db, name);

        crow::json::wvalue response;
        if (inserted) {
            response["success"] = true;
            response["message"] = "Category added successfully!";
        } else {
            response["success"] = false;
            response["message"] = "Failed to insert category.";
        }

        return crow::response(200, response);
    });
}
