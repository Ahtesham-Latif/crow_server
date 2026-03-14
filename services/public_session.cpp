#include "public_session.h"

#include <unordered_map>
#include <mutex>
#include <chrono>
#include <random>
#include <sstream>

namespace {

struct PublicSession {
    std::chrono::system_clock::time_point expires_at;
};

std::unordered_map<std::string, PublicSession> g_public_sessions;
std::mutex g_public_mutex;

std::string generateToken() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned long long> dis;

    std::ostringstream out;
    out << std::hex << dis(gen) << dis(gen);
    return out.str();
}

void pruneExpired() {
    const auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(g_public_mutex);
    for (auto it = g_public_sessions.begin(); it != g_public_sessions.end();) {
        if (it->second.expires_at <= now) {
            it = g_public_sessions.erase(it);
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

} // namespace

void issuePublicSession(crow::response& res) {
    const std::string token = generateToken();
    const auto expires_at = std::chrono::system_clock::now() + std::chrono::minutes(30);
    {
        std::lock_guard<std::mutex> lock(g_public_mutex);
        g_public_sessions[token] = {expires_at};
    }

    std::ostringstream cookie;
    cookie << "public_token=" << token
           << "; Path=/; Max-Age=1800; HttpOnly; SameSite=Lax; Secure";
    res.add_header("Set-Cookie", cookie.str());
}

bool publicSessionValid(const crow::request& req) {
    pruneExpired();

    std::string token;
    const std::string auth = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (auth.rfind(prefix, 0) == 0) {
        token = auth.substr(prefix.size());
    } else {
        token = getCookieValue(req, "public_token");
    }

    if (token.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_public_mutex);
    return g_public_sessions.find(token) != g_public_sessions.end();
}
