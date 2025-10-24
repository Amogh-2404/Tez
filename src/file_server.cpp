#include "file_server.hpp"
#include <fstream>
#include <iostream>

Response serve_file(const std::string& path) {
    Response resp;
    resp.status = "200 OK";
    resp.content_type = "text/plain; charset=utf-8";
    if (path.substr(0, 8) == "/static/") {
        std::string filename = path.substr(8);
        std::string file_path = "../static/" + filename;
        std::ifstream file(file_path, std::ios::binary);
        if (file) {
            std::string file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            resp.body = file_content;
            // Set MIME type
            if (path.find(".css") != std::string::npos) resp.content_type = "text/css";
            else if (path.find(".html") != std::string::npos) resp.content_type = "text/html";
            else resp.content_type = "application/octet-stream";
        } else {
            resp.status = "404 Not Found";
            resp.body = "File not found.\r\n";
            resp.content_type = "text/plain";
        }
    } else {
        resp.status = "400 Bad Request";
        resp.body = "Not a static file request.\r\n";
    }
    return resp;
}