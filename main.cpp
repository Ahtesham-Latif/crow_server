#include <crow.h>
#include <sqlite3.h>
#include <iostream>
using namespace std;

// Controllers
#include "controllers/category_controller.h"
#include "controllers/doctor_controller.h"
#include "controllers/schedule_controller.h"
#include "controllers/appointment_controller.h"
#include "controllers/cancellation_controller.h"
#include "controllers/page_controller.h"

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
    // API routes (MVC controllers)
    // -------------------------------------------------
    registerPageRoutes(app);
    registerCategoryRoutes(app, db);
    registerDoctorRoutes(app, db);
    registerScheduleRoutes(app, db);
    registerAppointmentRoutes(app, db);
    registerCancellationRoutes(app, db);

    // -------------------------------------------------
    // Start the server
    // -------------------------------------------------
    cout << "Server running at https://localhost:8443\n";
    app.port(8443).ssl_file("D:/APPOINTMNENT BOOKING SYSTEM/crow_backend/cert.pem","D:/APPOINTMNENT BOOKING SYSTEM/crow_backend/key.pem")
    .multithreaded()
    .run();



    // Close DB connection
    sqlite3_close(db);
    return 0;
}
