#include <crow.h>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <random>
#include "../models/category.h"
#include "../services/public_session.h"
#include "category_controller.h"

namespace {

struct CategoryContext {
    int category_id;
    std::string category_name;
    std::chrono::system_clock::time_point expires_at;
};

std::unordered_map<std::string, CategoryContext> g_category_contexts;
std::mutex g_category_mutex;

std::string generateCategoryToken() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned long long> dis;

    std::ostringstream out;
    out << std::hex << dis(gen) << dis(gen);
    return out.str();
}

void pruneCategoryContexts() {
    const auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(g_category_mutex);
    for (auto it = g_category_contexts.begin(); it != g_category_contexts.end();) {
        if (it->second.expires_at <= now) {
            it = g_category_contexts.erase(it);
        } else {
            ++it;
        }
    }
}

std::string getCategoryTokenFromRequest(const crow::request& req) {
    const std::string auth_header = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (auth_header.rfind(prefix, 0) == 0) {
        return auth_header.substr(prefix.size());
    }

    const std::string cookie = req.get_header_value("Cookie");
    if (cookie.empty()) {
        return "";
    }

    size_t start = 0;
    while (start < cookie.size()) {
        size_t end = cookie.find(';', start);
        if (end == std::string::npos) {
            end = cookie.size();
        }

        size_t eq = cookie.find('=', start);
        if (eq != std::string::npos && eq < end) {
            std::string k = cookie.substr(start, eq - start);
            while (!k.empty() && k.front() == ' ') {
                k.erase(k.begin());
            }
            if (k == "category_token") {
                return cookie.substr(eq + 1, end - (eq + 1));
            }
        }

        start = end + 1;
    }

    return "";
}

bool getCategoryContext(const std::string& token, CategoryContext& out) {
    pruneCategoryContexts();
    std::lock_guard<std::mutex> lock(g_category_mutex);
    auto it = g_category_contexts.find(token);
    if (it == g_category_contexts.end()) {
        return false;
    }
    out = it->second;
    return true;
}

} // namespace

using namespace std;

void registerCategoryRoutes(crow::SimpleApp& app, sqlite3* db) {

    // POST: Create category context
    CROW_ROUTE(app, "/category_context").methods("POST"_method)
    ([db](const crow::request& req) {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("category_id")) {
            return crow::response(400, "Please provide category_id.");
        }

        const int category_id = body["category_id"].i();
        if (category_id <= 0) {
            return crow::response(400, "Please provide a valid category_id.");
        }

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT category_name FROM Category WHERE category_id = ? LIMIT 1";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't load the category right now.");
        }

        sqlite3_bind_int(stmt, 1, category_id);
        std::string category_name;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            category_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);

        if (category_name.empty()) {
            return crow::response(404, "Category not found.");
        }

        const std::string token = generateCategoryToken();
        const auto expires_at = std::chrono::system_clock::now() + std::chrono::minutes(15);
        {
            std::lock_guard<std::mutex> lock(g_category_mutex);
            g_category_contexts[token] = {category_id, category_name, expires_at};
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "Category context created.";

        crow::response response(200, res);
        std::ostringstream cookie;
        cookie << "category_token=" << token
               << "; Path=/; Max-Age=900; HttpOnly; SameSite=Strict; Secure";
        response.add_header("Set-Cookie", cookie.str());
        return response;
    });

    // GET: Category context
    CROW_ROUTE(app, "/category_context").methods("GET"_method)
    ([db](const crow::request& req) {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        const std::string token = getCategoryTokenFromRequest(req);
        if (token.empty()) {
            return crow::response(401, "Missing category token.");
        }

        CategoryContext ctx;
        if (!getCategoryContext(token, ctx)) {
            return crow::response(401, "Invalid or expired category token.");
        }

        crow::json::wvalue res;
        res["category_id"] = ctx.category_id;
        res["category_name"] = ctx.category_name;
        return crow::response(200, res);
    });

    // GET all categories
    CROW_ROUTE(app, "/get_categories").methods("GET"_method)
([db](const crow::request& req) {
    if (!publicSessionValid(req)) {
        return crow::response(401, "Please refresh and try again.");
    }

    sqlite3_stmt* stmt;
    const char* sql = "SELECT category_id, category_name, description FROM Category";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
        return crow::response(500, "Sorry, we couldn't load the categories right now. Please try again.");
    }

    crow::json::wvalue result;
    int i = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char* name_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* desc_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        std::string name = name_ptr ? name_ptr : "";
        std::string description = desc_ptr ? desc_ptr : "";

        result[i]["category_id"] = id;
        result[i]["category_name"] = name;
        result[i]["description"] = description;
        i++;
    }

    sqlite3_finalize(stmt);
    return crow::response(200, result);
});


    // POST new category
    CROW_ROUTE(app, "/add_category").methods("POST"_method)
([db](const crow::request& req) {
    if (!publicSessionValid(req)) {
        return crow::response(401, "Please refresh and try again.");
    }
    auto body = crow::json::load(req.body);

    if (!body || !body.has("category_name") || !body.has("description")) {
        return crow::response(400, "Please provide a category name and description.");
    }

    string name = body["category_name"].s();
    string description = body["description"].s();

    bool inserted = Category::insert(db, name, description);

    crow::json::wvalue response;
    response["success"] = inserted;
    response["message"] =
        inserted ? "Category added successfully." : "Sorry, we could not add that category right now.";

    return crow::response(200, response);
});
    // DELETE category
    CROW_ROUTE(app, "/delete_category/<int>").methods("DELETE"_method)
([db](const crow::request& req, int category_id) {
    if (!publicSessionValid(req)) {
        return crow::response(401, "Please refresh and try again.");
    }

    if (category_id <= 0) {
        return crow::response(400, "Please provide a valid category_id.");
    }

    bool deleted = Category::remove(db, category_id);

    crow::json::wvalue response;
    response["success"] = deleted;
    response["message"] =
        deleted ? "Category deleted successfully."
                : "Sorry, that category was not found or has already been deleted.";

    return crow::response(200, response);
});


}
