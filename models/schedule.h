#pragma once

#include <sqlite3.h>
#include <string>
#include <iostream>
#include <vector>

class DoctorSchedule {
public:
    int schedule_id;
    std::string time_slot;  // HH:MM format

    DoctorSchedule() = default;
    DoctorSchedule(int id, const std::string& slot) : schedule_id(id), time_slot(slot) {}

    // Insert a new slot into the DB
    static bool insert(sqlite3* db, const std::string& slot) {
        const char* sql = "INSERT INTO Doctor_Schedule(time_slot) VALUES (?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, slot.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            std::cerr << "Insert failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        return true;
    }

    // Fetch all slots
    static std::vector<DoctorSchedule> fetchAll(sqlite3* db) {
        std::vector<DoctorSchedule> slots;
        const char* sql = "SELECT schedule_id, time_slot FROM Doctor_Schedule ORDER BY time_slot;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return slots;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            std::string slot = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            slots.emplace_back(id, slot);
        }

        sqlite3_finalize(stmt);
        return slots;
    }

    // Fetch available slots for a doctor on a given date
    static std::vector<DoctorSchedule> fetchAvailableSlots(sqlite3* db, int doctor_id, const std::string& date) {
        std::vector<DoctorSchedule> slots;
        const char* sql =
            "SELECT ds.schedule_id, ds.time_slot "
            "FROM Doctor_Schedule ds "
            "WHERE NOT EXISTS ("
            "    SELECT 1 FROM Appointment a "
            "    WHERE a.doctor_id = ? "
            "      AND a.schedule_id = ds.schedule_id "
            "      AND a.appointmentDateTime = ? "
            "      AND a.status = 'BOOKED'"
            ") "
            "ORDER BY ds.time_slot;";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return slots;
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            std::string slot = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            slots.emplace_back(id, slot);
        }

        sqlite3_finalize(stmt);
        return slots;
    }
};
