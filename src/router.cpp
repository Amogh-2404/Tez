#include "router.hpp"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

Response handle_route(const std::string& path) {
    Response resp;
    nlohmann::json config;
    std::ifstream config_file("../config.json");  // From build/ directory
    if (config_file) {
        config_file >> config;
        if (config.contains(path)) {
            resp.status = config[path]["status"];
            resp.content_type = config[path]["content_type"];
            resp.body = config[path]["body"];
            return resp;
        }
    }
    // Fallback to hardcoded 404
    resp.status = "404 Not Found";
    resp.content_type = "text/html; charset=utf-8";
    resp.body = "<html><head><title>404</title></head><body><h1>Page Not Found</h1></body></html>";
    return resp;
}