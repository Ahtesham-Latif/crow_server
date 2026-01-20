#pragma once

#include <sqlite3.h>
#include <string>
#include <iostream>
#include <vector>
using namespace std;

class DoctorSchedule {
public:
    int schedule_id;
    string time_slot;  // HH:MM format

    DoctorSchedule() = default;
    DoctorSchedule(int id, const std::string& slot) : schedule_id(id), time_slot(slot) {}

    // Insert a new slot into the DB
    static bool insert(sqlite3* db, const std::string& slot) {
        const char* sql = "INSERT INTO Doctor_Schedule(time_slot) VALUES (?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, slot.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            cerr << "Insert failed: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        return true;
    }

    // Fetch all slots
    static vector<DoctorSchedule> fetchAll(sqlite3* db) {
        vector<DoctorSchedule> slots;
        const char* sql = "SELECT schedule_id, time_slot FROM Doctor_Schedule ORDER BY time_slot;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
            return slots;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            string slot = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            slots.emplace_back(id, slot);
        }

        sqlite3_finalize(stmt);
        return slots;
    }

    // Fetch available slots for a doctor on a given date (exclude BOOKED or BLOCKED)
    static vector<DoctorSchedule> fetchAvailableSlots(sqlite3* db, int doctor_id, const string& date) {
        vector<DoctorSchedule> slots;
        const char* sql =
            "SELECT ds.schedule_id, ds.time_slot "
            "FROM Doctor_Schedule ds "
            "WHERE NOT EXISTS ("
            "    SELECT 1 FROM Appointment a "
            "    WHERE a.doctor_id = ? "
            "      AND a.schedule_id = ds.schedule_id "
            "      AND a.appointmentDateTime = ? "
            "      AND (a.status = 'BOOKED' OR a.status = 'BLOCKED')"
            ") "
            "ORDER BY ds.time_slot;";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
            return slots;
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            string slot = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            slots.emplace_back(id, slot);
        }

        sqlite3_finalize(stmt);
        return slots;
    }

    // Block a slot for a specific doctor
    static bool blockSlotForDoctor(sqlite3* db, int doctor_id, int schedule_id) {
        // First check if slot already has a BOOKED appointment
        const char* check_sql =
            "SELECT COUNT(*) FROM Appointment WHERE doctor_id = ? AND schedule_id = ? AND status = 'BOOKED';";
        sqlite3_stmt* check_stmt;

        if (sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, nullptr) != SQLITE_OK) {
            cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_int(check_stmt, 1, doctor_id);
        sqlite3_bind_int(check_stmt, 2, schedule_id);

        int rc = sqlite3_step(check_stmt);
        int count = 0;
        if (rc == SQLITE_ROW) {
            count = sqlite3_column_int(check_stmt, 0);
        }
        sqlite3_finalize(check_stmt);

        if (count > 0) {
            cerr << "Cannot block slot: doctor already has a booked appointment." << endl;
            return false;
        }

        // Insert BLOCKED appointment (doctor-specific)
        const char* sql =
            "INSERT INTO Appointment (doctor_id, schedule_id, appointmentDateTime, status) "
            "VALUES (?, ?, '', 'BLOCKED');";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_int(stmt, 2, schedule_id);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            cerr << "Failed to block slot: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        return true;
    }
};
