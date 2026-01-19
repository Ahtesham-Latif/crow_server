#pragma once
// Crow include for web framework functionalities
#include <crow.h>
#include <sqlite3.h>
// Function to register category-related routes
void registerCategoryRoutes(crow::SimpleApp& app, sqlite3* db);
