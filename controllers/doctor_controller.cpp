#include <crow.h>
#include <sqlite3.h>
#include <vector>
#include <string>
#include "../models/doctor.h"          // Doctor model
#include "../services/public_session.h"
#include "doctor_controller.h"         // This controller's header

using namespace std;
void registerDoctorRoutes(crow::SimpleApp& app, sqlite3* db) {

    // ---------------------------------
    // GET doctors by category (query param version)
    // ---------------------------------
    CROW_ROUTE(app, "/get_doctors").methods("GET"_method)
    ([db](const crow::request& req) {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }
        auto query = req.url_params.get("category_id");
        sqlite3_stmt* stmt;
        const char* sql = nullptr;
        int category_id = 0;
        int cache_key = -1;

        if (query) {
            category_id = std::stoi(query);
            cache_key = category_id;
            sql =
                "SELECT doctor_id, doctor_name, phone, experience_years, qualification, ratings, category_id "
                "FROM Doctor WHERE category_id = ?";
        } else {
            sql =
                "SELECT doctor_id, doctor_name, phone, experience_years, qualification, ratings, category_id "
                "FROM Doctor";
        }

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "SQL error: " << sqlite3_errmsg(db) << std::endl;
            return crow::response(500, "Sorry, we couldn't load the doctors right now. Please try again.");
        }

        if (query) {
            sqlite3_bind_int(stmt, 1, category_id);
        }

        crow::json::wvalue result;
        int i = 0;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result[i]["doctor_id"] = sqlite3_column_int(stmt, 0);
            result[i]["doctor_name"] =
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            result[i]["phone"] =
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            result[i]["experience_years"] =
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            result[i]["qualifications"] =
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            result[i]["ratings"] = sqlite3_column_double(stmt, 5);
            result[i]["category_id"] = sqlite3_column_int(stmt, 6);
            i++;
        }

        sqlite3_finalize(stmt);
        return crow::response(200, result);
    });

    // ---------------------------------
    // POST add new doctor
    // ---------------------------------
    CROW_ROUTE(app, "/add_doctor").methods("POST"_method)
    ([db](const crow::request& req) {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "Please send a valid request.");
        }

        if (!body.has("doctor_name") ||
            !body.has("phone") ||
            !body.has("experience_years") ||
            !body.has("qualifications") ||
            !body.has("ratings") ||
            !body.has("category_id")) {
            return crow::response(400, "Please fill in all required fields.");
        }

        // Extract fields
        string name = body["doctor_name"].s();
        string phone = body["phone"].s();
        string experience = body["experience_years"].s();
        string degree = body["qualifications"].s();
        double rating = body["ratings"].d();
        int category_id = body["category_id"].i();

        // Call insert() directly with proper arguments
        bool inserted = Doctor::insert(db, name, phone, experience, degree, rating, category_id);

        crow::json::wvalue response;
        response["success"] = inserted;
        response["message"] =
            inserted ? "Doctor added successfully." : "Sorry, we could not add that doctor right now.";

    return crow::response(200, response);
});

    // ---------------------------------
    // PUT update doctor
    // ---------------------------------
    CROW_ROUTE(app, "/update_doctor/<int>").methods("PUT"_method)
    ([db](const crow::request& req, int doctor_id) {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        if (doctor_id <= 0) {
            return crow::response(400, "Please provide a valid doctor_id.");
        }

        auto body = crow::json::load(req.body);
        if (!body ||
            !body.has("doctor_name") ||
            !body.has("phone") ||
            !body.has("experience_years") ||
            !body.has("qualifications") ||
            !body.has("ratings") ||
            !body.has("category_id")) {
            return crow::response(400, "Please fill in all required fields.");
        }

        string name = body["doctor_name"].s();
        string phone = body["phone"].s();
        string experience = body["experience_years"].s();
        string degree = body["qualifications"].s();
        double rating = body["ratings"].d();
        int category_id = body["category_id"].i();

        sqlite3_stmt* stmt = nullptr;
        const char* sql =
            "UPDATE Doctor SET doctor_name = ?, phone = ?, experience_years = ?, qualification = ?, ratings = ?, category_id = ? "
            "WHERE doctor_id = ?;";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return crow::response(500, "Sorry, we couldn't update the doctor right now. Please try again.");
        }

        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, phone.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, experience.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, degree.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 5, rating);
        sqlite3_bind_int(stmt, 6, category_id);
        sqlite3_bind_int(stmt, 7, doctor_id);

        const int rc = sqlite3_step(stmt);
        const int changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);

        const bool success = (rc == SQLITE_DONE && changes > 0);

        crow::json::wvalue response;
        response["success"] = success;
        response["message"] = success
            ? "Doctor updated successfully."
            : "Sorry, that doctor was not found or could not be updated.";

        return crow::response(200, response);
    });

    // ---------------------------------
    // DELETE doctor
    // ---------------------------------
    CROW_ROUTE(app, "/delete_doctor/<int>").methods("DELETE"_method)
    ([db](const crow::request& req, int doctor_id) {
        if (!publicSessionValid(req)) {
            return crow::response(401, "Please refresh and try again.");
        }

        if (doctor_id <= 0) {
            return crow::response(400, "Please provide a valid doctor_id.");
        }

        bool deleted = Doctor::remove(db, doctor_id);

        crow::json::wvalue response;
        response["success"] = deleted;
        response["message"] =
            deleted ? "Doctor deleted successfully."
                    : "Sorry, that doctor was not found or has already been deleted.";

        return crow::response(200, response);
    });

}
