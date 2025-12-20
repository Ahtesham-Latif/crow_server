#pragma once
#include <crow.h>
#include <sqlite3.h>

void registerAppointmentRoutes(crow::SimpleApp& app, sqlite3* db);
