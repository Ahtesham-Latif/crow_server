#include <crow.h>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <sstream>
#include "../models/category.h"
#include "category_controller.h"


using namespace std;

void registerCategoryRoutes(crow::SimpleApp& app, sqlite3* db) {

    // GET all categories
    CROW_ROUTE(app, "/get_categories").methods("GET"_method)
([db]() {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT category_id, category_name, description FROM Category";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
        return crow::response(500, "Database error");
    }

    crow::json::wvalue result;
    int i = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char* name_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* desc_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        std::string name = name_ptr ? name_ptr : "";
        std::string description = desc_ptr ? desc_ptr : "";

        result[i]["category_id"] = id;
        result[i]["category_name"] = name;
        result[i]["description"] = description;
        i++;
    }

    sqlite3_finalize(stmt);
    return crow::response(200, result);
});


    // POST new category
    CROW_ROUTE(app, "/add_category").methods("POST"_method)
([db](const crow::request& req) {
    auto body = crow::json::load(req.body);

    if (!body || !body.has("category_name") || !body.has("description")) {
        return crow::response(400, "Missing category_name or description");
    }

    string name = body["category_name"].s();
    string description = body["description"].s();

    bool inserted = Category::insert(db, name, description);

    crow::json::wvalue response;
    response["success"] = inserted;
    response["message"] =
        inserted ? "Category added successfully!" : "Failed to insert category.";

    return crow::response(200, response);
});

}
