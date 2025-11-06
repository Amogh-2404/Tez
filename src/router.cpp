#include "router.hpp"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "middleware.hpp"

Response handle_route(const std::string& path) {
    if (path == "/health") {
        Response resp;
        resp.status = "200 OK";
        resp.content_type = "application/json";
        resp.body = "{\"status\":\"ok\"}\n";
        return resp;
    }

    Response cached = get_cached_response(path);
    if(!cached.body.empty()){
        return cached;
    }

    Response resp;
    nlohmann::json config;
    std::ifstream config_file("../config.json");  // From build/ directory

    try{
    if (config_file) {
        config_file >> config;
        if (config.contains(path)) {
            resp.status = config[path]["status"];
            resp.content_type = config[path]["content_type"];
            resp.body = config[path]["body"];
        }else{
            resp.status = "404 Not Found";
            resp.content_type = "text/html; charset=utf-8";
            resp.body = "<html><head><title>404</title></head><body><h1>Page Not Found</h1></body></html>";
        }
    }else{
        throw std::runtime_error("Config file not found");
    }
    }catch(const std::exception& e){
        std::cerr << "Error loading config: " << e.what() << "\n";
        resp.status = "500 Internal Server Error";
        resp.content_type = "text/plain";
        resp.body = "Internal server error.\n";
    }


    cache_response(path, resp);
    return resp;
}

Response handle_route_with_method(const std::string& method, const std::string& path, const std::string& body) {
    Response resp;

    // Health endpoint - always GET
    if (path == "/health") {
        if (method != "GET") {
            resp.status = "405 Method Not Allowed";
            resp.content_type = "application/json";
            resp.body = "{\"error\":\"Method not allowed\"}\n";
            return resp;
        }
        resp.status = "200 OK";
        resp.content_type = "application/json";
        resp.body = "{\"status\":\"ok\"}\n";
        return resp;
    }

    // Echo endpoint for testing POST/PUT
    if (path == "/echo" && (method == "POST" || method == "PUT")) {
        resp.status = "200 OK";
        resp.content_type = "application/json";
        nlohmann::json response_json;
        response_json["method"] = method;
        response_json["received_body"] = body;
        response_json["body_length"] = body.length();
        resp.body = response_json.dump(2) + "\n";
        return resp;
    }

    // API endpoint for RESTful operations
    if (path == "/api/data") {
        if (method == "GET") {
            resp.status = "200 OK";
            resp.content_type = "application/json";
            resp.body = "{\"data\":[\"item1\",\"item2\",\"item3\"]}\n";
        } else if (method == "POST") {
            resp.status = "201 Created";
            resp.content_type = "application/json";
            nlohmann::json response_json;
            response_json["message"] = "Resource created";
            response_json["received"] = body;
            resp.body = response_json.dump(2) + "\n";
        } else if (method == "PUT") {
            resp.status = "200 OK";
            resp.content_type = "application/json";
            nlohmann::json response_json;
            response_json["message"] = "Resource updated";
            response_json["received"] = body;
            resp.body = response_json.dump(2) + "\n";
        } else if (method == "DELETE") {
            resp.status = "200 OK";
            resp.content_type = "application/json";
            resp.body = "{\"message\":\"Resource deleted\"}\n";
        } else {
            resp.status = "405 Method Not Allowed";
            resp.content_type = "application/json";
            resp.body = "{\"error\":\"Method not allowed\"}\n";
        }
        return resp;
    }

    // For GET requests, fall back to config-based routing
    if (method == "GET") {
        return handle_route(path);
    }

    // Method not supported for this path
    resp.status = "405 Method Not Allowed";
    resp.content_type = "text/plain";
    resp.body = "Method not allowed for this path\n";
    return resp;
}