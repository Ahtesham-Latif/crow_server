#pragma once
#include <crow.h>
#include <sqlite3.h>

void registerCancellationRoutes(crow::SimpleApp& app, sqlite3* db);
