#pragma once

#include <crow.h>
#include <sqlite3.h>

// Register all doctor-related routes
void registerDoctorRoutes(crow::SimpleApp& app, sqlite3* db);
