#pragma once

#include <crow.h>
#include <sqlite3.h>

// Register all admin-related routes
void registerAdminRoutes(crow::SimpleApp& app, sqlite3* db);
