#pragma once

#include <crow.h>
#include <sqlite3.h>

void registerCategoryRoutes(crow::SimpleApp& app, sqlite3* db);
