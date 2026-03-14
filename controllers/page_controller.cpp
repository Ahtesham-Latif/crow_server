#include "page_controller.h"
#include "../services/public_session.h"

#include <fstream>
#include <sstream>

namespace {

std::string getMimeType(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return "application/octet-stream";
    }
    const std::string ext = path.substr(dot + 1);
    if (ext == "html") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "ico") return "image/x-icon";
    return "application/octet-stream";
}

crow::response serveFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    crow::response res;

    if (!file.is_open()) {
        res.code = 404;
        res.write("Sorry, the requested file was not found.");
        return res;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    res.set_header("Content-Type", getMimeType(path));
    res.write(buffer.str());
    return res;
}

} // namespace

void registerPageRoutes(crow::SimpleApp& app)
{
    CROW_ROUTE(app, "/public_session").methods("GET"_method)
    ([](const crow::request& req) {
        crow::response res(204);
        issuePublicSession(res);
        return res;
    });

    CROW_ROUTE(app, "/categories_page")([]() { return serveFile("../public/category.html"); });
    CROW_ROUTE(app, "/doctors_page")([]() { return serveFile("../public/doctor.html"); });
    CROW_ROUTE(app, "/schedule_page")([]() { return serveFile("../public/schedule.html"); });
    CROW_ROUTE(app, "/appointment_page")([]() { return serveFile("../public/appointment.html"); });
    CROW_ROUTE(app, "/confirmation_page")([]() { return serveFile("../public/confirmation.html"); });
    CROW_ROUTE(app, "/cancel_appointment_page")([]() { return serveFile("../public/cancellation.html"); });
    CROW_ROUTE(app, "/doctor_dashboard_page")([]() { return serveFile("../public/doctor_dashboard.html"); });
    CROW_ROUTE(app, "/controller(mind)")([]() { return serveFile("../public/mind.html"); });

    CROW_ROUTE(app, "/assets/<string>")([](const std::string& filename) {
        if (filename.find("..") != std::string::npos ||
            filename.find('/') != std::string::npos ||
            filename.find('\\') != std::string::npos) {
            return crow::response(400, "Sorry, that file path is not allowed.");
        }
        return serveFile("../public/assets/" + filename);
    });
}
