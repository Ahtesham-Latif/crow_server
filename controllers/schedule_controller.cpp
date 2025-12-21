#include "schedule_controller.h"

#include "../models/schedule.h"
#include <iostream>
#include <fstream>
#include <sstream>

void registerScheduleRoutes(crow::SimpleApp& app, sqlite3* db)
{
    // --------------------------------------------------
    // GET: Available slots for a doctor on a given date
    // --------------------------------------------------
    CROW_ROUTE(app, "/get_available_slots/<int>/<string>").methods("GET"_method)
    ([db](int doctor_id, const std::string& appointment_date)
    {
        sqlite3_stmt* stmt = nullptr;

        const char* sql =
            "SELECT ds.schedule_id, ds.time_slot "
            "FROM Doctor_Schedule ds "
            "WHERE ds.schedule_id NOT IN ( "
            "    SELECT a.schedule_id "
            "    FROM Appointment a "
            "    WHERE a.doctor_id = ? "
            "    AND a.appointment_date = ? "
            ") "
            "ORDER BY ds.time_slot;";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return crow::response(500, "Database error");
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_text(stmt, 2, appointment_date.c_str(), -1, SQLITE_STATIC);

        crow::json::wvalue result;
        int index = 0;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result[index]["schedule_id"] = sqlite3_column_int(stmt, 0);
            result[index]["time_slot"] =
                std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
            index++;
        }

        sqlite3_finalize(stmt);
        return crow::response(200, result);
    });

    // --------------------------------------------------
    // GET: Appointment page
    // --------------------------------------------------
    CROW_ROUTE(app, "/appointment_page/<int>/<string>/<string>/<string>/<string>")
    ([db](int doctor_id,
          const std::string& category_name,
          const std::string& doctor_name,
          const std::string& date,
          const std::string& slot_time)
    {
        std::ifstream file("../public/appointment.html");
        if (!file.is_open()) {
            return crow::response(404, "Appointment page not found");
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string html = buffer.str();

        auto replace = [&](const std::string& key, const std::string& value) {
            size_t pos = html.find(key);
            if (pos != std::string::npos) {
                html.replace(pos, key.length(), value);
            }
        };

        replace("{{CATEGORY_NAME}}", category_name);
        replace("{{DOCTOR_NAME}}", doctor_name);
        replace("{{SLOT_DATE}}", date);
        replace("{{SLOT_TIME}}", slot_time);

        return crow::response(200, html);
    });

    // --------------------------------------------------
    // POST: Add a new slot (dev/admin)
    // --------------------------------------------------
    CROW_ROUTE(app, "/add_slot").methods("POST"_method)
    ([db](const crow::request& req)
    {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("time_slot")) {
            return crow::response(400, "Missing time_slot");
        }

        std::string time_slot = std::string(body["time_slot"].s());

        const char* sql =
            "INSERT INTO Doctor_Schedule (time_slot) VALUES (?)";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }

        sqlite3_bind_text(stmt, 1, time_slot.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return crow::response(500, "Failed to insert slot");
        }

        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "Slot added successfully";
        return crow::response(200, res);
    });
}
