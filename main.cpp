#include "crow.h"
#include <sqlite3.h>
#include <iostream>
#include "controllers/category_controller.cpp"  // include your controller

int main() {
    crow::SimpleApp app;

    // Set mustache views folder
    crow::mustache::set_base("../views");

    // Open database
    sqlite3* db;
    if (sqlite3_open("../db/Marta_K Database.db", &db) != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    // Serve the HTML page
    CROW_ROUTE(app, "/")([]{
        return crow::response(crow::mustache::load_text("test_insert.html"));
    });

    // Register category routes
    registerCategoryRoutes(app, db);

    std::cout << "Server running at http://localhost:8080\n";
    app.port(8080).multithreaded().run();

    sqlite3_close(db);
    return 0;
}
