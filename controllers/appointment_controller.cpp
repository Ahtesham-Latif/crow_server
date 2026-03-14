#include "appointment_controller.h"

#include "../models/patient.h"
#include "../models/doctor.h"
#include "../models/appointment.h"
#include "../config/n8n_config.h"
#include "../services/utils.h"
#include "../services/public_session.h"

#include <iostream>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <random>
#include <sstream>

namespace {

struct ConfirmationSession {
    int appointment_id;
    int patient_id;
    std::chrono::system_clock::time_point expires_at;
};

struct BookingContext {
    int doctor_id;
    std::string doctor_name;
    std::string category_name;
    std::string appointment_date;
    std::string time_slot;
    std::chrono::system_clock::time_point expires_at;
};

std::unordered_map<std::string, ConfirmationSession> g_confirmation_sessions;
std::mutex g_confirmation_mutex;

std::unordered_map<std::string, BookingContext> g_booking_contexts;
std::mutex g_booking_mutex;

std::string generateConfirmationToken() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned long long> dis;

    std::ostringstream out;
    out << std::hex << dis(gen) << dis(gen);
    return out.str();
}

void pruneExpiredConfirmations() {
    const auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(g_confirmation_mutex);
    for (auto it = g_confirmation_sessions.begin(); it != g_confirmation_sessions.end();) {
        if (it->second.expires_at <= now) {
            it = g_confirmation_sessions.erase(it);
        } else {
            ++it;
        }
    }
}

std::string generateBookingToken() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned long long> dis;

    std::ostringstream out;
    out << std::hex << dis(gen) << dis(gen);
    return out.str();
}

void pruneExpiredBookings() {
    const auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(g_booking_mutex);
    for (auto it = g_booking_contexts.begin(); it != g_booking_contexts.end();) {
        if (it->second.expires_at <= now) {
            it = g_booking_contexts.erase(it);
        } else {
            ++it;
        }
    }
}

std::string getCookieValue(const crow::request& req, const std::string& key) {
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
            if (k == key) {
                return cookie.substr(eq + 1, end - (eq + 1));
            }
        }

        start = end + 1;
    }

    return "";
}

std::string getConfirmationTokenFromRequest(const crow::request& req) {
    const std::string auth_header = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (auth_header.rfind(prefix, 0) == 0) {
        return auth_header.substr(prefix.size());
    }

    return getCookieValue(req, "confirmation_token");
}

std::string getBookingTokenFromRequest(const crow::request& req) {
    const std::string auth_header = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (auth_header.rfind(prefix, 0) == 0) {
        return auth_header.substr(prefix.size());
    }

    return getCookieValue(req, "booking_token");
}

bool getConfirmationSession(const std::string& token, ConfirmationSession& out) {
    pruneExpiredConfirmations();
    std::lock_guard<std::mutex> lock(g_confirmation_mutex);
    auto it = g_confirmation_sessions.find(token);
    if (it == g_confirmation_sessions.end()) {
        return false;
    }
    out = it->second;
    return true;
}

bool getBookingContext(const std::string& token, BookingContext& out) {
    pruneExpiredBookings();
    std::lock_guard<std::mutex> lock(g_booking_mutex);
    auto it = g_booking_contexts.find(token);
    if (it == g_booking_contexts.end()) {
        return false;
    }
    out = it->second;
    return true;
}

void deletePatientById(sqlite3* db, int patient_id) {
    if (!db || patient_id <= 0) return;
    const char* sql = "DELETE FROM Patient WHERE patient_id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    sqlite3_bind_int(stmt, 1, patient_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

bool rebookCancelledAppointment(sqlite3* db,
                                int doctor_id,
                                int schedule_id,
                                const std::string& appointment_date,
                                int patient_id,
                                int new_appointment_id)
{
    if (!db) return false;
    const char* sql =
        "UPDATE Appointment "
        "SET appointment_id = ?, patient_id = ?, status = 'BOOKED', created_at = datetime('now','localtime') "
        "WHERE doctor_id = ? AND schedule_id = ? AND appointment_date = ? AND status != 'BOOKED';";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, new_appointment_id);
    sqlite3_bind_int(stmt, 2, patient_id);
    sqlite3_bind_int(stmt, 3, doctor_id);
    sqlite3_bind_int(stmt, 4, schedule_id);
    sqlite3_bind_text(stmt, 5, appointment_date.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(stmt);
    const int changed = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    return changed > 0;
}

} // namespace

