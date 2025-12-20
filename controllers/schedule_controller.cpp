#include "schedule_controller.h"
#include "../models/schedule.h"   // Doctor_Schedule model
#include <iostream>
#include <fstream>

void registerScheduleRoutes(crow::SimpleApp& app, sqlite3* db) {

    // ---------------------------------
    // GET available slots for a doctor on a specific date
    // ---------------------------------
    CROW_ROUTE(app, "/get_available_slots/<int>/<string>").methods("GET"_method)
    ([db](int doctor_id, const std::string& appointmentDate) {

        sqlite3_stmt* stmt;
        const char* sql =
            "SELECT ds.schedule_id, ds.time_slot "
            "FROM Doctor_Schedule ds "
            "WHERE NOT EXISTS ("
            "    SELECT 1 FROM Appointment a "
            "    WHERE a.doctor_id = ? "
            "      AND a.schedule_id = ds.schedule_id "
            "      AND a.appointment_date = ? "
            "      AND a.status = 'BOOKED'"
            ") "
            "ORDER BY ds.time_slot;";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return crow::response(500, "Database error");
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_text(stmt, 2, appointmentDate.c_str(), -1, SQLITE_STATIC);

        crow::json::wvalue result;
        int i = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result[i]["schedule_id"] = sqlite3_column_int(stmt, 0);
            result[i]["time_slot"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            i++;
        }

        sqlite3_finalize(stmt);
        return crow::response(200, result);
    });

    // ---------------------------------
    // Redirect to appointment page with prefilled data
    // ---------------------------------
    CROW_ROUTE(app, "/appointment_page/<int>/<string>/<string>/<string>/<string>")
    ([db](int doctor_id, const std::string& category_name,
          const std::string& doctor_name, const std::string& date,
          const std::string& slot_time) {

        std::ifstream file("../public/appointment.html");
        if (!file.is_open())
            return crow::response(404, "Appointment page not found");

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string html = buffer.str();

        // Replace placeholders
        size_t pos;
        pos = html.find("{{CATEGORY_NAME}}");
        if (pos != std::string::npos) html.replace(pos, 17, category_name);

        pos = html.find("{{DOCTOR_NAME}}");
        if (pos != std::string::npos) html.replace(pos, 14, doctor_name);

        pos = html.find("{{SLOT_DATE}}");
        if (pos != std::string::npos) html.replace(pos, 12, date);

        pos = html.find("{{SLOT_TIME}}");
        if (pos != std::string::npos) html.replace(pos, 12, slot_time);

        return crow::response(html);
    });

    // ---------------------------------
    // POST: Add new slots to Doctor_Schedule
    // ---------------------------------
    CROW_ROUTE(app, "/add_slot").methods("POST"_method)
    ([db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("time_slot")) {
            return crow::response(400, "Missing time_slot");
        }

        std::string slot = body["time_slot"].s();
        const char* sql = "INSERT INTO Doctor_Schedule(time_slot) VALUES(?)";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Database error");
        }

        sqlite3_bind_text(stmt, 1, slot.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return crow::response(500, "Failed to insert slot");
        }

        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "Slot added successfully!";
        return crow::response(200, res);
    });
}
