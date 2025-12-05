#include <iostream>
#include "crow.h"

using namespace std;

// In-memory storage
string storedItem = "";

// URL decode helper: converts '+' → space, '%xx' → corresponding char
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
    crow::SimpleApp app;

    // ------------------- CREATE -------------------
    CROW_ROUTE(app, "/create")
        .methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        // Note: The POST form is already a separate origin (different port in your case),
        // so setting CORS headers here is also a good practice for /create, 
        // though typically not strictly required for POSTs that don't use custom headers.
        crow::response res;
        res.set_header("Access-Control-Allow-Origin", "*"); 
        
        string body = req.body; // e.g., "data=%7B%22message%22%3A+%22Hello%2C+K%22%7D"
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

        storedItem = decoded;
        res.code = 201;
        res.write("Created from Browser Form:\n" + storedItem);
        return res;
    });

    // ------------------- READ -------------------
    CROW_ROUTE(app, "/read")
        .methods(crow::HTTPMethod::Get)
    ([](){
        crow::response res;
        
        // ***************************************************************
        // CRITICAL CHANGE: Set the CORS header to allow cross-origin access 
        // from your HTML page running on port 5500.
        // ***************************************************************
        res.set_header("Access-Control-Allow-Origin", "*"); 
        
        if (storedItem.empty()) {
            res.code = 200;
            res.write("No item stored.");
            return res;
        }
        
        // Also set the Content-Type, as your response is plain text
        res.set_header("Content-Type", "text/plain");
        
        res.code = 200;
        res.write("Stored item: " + storedItem);
        return res;
    });

    // ------------------- UPDATE -------------------
    CROW_ROUTE(app, "/update")
        .methods(crow::HTTPMethod::Put)
    ([](const crow::request& req){
        crow::response res;
        res.set_header("Access-Control-Allow-Origin", "*");
        
        if (storedItem.empty()) {
            res.code = 404;
            res.write("Nothing to update.");
            return res;
        }
        storedItem = req.body; // update via raw PUT body
        res.code = 200;
        res.write("Updated to: " + storedItem);
        return res;
    });

    // ------------------- DELETE -------------------
    CROW_ROUTE(app, "/delete")
        .methods(crow::HTTPMethod::Delete)
    ([](){
        crow::response res;
        res.set_header("Access-Control-Allow-Origin", "*");
        
        if (storedItem.empty()) {
            res.code = 404;
            res.write("Nothing to delete.");
            return res;
        }
        storedItem.clear();
        res.code = 200;
        res.write("Deleted.");
        return res;
    });

    CROW_LOG_INFO << "Server running at http://0.0.0.0:18080";
    app.port(18080).multithreaded().run();

    return 0;
}