static bool isSlotAlreadyBooked(sqlite3* db, int doctor_id, int schedule_id, const std::string& appointment_date)
{
    const char* sql =
        "SELECT 1 FROM Appointment "
        "WHERE doctor_id = ? AND schedule_id = ? AND appointment_date = ? AND status = 'BOOKED' "
        "LIMIT 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        // fail-safe: treat as booked
        return true;
    }

    sqlite3_bind_int(stmt, 1, doctor_id);
    sqlite3_bind_int(stmt, 2, schedule_id);
    sqlite3_bind_text(stmt, 3, appointment_date.c_str(), -1, SQLITE_TRANSIENT);

    bool booked = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return booked;
}

static bool isSlotBlocked(sqlite3* db, int doctor_id, int schedule_id, const std::string& appointment_date)
{
    const char* sql =
        "SELECT 1 FROM Doctor_Blocked_Slots "
        "WHERE doctor_id = ? AND schedule_id = ? AND appointment_date = ? "
        "LIMIT 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        // fail-safe: treat as blocked
        return true;
    }

    sqlite3_bind_int(stmt, 1, doctor_id);
    sqlite3_bind_int(stmt, 2, schedule_id);
    sqlite3_bind_text(stmt, 3, appointment_date.c_str(), -1, SQLITE_TRANSIENT);

    bool blocked = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return blocked;
}

