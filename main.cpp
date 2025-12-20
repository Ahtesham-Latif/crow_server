#include <crow.h>
#include <sqlite3.h>
#include <iostream>
#include <fstream>
#include <sstream>

// Controllers
#include "controllers/category_controller.h"
#include "controllers/doctor_controller.h"
#include "controllers/schedule_controller.h"
#include "controllers/appointment_controller.h"
#include "controllers/cancellation_controller.h"

// -------------------------------------------------
// Helper: Serve static HTML files
// -------------------------------------------------
crow::response serveFile(const std::string& path) {
    std::ifstream file(path);
    crow::response res;

    if (!file.is_open()) {
        res.code = 404;
        res.write("File not found!");
        return res;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    res.set_header("Content-Type", "text/html");
    res.write(buffer.str());
    return res;
}

int main() {
    crow::SimpleApp app;

    // Optional: Mustache templates
    crow::mustache::set_base("../views");

    // -------------------------------------------------
    // Open SQLite database
    // -------------------------------------------------
    sqlite3* db = nullptr;
    if (sqlite3_open("../db/Marta_K Database.db", &db) != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    // -------------------------------------------------
    // Serve static HTML pages
    // -------------------------------------------------
    CROW_ROUTE(app, "/categories_page")([]() { return serveFile("../public/category.html"); });
    CROW_ROUTE(app, "/doctors_page")([]() { return serveFile("../public/doctor.html"); });
    CROW_ROUTE(app, "/schedule_page")([]() { return serveFile("../public/schedule.html"); });
    CROW_ROUTE(app, "/appointment_page")([]() { return serveFile("../public/appointment.html"); });
    CROW_ROUTE(app, "/confirmation_page")([]() { return serveFile("../public/confirmation.html"); });
    
    // ✅ Match your actual file name here
    CROW_ROUTE(app, "/cancel_appointment_page")([]() { 
        return serveFile("../public/cancellation.html"); 
    });

    // -------------------------------------------------
    // API routes (MVC controllers)
    // -------------------------------------------------
    registerCategoryRoutes(app, db);
    registerDoctorRoutes(app, db);
    registerScheduleRoutes(app, db);
    registerAppointmentRoutes(app, db);
    registerCancellationRoutes(app, db);

    // -------------------------------------------------
    // Start the server
    // -------------------------------------------------
    std::cout << "Server running at http://localhost:8080\n";
    app.port(8080).multithreaded().run();

    // Close DB connection
    sqlite3_close(db);
    return 0;
}
