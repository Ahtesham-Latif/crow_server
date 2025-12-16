// Force Windows 10 compatibility for httplib
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00  // Windows 10
#endif

#include <iostream>
#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include "crow.h"
#include "httplib.h"  // For sending HTTP requests to n8n

using namespace std;

// ============================================
// 🔧 N8N WEBHOOK CONFIGURATION
// ============================================
// TODO: Change this to your actual n8n webhook path!
// Example: "/webhook/abc-123-def-456"
const string N8N_HOST = "localhost";
const int N8N_PORT = 5678;
const string N8N_WEBHOOK_PATH = "WEBHOOK_PATH";  // ⚠️ CHANGE THIS!
// ============================================

// Global database pointer
sqlite3* db = nullptr;

// Initialize database and create tables
void initDatabase() {
    int rc = sqlite3_open("mydata.db", &db);
    if (rc) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return;
    }
    
    // Create items table (existing table)
    const char* sql_items = 
        "CREATE TABLE IF NOT EXISTS items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "data TEXT NOT NULL,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");";
    
    // ============================================
    // 🆕 NEW: Create users table for login data
    // ============================================
    const char* sql_users = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "email TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");";
    
    char* errMsg = nullptr;
    
    // Create items table
    rc = sqlite3_exec(db, sql_items, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error (items): " << errMsg << endl;
        sqlite3_free(errMsg);
    }
    
    // Create users table
    rc = sqlite3_exec(db, sql_users, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error (users): " << errMsg << endl;
        sqlite3_free(errMsg);
    } else {
        cout << "Database initialized successfully!" << endl;
    }
}

// ============================================
// 🆕 NEW: Function to send data to n8n webhook
// ============================================
bool sendToN8nWebhook(const string& email, const string& name) {
    try {
        // Create HTTP client
        httplib::Client cli(N8N_HOST, N8N_PORT);
        cli.set_connection_timeout(5, 0); // 5 seconds timeout
        
        // Prepare JSON data to send
        crow::json::wvalue webhookData;
        webhookData["email"] = email;
        webhookData["name"] = name;
        webhookData["timestamp"] = time(nullptr);
        webhookData["source"] = "crow_server";
        
        string jsonStr = webhookData.dump();
        
        // Send POST request to n8n
        auto res = cli.Post(N8N_WEBHOOK_PATH, jsonStr, "application/json");
        
        if (res && (res->status == 200 || res->status == 201)) {
            cout << "✅ Webhook sent successfully to n8n!" << endl;
            cout << "Response: " << res->body << endl;
            return true;
        } else {
            cerr << "❌ Webhook failed. Status: " << (res ? res->status : 0) << endl;
            if (res) {
                cerr << "Response body: " << res->body << endl;
            }
            return false;
        }
        
    } catch (const exception& e) {
        cerr << "❌ Webhook error: " << e.what() << endl;
        return false;
    }
}

// URL decode helper (existing)
string url_decode(const string &s) {
    string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < s.size()) {
            string hex = s.substr(i + 1, 2);
            char decoded = static_cast<char>(strtol(hex.c_str(), nullptr, 16));
            out += decoded;
            i += 2;
        } else {
            out += c;
        }
    }
    return out;
}

