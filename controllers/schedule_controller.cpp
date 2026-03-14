#include "schedule_controller.h"
#include "../models/schedule.h"
#include "../services/public_session.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <random>

namespace {

struct DoctorSession {
    int doctor_id;
    std::chrono::system_clock::time_point expires_at;
};

struct ScheduleContext {
    int doctor_id;
    std::string doctor_name;
    std::string category_name;
    std::string experience_years;
    double ratings;
    std::chrono::system_clock::time_point expires_at;
};

std::unordered_map<std::string, DoctorSession> g_doctor_sessions;
std::mutex g_session_mutex;

std::unordered_map<std::string, ScheduleContext> g_schedule_contexts;
std::mutex g_schedule_mutex;

std::string generateSessionToken() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned long long> dis;

    std::ostringstream out;
    out << std::hex << dis(gen) << dis(gen);
    return out.str();
}

std::string generateScheduleToken() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned long long> dis;

    std::ostringstream out;
    out << std::hex << dis(gen) << dis(gen);
    return out.str();
}

void pruneExpiredSessions() {
    const auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(g_session_mutex);

    for (auto it = g_doctor_sessions.begin(); it != g_doctor_sessions.end();) {
        if (it->second.expires_at <= now) {
            it = g_doctor_sessions.erase(it);
        } else {
            ++it;
        }
    }
}

void pruneExpiredScheduleContexts() {
    const auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(g_schedule_mutex);
    for (auto it = g_schedule_contexts.begin(); it != g_schedule_contexts.end();) {
        if (it->second.expires_at <= now) {
            it = g_schedule_contexts.erase(it);
        } else {
            ++it;
        }
    }
}

int doctorIdFromToken(const std::string& token) {
    pruneExpiredSessions();
    std::lock_guard<std::mutex> lock(g_session_mutex);
    auto it = g_doctor_sessions.find(token);
    if (it == g_doctor_sessions.end()) {
        return -1;
    }
    return it->second.doctor_id;
}

std::string getTokenFromRequest(const crow::request& req, crow::json::rvalue body = crow::json::rvalue()) {
    if (body && body.has("token") && body["token"].t() == crow::json::type::String) {
        return std::string(body["token"].s());
    }

    const char* query_token = req.url_params.get("token");
    if (query_token != nullptr) {
        return std::string(query_token);
    }

    auto auth_header = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (auth_header.rfind(prefix, 0) == 0) {
        return auth_header.substr(prefix.size());
    }

    return "";
}

std::string getScheduleTokenFromRequest(const crow::request& req) {
    auto auth_header = req.get_header_value("Authorization");
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
            if (k == "schedule_token") {
                return cookie.substr(eq + 1, end - (eq + 1));
            }
        }

        start = end + 1;
    }

    return "";
}

bool getScheduleContext(const std::string& token, ScheduleContext& out) {
    pruneExpiredScheduleContexts();
    std::lock_guard<std::mutex> lock(g_schedule_mutex);
    auto it = g_schedule_contexts.find(token);
    if (it == g_schedule_contexts.end()) {
        return false;
    }
    out = it->second;
    return true;
}

} // namespace

