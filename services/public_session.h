#pragma once

#include <crow.h>

// Issues a short-lived public session cookie for browsing.
void issuePublicSession(crow::response& res);

// Validates public session from Authorization Bearer or Cookie.
bool publicSessionValid(const crow::request& req);
