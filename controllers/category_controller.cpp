#include <crow.h>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <sstream>
#include "../models/category.h"
#include "category_controller.h"

void registerCategoryRoutes(crow::SimpleApp& app, sqlite3* db) {

    // GET all categories
    CROW_ROUTE(app, "/get_categories").methods("GET"_method)
    ([db]() {
        sqlite3_stmt* stmt;
        const char* sql = "SELECT category_id, category_name FROM Category";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Database error");
        }

        crow::json::wvalue result;
        int i = 0;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result[i]["category_id"] = sqlite3_column_int(stmt, 0);
            result[i]["category_name"] =
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            i++;
        }

        sqlite3_finalize(stmt);
        return crow::response(200, result);
    });

    // POST new category
    CROW_ROUTE(app, "/add_category").methods("POST"_method)
    ([db](const crow::request& req) {
        auto body = crow::json::load(req.body);

        if (!body || !body.has("category_name")) {
            return crow::response(400, "Missing category_name");
        }

        std::string name = body["category_name"].s();
        bool inserted = Category::insert(db, name);

        crow::json::wvalue response;
        response["success"] = inserted;
        response["message"] =
            inserted ? "Category added successfully!" : "Failed to insert category.";

        return crow::response(200, response);
    });
}