int main() {
    // Initialize database
    initDatabase();
    
    crow::SimpleApp app;

    // ============================================
    // 🆕 NEW: Serve login HTML page
    // ============================================
    CROW_ROUTE(app, "/login")
        .methods(crow::HTTPMethod::Get)
    ([](){
        crow::response res;
        
        // Read login.html file
        ifstream file("login.html");
        if (!file.is_open()) {
            res.code = 500;
            res.write("Error: login.html not found! Make sure it's in the same folder as main.cpp");
            return res;
        }
        
        stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        res.set_header("Content-Type", "text/html");
        res.write(buffer.str());
        return res;
    });

    // ============================================
    // 🆕 NEW: Handle login form submission
    // ============================================
    CROW_ROUTE(app, "/api/login")
        .methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        crow::response res;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");

        // Parse JSON from request body
        auto body = crow::json::load(req.body);
        if (!body) {
            res.code = 400;
            crow::json::wvalue error;
            error["success"] = false;
            error["message"] = "Invalid JSON";
            res.write(error.dump());
            return res;
        }

        string email = body["email"].s();
        string name = body["name"].s();

        // Validate input
        if (email.empty() || name.empty()) {
            res.code = 400;
            crow::json::wvalue error;
            error["success"] = false;
            error["message"] = "Email and name are required";
            res.write(error.dump());
            return res;
        }

        // ============================================
        // STEP 1: Save to database
        // ============================================
        string sql = "INSERT INTO users (email, name) VALUES (?, ?);";
        sqlite3_stmt* stmt;
        bool dbSuccess = false;
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                dbSuccess = true;
                cout << "✅ User saved to database: " << email << endl;
            }
            sqlite3_finalize(stmt);
        }

        if (!dbSuccess) {
            res.code = 500;
            crow::json::wvalue error;
            error["success"] = false;
            error["message"] = "Failed to save to database";
            res.write(error.dump());
            return res;
        }

        // ============================================
        // STEP 2: Send to n8n webhook
        // ============================================
        bool webhookSuccess = sendToN8nWebhook(email, name);

        // ============================================
        // STEP 3: Return success response
        // ============================================
        crow::json::wvalue response;
        response["success"] = true;
        response["message"] = "Login successful!";
        response["database"] = "saved";
        response["webhook"] = webhookSuccess ? "sent" : "failed";
        
        res.code = 200;
        res.write(response.dump());
        return res;
    });

    // ============================================
    // EXISTING ROUTES BELOW (unchanged)
    // ============================================

    // ------------------- CREATE -------------------
    CROW_ROUTE(app, "/create")
        .methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        crow::response res;
        res.set_header("Access-Control-Allow-Origin", "*");

        string body = req.body;
        string key = "data=";
        string encoded = "";

        size_t pos = body.find(key);
        if (pos != string::npos) {
            encoded = body.substr(pos + key.size());
        }

        string decoded = url_decode(encoded);
        if (decoded.empty()) {
            res.code = 400;
            res.write("No 'data' field found.");
            return res;
        }

        // Insert into SQLite
        string sql = "INSERT INTO items (data) VALUES (?);";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, decoded.c_str(), -1, SQLITE_STATIC);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                res.code = 201;
                res.write("Created in Database:\n" + decoded);
            } else {
                res.code = 500;
                res.write("Failed to insert data.");
            }
            sqlite3_finalize(stmt);
        } else {
            res.code = 500;
            res.write("Database error.");
        }
        
        return res;
    });

    // ------------------- READ -------------------
    CROW_ROUTE(app, "/read")
        .methods(crow::HTTPMethod::Get)
    ([](){
        crow::response res;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");

        // Read all items from SQLite
        string sql = "SELECT id, data, created_at FROM items ORDER BY id DESC;";
        sqlite3_stmt* stmt;
        
        crow::json::wvalue result;
        int index = 0;
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                result[index]["id"] = sqlite3_column_int(stmt, 0);
                result[index]["data"] = string((const char*)sqlite3_column_text(stmt, 1));
                result[index]["created_at"] = string((const char*)sqlite3_column_text(stmt, 2));
                index++;
            }
            sqlite3_finalize(stmt);
        }
        
        if (index == 0) {
            res.code = 200;
            res.write("No items in database.");
        } else {
            res.code = 200;
            res.write(result.dump());
        }
        
        return res;
    });

    // ------------------- UPDATE -------------------
    CROW_ROUTE(app, "/update/<int>")
        .methods(crow::HTTPMethod::Put)
    ([](const crow::request& req, int id){
        crow::response res;
        res.set_header("Access-Control-Allow-Origin", "*");

        string newData = req.body;
        if (newData.empty()) {
            res.code = 400;
            res.write("No data provided.");
            return res;
        }

        // Update in SQLite
        string sql = "UPDATE items SET data = ? WHERE id = ?;";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, newData.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, id);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                if (sqlite3_changes(db) > 0) {
                    res.code = 200;
                    res.write("Updated item " + to_string(id) + " to: " + newData);
                } else {
                    res.code = 404;
                    res.write("Item not found.");
                }
            } else {
                res.code = 500;
                res.write("Failed to update.");
            }
            sqlite3_finalize(stmt);
        } else {
            res.code = 500;
            res.write("Database error.");
        }
        
        return res;
    });

    // ------------------- DELETE -------------------
    CROW_ROUTE(app, "/delete/<int>")
        .methods(crow::HTTPMethod::Delete)
    ([](int id){
        crow::response res;
        res.set_header("Access-Control-Allow-Origin", "*");

        // Delete from SQLite
        string sql = "DELETE FROM items WHERE id = ?;";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                if (sqlite3_changes(db) > 0) {
                    res.code = 200;
                    res.write("Deleted item " + to_string(id));
                } else {
                    res.code = 404;
                    res.write("Item not found.");
                }
            } else {
                res.code = 500;
                res.write("Failed to delete.");
            }
            sqlite3_finalize(stmt);
        } else {
            res.code = 500;
            res.write("Database error.");
        }
        
        return res;
    });

    // ============================================
    // Server startup
    // ============================================
    CROW_LOG_INFO << "Server running at http://0.0.0.0:18080";
    CROW_LOG_INFO << "Login page: http://localhost:18080/login";
    CROW_LOG_INFO << "Database: mydata.db";
    CROW_LOG_INFO << "N8N Webhook: " << N8N_HOST << ":" << N8N_PORT << N8N_WEBHOOK_PATH;
    
    app.port(18080).multithreaded().run();
    
    // Close database on exit
    sqlite3_close(db);
    return 0;
}