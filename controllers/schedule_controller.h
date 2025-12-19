#pragma once
#include <crow.h>
#include <sqlite3.h>

// Register all schedule/appointment routes
void registerScheduleRoutes(crow::SimpleApp& app, sqlite3* db);