void registerScheduleRoutes(crow::SimpleApp& app, sqlite3* db)
{
    // Keep doctor blocking data separate from Appointment to avoid
    // schema conflicts (Appointment has a unique constraint on doctor+slot).
    const char* create_blocked_slots_sql =
        "CREATE TABLE IF NOT EXISTS Doctor_Blocked_Slots ("
        "  doctor_id INTEGER NOT NULL,"
        "  schedule_id INTEGER NOT NULL,"
        "  appointment_date TEXT NOT NULL,"
        "  created_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),"
        "  PRIMARY KEY (doctor_id, schedule_id, appointment_date),"
        "  FOREIGN KEY (doctor_id) REFERENCES Doctor(doctor_id) ON DELETE CASCADE,"
        "  FOREIGN KEY (schedule_id) REFERENCES Doctor_Schedule(schedule_id) ON DELETE CASCADE"
        ");";
    char* err_msg = nullptr;
    if (sqlite3_exec(db, create_blocked_slots_sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::cerr << "[ERROR] Failed to ensure Doctor_Blocked_Slots table: "
                  << (err_msg ? err_msg : "unknown") << std::endl;
        sqlite3_free(err_msg);
    }


    // --------------------------------------------------
    // GET: Available slots for a doctor on a given date
    // Exclude BOOKED or BLOCKED
    // --------------------------------------------------
    CROW_ROUTE(app, "/get_available_slots/<int>/<string>").methods("GET"_method)
    ([db](const crow::request& req, int doctor_id, const std::string& appointment_date)
    {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }
        sqlite3_stmt* stmt = nullptr;

        const char* sql =
            "SELECT ds.schedule_id, ds.time_slot "
            "FROM Doctor_Schedule ds "
            "WHERE NOT EXISTS ( "
            "    SELECT 1 "
            "    FROM Appointment a "
            "    WHERE a.doctor_id = ? "
            "      AND a.appointment_date = ? "
            "      AND a.schedule_id = ds.schedule_id "
            "      AND a.status = 'BOOKED' "
            ") "
            "AND NOT EXISTS ( "
            "    SELECT 1 "
            "    FROM Doctor_Blocked_Slots b "
            "    WHERE b.doctor_id = ? "
            "      AND b.appointment_date = ? "
            "      AND b.schedule_id = ds.schedule_id "
            ") "
            "ORDER BY ds.time_slot;";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return crow::response(500, "Sorry, we couldn't load the slots right now. Please try again.");
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_text(stmt, 2, appointment_date.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, doctor_id);
        sqlite3_bind_text(stmt, 4, appointment_date.c_str(), -1, SQLITE_STATIC);

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
    // GET: All slots for a doctor on a given date with status
    // --------------------------------------------------
    CROW_ROUTE(app, "/get_slots_status/<int>/<string>").methods("GET"_method)
    ([db](const crow::request& req, int doctor_id, const std::string& appointment_date)
    {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }
        const char* sql =
            "SELECT ds.schedule_id, ds.time_slot, "
            "       CASE "
            "         WHEN EXISTS ("
            "           SELECT 1 FROM Appointment a "
            "           WHERE a.doctor_id = ? "
            "             AND a.schedule_id = ds.schedule_id "
            "             AND a.appointment_date = ? "
            "             AND a.status = 'BOOKED' "
            "         ) THEN 'BOOKED' "
            "         WHEN EXISTS ("
            "           SELECT 1 FROM Doctor_Blocked_Slots b "
            "           WHERE b.doctor_id = ? "
            "             AND b.schedule_id = ds.schedule_id "
            "             AND b.appointment_date = ? "
            "         ) THEN 'BLOCKED' "
            "         ELSE 'AVAILABLE' "
            "       END AS slot_status "
            "FROM Doctor_Schedule ds "
            "ORDER BY ds.time_slot;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[ERROR] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return crow::response(500, "Sorry, we couldn't load the slots right now. Please try again.");
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_text(stmt, 2, appointment_date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, doctor_id);
        sqlite3_bind_text(stmt, 4, appointment_date.c_str(), -1, SQLITE_TRANSIENT);

        crow::json::wvalue result;
        int index = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result[index]["schedule_id"] = sqlite3_column_int(stmt, 0);
            result[index]["time_slot"] =
                std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
            result[index]["status"] =
                std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
            index++;
        }
        sqlite3_finalize(stmt);

        return crow::response(200, result);
    });

    // --------------------------------------------------
    // POST: Create schedule context (public flow)
    // --------------------------------------------------
    CROW_ROUTE(app, "/schedule_context").methods("POST"_method)
    ([db](const crow::request& req)
    {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("doctor_id")) {
            return crow::response(400, "Please provide doctor_id.");
        }

        const int doctor_id = body["doctor_id"].i();
        if (doctor_id <= 0) {
            return crow::response(400, "Please provide a valid doctor_id.");
        }

        sqlite3_stmt* stmt = nullptr;
        const char* sql_doctor =
            "SELECT doctor_name, experience_years, ratings, category_id "
            "FROM Doctor WHERE doctor_id = ? LIMIT 1;";
        if (sqlite3_prepare_v2(db, sql_doctor, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't load the doctor right now.");
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        std::string doctor_name;
        std::string experience_years;
        double ratings = 0.0;
        int category_id = -1;

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            doctor_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            experience_years = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            ratings = sqlite3_column_double(stmt, 2);
            category_id = sqlite3_column_int(stmt, 3);
        }
        sqlite3_finalize(stmt);

        if (doctor_name.empty() || category_id <= 0) {
            return crow::response(404, "Doctor not found.");
        }

        std::string category_name;
        const char* sql_category =
            "SELECT category_name FROM Category WHERE category_id = ? LIMIT 1;";
        if (sqlite3_prepare_v2(db, sql_category, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't load the category right now.");
        }
        sqlite3_bind_int(stmt, 1, category_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            category_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);

        const std::string token = generateScheduleToken();
        const auto expires_at = std::chrono::system_clock::now() + std::chrono::minutes(15);
        {
            std::lock_guard<std::mutex> lock(g_schedule_mutex);
            g_schedule_contexts[token] = {doctor_id, doctor_name, category_name, experience_years, ratings, expires_at};
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "Schedule context created.";

        crow::response response(200, res);
        std::ostringstream cookie;
        cookie << "schedule_token=" << token
               << "; Path=/; Max-Age=900; HttpOnly; SameSite=Strict; Secure";
        response.add_header("Set-Cookie", cookie.str());
        return response;
    });

    // --------------------------------------------------
    // GET: Schedule context (public flow)
    // --------------------------------------------------
    CROW_ROUTE(app, "/schedule_context").methods("GET"_method)
    ([db](const crow::request& req)
    {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        const std::string token = getScheduleTokenFromRequest(req);
        if (token.empty()) {
            return crow::response(401, "Missing schedule token.");
        }

        ScheduleContext ctx;
        if (!getScheduleContext(token, ctx)) {
            return crow::response(401, "Invalid or expired schedule token.");
        }

        crow::json::wvalue res;
        res["doctor_id"] = ctx.doctor_id;
        res["doctor_name"] = ctx.doctor_name;
        res["category_name"] = ctx.category_name;
        res["experience_years"] = ctx.experience_years;
        res["ratings"] = ctx.ratings;

        return crow::response(200, res);
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
            return crow::response(404, "Sorry, the appointment page is not available right now.");
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
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }
        auto body = crow::json::load(req.body);
        if (!body || !body.has("time_slot")) {
            return crow::response(400, "Please provide a time slot.");
        }

        std::string time_slot = std::string(body["time_slot"].s());

        const char* sql =
            "INSERT INTO Doctor_Schedule (time_slot) VALUES (?)";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't add the slot right now. Please try again.");
        }

        sqlite3_bind_text(stmt, 1, time_slot.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return crow::response(500, "Sorry, we couldn't add the slot right now. Please try again.");
        }

        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "Slot added successfully.";
        return crow::response(200, res);
    });

    // --------------------------------------------------
    // POST: Block a slot for a doctor (legacy endpoint)
    // --------------------------------------------------
    CROW_ROUTE(app, "/block_slot").methods("POST"_method)
    ([db](const crow::request& req)
    {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }
        auto body = crow::json::load(req.body);
        if (!body || !body.has("doctor_id") || !body.has("schedule_id") || !body.has("appointment_date")) {
            return crow::response(400, "Please provide doctor_id, schedule_id, and appointment_date.");
        }

        int doctor_id = body["doctor_id"].i();
        int schedule_id = body["schedule_id"].i();
        std::string appointment_date = body["appointment_date"].s();

        const char* booked_check_sql =
            "SELECT 1 FROM Appointment "
            "WHERE doctor_id = ? AND schedule_id = ? AND appointment_date = ? AND status = 'BOOKED' "
            "LIMIT 1;";
        sqlite3_stmt* booked_stmt = nullptr;
        if (sqlite3_prepare_v2(db, booked_check_sql, -1, &booked_stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't update the slot right now. Please try again.");
        }
        sqlite3_bind_int(booked_stmt, 1, doctor_id);
        sqlite3_bind_int(booked_stmt, 2, schedule_id);
        sqlite3_bind_text(booked_stmt, 3, appointment_date.c_str(), -1, SQLITE_TRANSIENT);
        bool already_booked = sqlite3_step(booked_stmt) == SQLITE_ROW;
        sqlite3_finalize(booked_stmt);
        if (already_booked) {
            return crow::response(409, "Sorry, that slot is already booked for this date.");
        }

        const char* sql =
            "INSERT OR IGNORE INTO Doctor_Blocked_Slots (doctor_id, schedule_id, appointment_date) "
            "VALUES (?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't update the slot right now. Please try again.");
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_int(stmt, 2, schedule_id);
        sqlite3_bind_text(stmt, 3, appointment_date.c_str(), -1, SQLITE_STATIC);

        bool blocked = sqlite3_step(stmt) == SQLITE_DONE;
        int changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["success"] = blocked && changes > 0;
        res["message"] = (blocked && changes > 0) ? "Slot blocked successfully." : "That slot is already blocked.";

        return crow::response(blocked ? 200 : 409, res);
    });

    // --------------------------------------------------
    // POST: Verify doctor for dashboard
    // Required: doctor_name + phone
    // --------------------------------------------------
    CROW_ROUTE(app, "/doctor_dashboard/verify").methods("POST"_method)
    ([db](const crow::request& req)
    {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("doctor_name") || !body.has("phone")) {
            return crow::response(400, "Please provide both doctor_name and phone.");
        }

        std::string doctor_name = body["doctor_name"].s();
        std::string phone = body["phone"].s();

        const char* sql =
            "SELECT doctor_id, doctor_name "
            "FROM Doctor "
            "WHERE lower(trim(doctor_name)) = lower(trim(?)) "
            "  AND trim(phone) = trim(?) "
            "LIMIT 1;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't verify you right now. Please try again.");
        }

        sqlite3_bind_text(stmt, 1, doctor_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, phone.c_str(), -1, SQLITE_TRANSIENT);

        int doctor_id = -1;
        std::string matched_name;

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            doctor_id = sqlite3_column_int(stmt, 0);
            matched_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        }
        sqlite3_finalize(stmt);

        if (doctor_id <= 0) {
            return crow::response(401, "Sorry, we could not verify those details. Please check and try again.");
        }

        const auto expires_at = std::chrono::system_clock::now() + std::chrono::hours(12);
        const std::string token = generateSessionToken();

        {
            std::lock_guard<std::mutex> lock(g_session_mutex);
            g_doctor_sessions[token] = {doctor_id, expires_at};
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["token"] = token;
        res["doctor_id"] = doctor_id;
        res["doctor_name"] = matched_name;
        res["message"] = "Verification successful.";

        return crow::response(200, res);
    });

    // --------------------------------------------------
    // GET: Doctor dashboard slots (doctor can only view own)
    // --------------------------------------------------
    CROW_ROUTE(app, "/doctor_dashboard/slots/<string>").methods("GET"_method)
    ([db](const crow::request& req, const std::string& appointment_date)
    {
        const std::string token = getTokenFromRequest(req);
        const int doctor_id = doctorIdFromToken(token);

        if (doctor_id <= 0) {
            return crow::response(401, "Please verify your session and try again.");
        }

        const char* sql =
            "SELECT ds.schedule_id, ds.time_slot, "
            "       CASE "
            "         WHEN EXISTS ("
            "           SELECT 1 FROM Appointment a "
            "           WHERE a.doctor_id = ? "
            "             AND a.schedule_id = ds.schedule_id "
            "             AND a.appointment_date = ? "
            "             AND a.status = 'BOOKED'"
            "         ) THEN 'BOOKED' "
            "         WHEN EXISTS ("
            "           SELECT 1 FROM Doctor_Blocked_Slots b "
            "           WHERE b.doctor_id = ? "
            "             AND b.schedule_id = ds.schedule_id "
            "             AND b.appointment_date = ? "
            "         ) THEN 'BLOCKED' "
            "         ELSE 'AVAILABLE' "
            "       END AS slot_status "
            "FROM Doctor_Schedule ds "
            "ORDER BY ds.time_slot;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't load the slots right now. Please try again.");
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_text(stmt, 2, appointment_date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, doctor_id);
        sqlite3_bind_text(stmt, 4, appointment_date.c_str(), -1, SQLITE_TRANSIENT);

        crow::json::wvalue result;
        int idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result[idx]["schedule_id"] = sqlite3_column_int(stmt, 0);
            result[idx]["time_slot"] =
                std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
            result[idx]["status"] =
                std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
            idx++;
        }
        sqlite3_finalize(stmt);

        return crow::response(200, result);
    });

    // --------------------------------------------------
    // POST: Block slot from doctor dashboard (own slots only)
    // --------------------------------------------------
    CROW_ROUTE(app, "/doctor_dashboard/block_slot").methods("POST"_method)
    ([db](const crow::request& req)
    {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("schedule_id") || !body.has("appointment_date")) {
            return crow::response(400, "Please provide schedule_id and appointment_date.");
        }

        const std::string token = getTokenFromRequest(req, body);
        const int doctor_id = doctorIdFromToken(token);

        if (doctor_id <= 0) {
            return crow::response(401, "Please verify your session and try again.");
        }

        int schedule_id = body["schedule_id"].i();
        std::string appointment_date = body["appointment_date"].s();

        const char* booked_check_sql =
            "SELECT 1 FROM Appointment "
            "WHERE doctor_id = ? AND schedule_id = ? AND appointment_date = ? AND status = 'BOOKED' "
            "LIMIT 1;";
        sqlite3_stmt* booked_stmt = nullptr;
        if (sqlite3_prepare_v2(db, booked_check_sql, -1, &booked_stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't update the slot right now. Please try again.");
        }
        sqlite3_bind_int(booked_stmt, 1, doctor_id);
        sqlite3_bind_int(booked_stmt, 2, schedule_id);
        sqlite3_bind_text(booked_stmt, 3, appointment_date.c_str(), -1, SQLITE_TRANSIENT);
        bool already_booked = sqlite3_step(booked_stmt) == SQLITE_ROW;
        sqlite3_finalize(booked_stmt);
        if (already_booked) {
            return crow::response(409, "Sorry, that slot is already booked for this date.");
        }

        const char* sql =
            "INSERT OR IGNORE INTO Doctor_Blocked_Slots (doctor_id, schedule_id, appointment_date) "
            "VALUES (?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't update the slot right now. Please try again.");
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_int(stmt, 2, schedule_id);
        sqlite3_bind_text(stmt, 3, appointment_date.c_str(), -1, SQLITE_TRANSIENT);

        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        int changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);

        if (!ok) {
            return crow::response(409, "Sorry, that slot cannot be blocked right now.");
        }

        crow::json::wvalue res;
        res["success"] = changes > 0;
        res["message"] = (changes > 0) ? "Slot blocked." : "That slot is already blocked.";
        return crow::response(200, res);
    });

    // --------------------------------------------------
    // POST: Unblock slot from doctor dashboard (own slots only)
    // --------------------------------------------------
    CROW_ROUTE(app, "/doctor_dashboard/unblock_slot").methods("POST"_method)
    ([db](const crow::request& req)
    {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("schedule_id") || !body.has("appointment_date")) {
            return crow::response(400, "Please provide schedule_id and appointment_date.");
        }

        const std::string token = getTokenFromRequest(req, body);
        const int doctor_id = doctorIdFromToken(token);

        if (doctor_id <= 0) {
            return crow::response(401, "Please verify your session and try again.");
        }

        int schedule_id = body["schedule_id"].i();
        std::string appointment_date = body["appointment_date"].s();

        const char* sql =
            "DELETE FROM Doctor_Blocked_Slots "
            "WHERE doctor_id = ? "
            "  AND schedule_id = ? "
            "  AND appointment_date = ?;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't update the slot right now. Please try again.");
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_int(stmt, 2, schedule_id);
        sqlite3_bind_text(stmt, 3, appointment_date.c_str(), -1, SQLITE_TRANSIENT);

        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        int changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["success"] = ok && changes > 0;
        res["message"] = (ok && changes > 0) ? "Slot unblocked." : "No blocked slot was found.";
        return crow::response((ok && changes > 0) ? 200 : 404, res);
    });
}
