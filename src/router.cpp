#include "router.hpp"
#include <iostream>  // For any debug if needed

Response handle_route(const std::string& path) {
    Response resp;
    resp.status = "200 OK";
    resp.content_type = "text/html; charset=utf-8";
    if (path == "/") {
        resp.body = "<html><head><title>Tez Home</title><link rel='stylesheet' href='/static/style.css'></head><body><h1>Welcome to Tez!</h1><p>Your customizable web server.</p></body></html>";
    } else if (path == "/about") {
        resp.body = "<html><head><title>About Tez</title><link rel='stylesheet' href='/static/style.css'></head><body><h1>About Tez</h1><p>A high-performance web server in C++.</p></body></html>";
    } else {
        resp.status = "404 Not Found";
        resp.body = "<html><head><title>404</title></head><body><h1>Page Not Found</h1></body></html>";
    }
    return resp;
}