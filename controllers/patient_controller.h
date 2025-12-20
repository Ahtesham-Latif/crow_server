#pragma once

#include <crow.h>
#include <sqlite3.h>

void registerPatientRoutes(crow::SimpleApp& app, sqlite3* db);
