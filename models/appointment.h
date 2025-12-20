#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
#include <iostream>
#include "patient.h"        // Patient model
#include "schedule.h"       // DoctorSchedule model
#include "doctor.h"         // Doctor model

class Appointment {
public:
    int appointment_id;
    int patient_id;
    int doctor_id;
    int schedule_id;
    std::string appointment_date; // YYYY-MM-DD
    std::string status;           // BOOKED, CANCELLED
    std::string created_at;       // default: datetime('now','localtime')

    Appointment() = default;

    Appointment(int patient, int doctor, int schedule, const std::string& date,
                const std::string& stat = "BOOKED")
        : patient_id(patient), doctor_id(doctor), schedule_id(schedule),
          appointment_date(date), status(stat) {}

    // Insert appointment after patient exists
    static bool insert(sqlite3* db, int patient_id, int doctor_id, int schedule_id,
                       const std::string& date) 
    {
        const char* sql =
            "INSERT INTO Appointment(patient_id, doctor_id, schedule_id, appointment_date, status, created_at) "
            "VALUES (?, ?, ?, ?, 'BOOKED', datetime('now','localtime'));";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, patient_id);
        sqlite3_bind_int(stmt, 2, doctor_id);
        sqlite3_bind_int(stmt, 3, schedule_id);
        sqlite3_bind_text(stmt, 4, date.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            std::cerr << "Insert failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        return true;
    }

    // Fetch appointments for a doctor on a specific date
    static std::vector<Appointment> fetchByDoctorAndDate(sqlite3* db, int doctor_id, const std::string& date) {
        std::vector<Appointment> appointments;
        const char* sql =
            "SELECT appointment_id, patient_id, doctor_id, schedule_id, appointment_date, status, created_at "
            "FROM Appointment "
            "WHERE doctor_id = ? AND appointment_date = ? "
            "ORDER BY schedule_id;";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << std::endl;
            return appointments;
        }

        sqlite3_bind_int(stmt, 1, doctor_id);
        sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Appointment a;
            a.appointment_id = sqlite3_column_int(stmt, 0);
            a.patient_id = sqlite3_column_int(stmt, 1);
            a.doctor_id = sqlite3_column_int(stmt, 2);
            a.schedule_id = sqlite3_column_int(stmt, 3);
            a.appointment_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            a.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            a.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            appointments.push_back(a);
        }

        sqlite3_finalize(stmt);
        return appointments;
    }
};
