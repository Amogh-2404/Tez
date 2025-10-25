#include "file_server.hpp"
#include <fstream>
#include <iostream>
#include "middleware.hpp"

Response serve_file(const std::string& path) {
    Response cached = get_cached_file(path);
    if (!cached.body.empty()) {
        return cached;
    }


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
            else if (path.find(".js") != std::string::npos) resp.content_type = "application/javascript";
            else if (path.find(".png") != std::string::npos) resp.content_type = "image/png";
            else if (path.find(".jpg") != std::string::npos) resp.content_type = "image/jpeg";
            else if (path.find(".gif") != std::string::npos) resp.content_type = "image/gif";
            else if (path.find(".svg") != std::string::npos) resp.content_type = "image/svg+xml";
            else if (path.find(".webp") != std::string::npos) resp.content_type = "image/webp";
            else if (path.find(".mp4") != std::string::npos) resp.content_type = "video/mp4";
            else if (path.find(".mp3") != std::string::npos) resp.content_type = "audio/mpeg";
            else if (path.find(".wav") != std::string::npos) resp.content_type = "audio/wav";
            else if (path.find(".pdf") != std::string::npos) resp.content_type = "application/pdf";
            else if (path.find(".zip") != std::string::npos) resp.content_type = "application/zip";
            else if (path.find(".tar") != std::string::npos) resp.content_type = "application/x-tar";
            else if (path.find(".gz") != std::string::npos) resp.content_type = "application/gzip";
            else if (path.find(".bz2") != std::string::npos) resp.content_type = "application/x-bzip2";
            else if (path.find(".7z") != std::string::npos) resp.content_type = "application/x-7z-compressed";
            else if (path.find(".json") != std::string::npos) resp.content_type = "application/json";
            else if (path.find(".xml") != std::string::npos) resp.content_type = "application/xml";
            else if (path.find(".csv") != std::string::npos) resp.content_type = "text/csv";
            else if (path.find(".txt") != std::string::npos) resp.content_type = "text/plain";
            else if (path.find(".ico") != std::string::npos) resp.content_type = "image/x-icon";
            else if (path.find(".woff") != std::string::npos) resp.content_type = "application/font-woff";
            else if (path.find(".woff2") != std::string::npos) resp.content_type = "application/font-woff2";
            else if (path.find(".ttf") != std::string::npos) resp.content_type = "application/font-ttf";
            else if (path.find(".eot") != std::string::npos) resp.content_type = "application/vnd.ms-fontobject";
            else if (path.find(".otf") != std::string::npos) resp.content_type = "application/font-otf";
            else if (path.find(".webp") != std::string::npos) resp.content_type = "image/webp";
            else if (path.find(".webm") != std::string::npos) resp.content_type = "video/webm";
            else if (path.find(".webp") != std::string::npos) resp.content_type = "image/webp";
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
    cache_file(path, resp);
    return resp;
}