void registerAppointmentRoutes(crow::SimpleApp& app, sqlite3* db)
{
    CROW_ROUTE(app, "/booking_context").methods("POST"_method)
    ([db](const crow::request& req)
    {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }
        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "Please send a valid request.");
        }

        if (!body.has("doctor_id") || !body.has("doctor_name") || !body.has("category_name") ||
            !body.has("date") || !body.has("time_slot"))
        {
            return crow::response(400, "Please provide doctor_id, doctor_name, category_name, date, and time_slot.");
        }

        const int doctor_id = body["doctor_id"].i();
        const std::string doctor_name = body["doctor_name"].s();
        const std::string category_name = body["category_name"].s();
        const std::string appointment_date = body["date"].s();
        const std::string time_slot = body["time_slot"].s();

        if (doctor_id <= 0 || doctor_name.empty() || category_name.empty() ||
            appointment_date.empty() || time_slot.empty())
        {
            return crow::response(400, "Invalid booking details.");
        }

        // Verify doctor_id and doctor_name match the database
        sqlite3_stmt* stmt = nullptr;
        const char* sql_doctor =
            "SELECT doctor_name FROM Doctor WHERE doctor_id = ? LIMIT 1;";
        if (sqlite3_prepare_v2(db, sql_doctor, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't verify the doctor right now.");
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        std::string db_doctor_name;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            db_doctor_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);

        if (db_doctor_name.empty()) {
            return crow::response(400, "Doctor not found.");
        }

        // Resolve schedule_id from time_slot
        int schedule_id = -1;
        const char* sql_schedule =
            "SELECT schedule_id FROM Doctor_Schedule WHERE time_slot = ? LIMIT 1;";
        if (sqlite3_prepare_v2(db, sql_schedule, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't verify the schedule right now.");
        }
        sqlite3_bind_text(stmt, 1, time_slot.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            schedule_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        if (schedule_id <= 0) {
            return crow::response(400, "Invalid time slot.");
        }

        // Validate availability
        if (isSlotBlocked(db, doctor_id, schedule_id, appointment_date)) {
            return crow::response(409, "Sorry, that slot is blocked by the doctor for this date.");
        }
        if (isSlotAlreadyBooked(db, doctor_id, schedule_id, appointment_date)) {
            return crow::response(409, "Sorry, that slot has already been booked.");
        }

        const std::string booking_token = generateBookingToken();
        const auto expires_at = std::chrono::system_clock::now() + std::chrono::minutes(15);
        {
            std::lock_guard<std::mutex> lock(g_booking_mutex);
            g_booking_contexts[booking_token] = {
                doctor_id,
                db_doctor_name,
                category_name,
                appointment_date,
                time_slot,
                expires_at
            };
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["message"] = "Booking context created.";

        crow::response response(200, res);
        std::ostringstream cookie;
        cookie << "booking_token=" << booking_token
               << "; Path=/; Max-Age=900; HttpOnly; SameSite=Strict; Secure";
        response.add_header("Set-Cookie", cookie.str());
        return response;
    });

    CROW_ROUTE(app, "/booking_context").methods("GET"_method)
    ([db](const crow::request& req)
    {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }
        const std::string token = getBookingTokenFromRequest(req);
        if (token.empty()) {
            return crow::response(401, "Missing booking token.");
        }

        BookingContext ctx;
        if (!getBookingContext(token, ctx)) {
            return crow::response(401, "Invalid or expired booking token.");
        }

        // Refresh doctor name from DB to avoid mismatches
        sqlite3_stmt* stmt = nullptr;
        const char* sql_doctor =
            "SELECT doctor_name FROM Doctor WHERE doctor_id = ? LIMIT 1;";
        if (sqlite3_prepare_v2(db, sql_doctor, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't load booking details right now.");
        }
        sqlite3_bind_int(stmt, 1, ctx.doctor_id);
        std::string db_doctor_name;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            db_doctor_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["doctor_name"] = db_doctor_name.empty() ? ctx.doctor_name : db_doctor_name;
        res["category_name"] = ctx.category_name;
        res["date"] = ctx.appointment_date;
        res["time_slot"] = ctx.time_slot;

        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/book_appointment").methods("POST"_method)
    ([db](const crow::request& req)
    {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }
        const std::string booking_token = getBookingTokenFromRequest(req);
        BookingContext booking_ctx;
        if (booking_token.empty() || !getBookingContext(booking_token, booking_ctx)) {
            return crow::response(401, "Your booking session expired. Please select a slot again.");
        }

        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "Please send a valid request.");
        }

        // --- Required fields ---
        if (!body.has("name") || !body.has("age") || !body.has("email") ||
            !body.has("gender"))
        {
            return crow::response(400, "Please fill in all required fields.");
        }

        std::string name              = body["name"].s();
        int         age               = body["age"].i();
        std::string email             = body["email"].s();
        std::string gender            = body["gender"].s();
        std::string doctor_name       = booking_ctx.doctor_name;
        std::string appointment_date  = booking_ctx.appointment_date;
        std::string time_slot         = booking_ctx.time_slot;
        std::string request  ;
if (body.has("request")) {
    request = std::string(body["request"].s());
} else {
    request = "";
}


        std::cout << "[DEBUG] Received request: "
                  << name << ", " << age << ", " << email << ", "
                  << gender << ", " << doctor_name << ", "
                  << appointment_date << ", " << time_slot << ", "
                  << request << "\n";

        sqlite3_stmt* stmt = nullptr;

        // --- Step 1: Get doctor_id ---
        int doctor_id = -1;
        const char* sql_doctor =
            "SELECT doctor_id FROM Doctor WHERE doctor_name = ?";

        if (sqlite3_prepare_v2(db, sql_doctor, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't complete your request right now. Please try again.");
        }

        sqlite3_bind_text(stmt, 1, doctor_name.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            doctor_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        if (doctor_id == -1) {
            return crow::response(400, "Sorry, we could not find that doctor. Please choose another.");
        }

        std::cout << "[DEBUG] Doctor ID found: " << doctor_id << "\n";

        // --- Step 2: Get schedule_id ---
        int schedule_id = -1;
        const char* sql_schedule =
            "SELECT schedule_id FROM Doctor_Schedule WHERE time_slot = ?";

        if (sqlite3_prepare_v2(db, sql_schedule, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't complete your request right now. Please try again.");
        }

        sqlite3_bind_text(stmt, 1, time_slot.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            schedule_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        if (schedule_id == -1) {
            return crow::response(400, "Please select a valid time slot.");
        }

        std::cout << "[DEBUG] Schedule ID found: " << schedule_id << "\n";

        // --- Step 3: Slot safety check ---
        if (isSlotBlocked(db, doctor_id, schedule_id, appointment_date)) {
            return crow::response(409, "Sorry, that slot is blocked by the doctor for this date.");
        }
        if (isSlotAlreadyBooked(db, doctor_id, schedule_id, appointment_date)) {
            return crow::response(409, "Sorry, that slot has already been booked.");
        }

        // --- Step 4: Generate unique patient_id ---
        int patient_id;
        do {
            patient_id = generateRandomID();
        } while (Patient::exists(db, patient_id));

        std::cout << "[DEBUG] Generated patient ID: " << patient_id << "\n";

        // --- Step 5: Insert patient ---
        if (!Patient::insert(db, patient_id, name, age, email, gender, request)) {
            const int err = sqlite3_errcode(db);
            if (err == SQLITE_BUSY || err == SQLITE_LOCKED) {
                return crow::response(503, "Database is busy. Please try again in a moment.");
            }
            return crow::response(500, "Sorry, we couldn't save your details right now. Please try again.");
        }

        std::cout << "[DEBUG] Patient inserted successfully\n";

        // --- Step 6: Generate unique appointment_id ---
        int appointment_id;
        do {
            appointment_id = generateRandomID();
        } while (Appointment::exists(db, appointment_id));

        std::cout << "[DEBUG] Generated appointment ID: " << appointment_id << "\n";

        // --- Step 7: Insert appointment ---
        if (!Appointment::insert(
                db,
                appointment_id,
                patient_id,
                doctor_id,
                schedule_id,
                appointment_date))
        {
            const int err = sqlite3_errcode(db);
            if (err == SQLITE_CONSTRAINT) {
                // If the slot exists but is not BOOKED (e.g., Cancelled), reuse it.
                if (rebookCancelledAppointment(db, doctor_id, schedule_id, appointment_date, patient_id, appointment_id)) {
                    std::cout << "[DEBUG] Rebooked cancelled appointment: ID=" << appointment_id << "\n";
                } else {
                    deletePatientById(db, patient_id);
                    return crow::response(409, "Sorry, that slot has already been booked.");
                }
            }
            if (err == SQLITE_BUSY || err == SQLITE_LOCKED) {
                deletePatientById(db, patient_id);
                return crow::response(503, "Database is busy. Please try again in a moment.");
            }
            if (err != SQLITE_CONSTRAINT) {
                deletePatientById(db, patient_id);
                return crow::response(500, "Sorry, we couldn't finalize the appointment. Please try again.");
            }
        }

        std::cout << "[DEBUG] Appointment inserted successfully\n";

        // --- Step 8: Response ---
        crow::json::wvalue res;
        res["success"]        = true;
        res["message"]        = "Appointment booked successfully.";
        res["patient_id"]     = patient_id;
        res["appointment_id"] = appointment_id;

        const std::string confirmation_token = generateConfirmationToken();
        const auto expires_at = std::chrono::system_clock::now() + std::chrono::minutes(15);
        {
            std::lock_guard<std::mutex> lock(g_confirmation_mutex);
            g_confirmation_sessions[confirmation_token] = {appointment_id, patient_id, expires_at};
        }

        // --- Step 9: Async N8N ---
        crow::json::wvalue payload;
        payload["patient_id"]        = patient_id;
        payload["appointment_id"]    = appointment_id;
        payload["name"]              = name;
        payload["age"]               = age;
        payload["email"]             = email;
        payload["gender"]            = gender;
        payload["request"]           = request;
        payload["doctor_name"]       = doctor_name;
        payload["appointment_date"]  = appointment_date;
        payload["time_slot"]         = time_slot;

        std::thread([payload]() {
            sendToN8N(payload);
        }).detach();

        crow::response response(200, res);
        std::ostringstream cookie;
        cookie << "confirmation_token=" << confirmation_token
               << "; Path=/; Max-Age=900; HttpOnly; SameSite=Strict; Secure";
        response.add_header("Set-Cookie", cookie.str());
        return response;
    });

    CROW_ROUTE(app, "/confirmation_details").methods("GET"_method)
    ([db](const crow::request& req)
    {
        const std::string token = getConfirmationTokenFromRequest(req);
        if (token.empty()) {
            return crow::response(401, "Missing confirmation token.");
        }

        ConfirmationSession session;
        if (!getConfirmationSession(token, session)) {
            return crow::response(401, "Invalid or expired confirmation token.");
        }

        const char* sql =
            "SELECT a.appointment_id, a.patient_id, p.name, d.doctor_name, "
            "       a.appointment_date, ds.time_slot "
            "FROM Appointment a "
            "JOIN Patient p ON p.patient_id = a.patient_id "
            "JOIN Doctor d ON d.doctor_id = a.doctor_id "
            "JOIN Doctor_Schedule ds ON ds.schedule_id = a.schedule_id "
            "WHERE a.appointment_id = ? AND a.patient_id = ? "
            "LIMIT 1;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Sorry, we couldn't load the confirmation details right now.");
        }

        sqlite3_bind_int(stmt, 1, session.appointment_id);
        sqlite3_bind_int(stmt, 2, session.patient_id);

        crow::json::wvalue res;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            res["appointment_id"] = sqlite3_column_int(stmt, 0);
            res["patient_id"]     = sqlite3_column_int(stmt, 1);
            res["patient_name"]   = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
            res["doctor_name"]    = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
            res["date"]           = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
            res["time_slot"]      = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        } else {
            sqlite3_finalize(stmt);
            return crow::response(404, "Confirmation details not found.");
        }
        sqlite3_finalize(stmt);

        return crow::response(200, res);
    });
}
