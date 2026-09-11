#include "router.hpp"
#include "request.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
using Routes = std::unordered_map<std::string, Response>;
std::shared_ptr<const Routes> routes = std::make_shared<const Routes>();

Response response(const std::string &status, const std::string &type, const std::string &body,
                  const std::string &allow = "") {
    Response result;
    result.status = status;
    result.content_type = type;
    result.body = body;
    result.allow = allow;
    return result;
}

Response not_found() {
    return response("404 Not Found", "text/html; charset=utf-8",
                    "<!doctype html><html><head><title>404</title></head>"
                    "<body><h1>Page Not Found</h1></body></html>\n");
}

Response method_not_allowed(const std::string &allow) {
    return response("405 Method Not Allowed", "application/json",
                    "{\"error\":\"Method not allowed\"}\n", allow);
}

bool printable_header(const std::string &value) {
    for (const unsigned char c : value) {
        if (c < 0x20 || c > 0x7e)
            return false;
    }
    return !value.empty();
}

bool valid_status(const std::string &status) {
    return status.size() >= 5 && status.size() <= 128 && status[0] >= '2' && status[0] <= '5' &&
           status[1] >= '0' && status[1] <= '9' && status[2] >= '0' && status[2] <= '9' &&
           status[3] == ' ' && printable_header(status);
}

bool valid_route_path(const std::string &path) {
    return path.size() <= 2048 && valid_uri_path(path);
}

std::string read_config(const std::string &path) {
    struct ConfigFile {
        int descriptor;
        ~ConfigFile() {
            if (descriptor >= 0)
                ::close(descriptor);
        }
    };
    // Opening with O_NONBLOCK avoids waiting on a FIFO before fstat rejects it.
    ConfigFile file{::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY)};
    if (file.descriptor < 0)
        throw std::runtime_error("Cannot open route configuration: " + path);
    struct stat info {};
    if (::fstat(file.descriptor, &info) < 0 || !S_ISREG(info.st_mode)) {
        throw std::runtime_error("Route configuration must be a regular file");
    }
    if (info.st_size < 0 || static_cast<std::uintmax_t>(info.st_size) > MAX_CONFIG_BYTES) {
        throw std::runtime_error("Route configuration exceeds the 1 MiB limit");
    }
    std::string content(MAX_CONFIG_BYTES + 1, '\0');
    std::size_t offset = 0;
    while (offset < content.size()) {
        const auto count =
            ::read(file.descriptor, content.data() + offset, content.size() - offset);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0)
            throw std::runtime_error("Cannot read route configuration");
        if (count == 0)
            break;
        offset += static_cast<std::size_t>(count);
    }
    if (offset > MAX_CONFIG_BYTES) {
        throw std::runtime_error("Route configuration exceeds the 1 MiB limit");
    }
    content.resize(offset);
    return content;
}

nlohmann::json parse_config(const std::string &content) {
    using Json = nlohmann::json;
    std::vector<std::unordered_set<std::string>> object_keys;
    return Json::parse(content, [&](int depth, Json::parse_event_t event, Json &parsed) {
        if (depth > 2)
            throw std::runtime_error("Route configuration is nested too deeply");
        if (event == Json::parse_event_t::object_start)
            object_keys.emplace_back();
        else if (event == Json::parse_event_t::object_end)
            object_keys.pop_back();
        else if (event == Json::parse_event_t::key &&
                 !object_keys.back().insert(parsed.get<std::string>()).second) {
            throw std::runtime_error("Duplicate key in route configuration");
        }
        return true;
    });
}

// Echo is a text demo: malformed UTF-8 is replaced with U+FFFD while the byte
// count still reports the original request size. Binary bodies cannot throw.
std::string dump_json(const nlohmann::json &value) {
    return value.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
}
} // namespace

void init_router_config(const std::string &path) {
    std::string config_path = path;
    if (config_path.empty()) {
        if (std::filesystem::exists("config.json"))
            config_path = "config.json";
        else if (std::filesystem::exists("../config.json"))
            config_path = "../config.json";
        else {
            std::atomic_store(&routes, std::make_shared<const Routes>());
            std::cerr << "No config.json found; only built-in routes are enabled.\n";
            return;
        }
    }

    const auto config = parse_config(read_config(config_path));
    if (!config.is_object())
        throw std::runtime_error("Route configuration must be a JSON object");

    auto next = std::make_shared<Routes>();
    for (auto it = config.begin(); it != config.end(); ++it) {
        const auto &route = it.value();
        if (!valid_route_path(it.key()))
            throw std::runtime_error("Invalid route path in configuration");
        if (it.key() == "/health" || it.key() == "/echo" || it.key() == "/api/data" ||
            it.key().compare(0, 8, "/static/") == 0) {
            throw std::runtime_error("Configuration shadows a built-in route: " + it.key());
        }
        if (!route.is_object() || route.size() != 3 || !route.contains("status") ||
            !route["status"].is_string() || !route.contains("content_type") ||
            !route["content_type"].is_string() || !route.contains("body") ||
            !route["body"].is_string()) {
            throw std::runtime_error("Each route requires status, content_type and body strings");
        }
        auto configured =
            response(route["status"].get<std::string>(), route["content_type"].get<std::string>(),
                     route["body"].get<std::string>());
        if (!valid_status(configured.status) || configured.content_type.size() > 256 ||
            !printable_header(configured.content_type)) {
            throw std::runtime_error("Invalid status or content_type in route configuration");
        }
        const auto code = configured.status.substr(0, 3);
        if ((code == "204" || code == "205" || code == "304") && !configured.body.empty()) {
            throw std::runtime_error("Statuses 204, 205 and 304 require an empty body");
        }
        if (code == "405")
            configured.allow = "GET, HEAD";
        next->emplace(it.key(), std::move(configured));
    }
    std::atomic_store(&routes, std::shared_ptr<const Routes>(std::move(next)));
}

Response handle_route(const std::string &path) {
    return handle_route_with_method("GET", path, "");
}

Response handle_route_with_method(const std::string &method, const std::string &target,
                                  const std::string &body) {
    const auto path = target.substr(0, target.find('?'));
    const bool read_method = method == "GET" || method == "HEAD";
    if (path == "/health") {
        if (!read_method)
            return method_not_allowed("GET, HEAD");
        return response("200 OK", "application/json", "{\"status\":\"ok\"}\n");
    }
    if (path == "/echo") {
        if (method != "POST" && method != "PUT")
            return method_not_allowed("POST, PUT");
        const nlohmann::json value = {
            {"method", method}, {"received_body", body}, {"body_length", body.size()}};
        return response("200 OK", "application/json", dump_json(value));
    }
    if (path == "/api/data") {
        if (read_method) {
            return response("200 OK", "application/json",
                            "{\"data\":[\"item1\",\"item2\",\"item3\"]}\n");
        }
        if (method == "POST" || method == "PUT") {
            const bool create = method == "POST";
            const nlohmann::json value = {
                {"message", create ? "Resource created" : "Resource updated"}, {"received", body}};
            return response(create ? "201 Created" : "200 OK", "application/json",
                            dump_json(value));
        }
        if (method == "DELETE") {
            return response("200 OK", "application/json", "{\"message\":\"Resource deleted\"}\n");
        }
        return method_not_allowed("GET, HEAD, POST, PUT, DELETE");
    }
    const auto snapshot = std::atomic_load(&routes);
    const auto found = snapshot->find(path);
    if (found == snapshot->end())
        return not_found();
    if (!read_method)
        return method_not_allowed("GET, HEAD");
    return found->second;
}
