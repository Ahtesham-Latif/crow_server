#include "admin_controller.h"
#include "../services/public_session.h"
#include <iostream>
#include <ctime>

void registerAdminRoutes(crow::SimpleApp& app, sqlite3* db)
{
    // --------------------------------------------------
    // POST: Block all slots for all doctors on a date
    // (Admin only - for holidays, Eid, national holidays, etc)
    // --------------------------------------------------
    CROW_ROUTE(app, "/admin/block_all_slots").methods("POST"_method)
    ([db](const crow::request& req)
    {
        // Check admin session (you can enhance this with proper admin authentication)
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("blocked_date") || !body.has("reason")) {
            return crow::response(400, "Please provide blocked_date and reason.");
        }

        std::string blocked_date = body["blocked_date"].s();
        std::string reason = body["reason"].s();

        if (blocked_date.empty() || reason.empty()) {
            return crow::response(400, "Date and reason cannot be empty.");
        }

        // Validate date format (YYYY-MM-DD)
        if (blocked_date.length() != 10 || blocked_date[4] != '-' || blocked_date[7] != '-') {
            return crow::response(400, "Invalid date format. Please use YYYY-MM-DD.");
        }

        // Block all available slots for all doctors on this date
        // Insert into Doctor_Blocked_Slots for all doctors and all time slots
        const char* block_all_sql =
            "INSERT OR IGNORE INTO Doctor_Blocked_Slots (doctor_id, schedule_id, appointment_date) "
            "SELECT DISTINCT d.doctor_id, ds.schedule_id, ? "
            "FROM Doctor d "
            "CROSS JOIN Doctor_Schedule ds;";

        sqlite3_stmt* block_stmt = nullptr;
        if (sqlite3_prepare_v2(db, block_all_sql, -1, &block_stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return crow::response(500, "Sorry, we couldn't block the slots. Please try again.");
        }

        sqlite3_bind_text(block_stmt, 1, blocked_date.c_str(), -1, SQLITE_TRANSIENT);

        bool blocked = sqlite3_step(block_stmt) == SQLITE_DONE;
        int blocked_count = sqlite3_changes(db);
        sqlite3_finalize(block_stmt);

        if (!blocked) {
            return crow::response(500, "Sorry, we couldn't block all slots. Please try again.");
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "All slots blocked successfully for the date.";
        res["blocked_date"] = blocked_date;
        res["reason"] = reason;
        res["slots_blocked"] = blocked_count;
        return crow::response(200, res);
    });

    // --------------------------------------------------
    // POST: Unblock all slots for a specific date
    // (Admin only)
    // --------------------------------------------------
    CROW_ROUTE(app, "/admin/unblock_all_slots").methods("POST"_method)
    ([db](const crow::request& req)
    {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("blocked_date")) {
            return crow::response(400, "Please provide blocked_date.");
        }

        std::string blocked_date = body["blocked_date"].s();

        if (blocked_date.empty()) {
            return crow::response(400, "Date cannot be empty.");
        }

        // Validate date format (YYYY-MM-DD)
        if (blocked_date.length() != 10 || blocked_date[4] != '-' || blocked_date[7] != '-') {
            return crow::response(400, "Invalid date format. Please use YYYY-MM-DD.");
        }

        // Delete all blocked slots for this date
        const char* delete_sql =
            "DELETE FROM Doctor_Blocked_Slots WHERE appointment_date = ?;";

        sqlite3_stmt* delete_stmt = nullptr;
        if (sqlite3_prepare_v2(db, delete_sql, -1, &delete_stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return crow::response(500, "Sorry, we couldn't unblock the slots. Please try again.");
        }

        sqlite3_bind_text(delete_stmt, 1, blocked_date.c_str(), -1, SQLITE_TRANSIENT);

        bool deleted = sqlite3_step(delete_stmt) == SQLITE_DONE;
        int unblocked_count = sqlite3_changes(db);
        sqlite3_finalize(delete_stmt);

        if (!deleted) {
            return crow::response(500, "Sorry, we couldn't unblock the slots. Please try again.");
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "All slots unblocked successfully for the date.";
        res["blocked_date"] = blocked_date;
        res["slots_unblocked"] = unblocked_count;
        return crow::response(200, res);
    });

    // --------------------------------------------------
    // GET: List all distinct blocked dates
    // --------------------------------------------------
    CROW_ROUTE(app, "/get_blocked_dates").methods("GET"_method)
    ([db](const crow::request& req)
    {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        const char* sql =
            "SELECT DISTINCT appointment_date FROM Doctor_Blocked_Slots "
            "WHERE appointment_date >= date('now', 'localtime') "
            "ORDER BY appointment_date DESC;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return crow::response(500, "Sorry, we couldn't load blocked dates. Please try again.");
        }

        crow::json::wvalue result;
        int idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result[idx]["blocked_date"] =
                std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
            result[idx]["reason"] = "System-blocked date";
            idx++;
        }
        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["success"] = true;
        res["count"] = idx;
        res["blocked_dates"] = std::move(result);
        return crow::response(200, res);
    });
}
