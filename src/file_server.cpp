#include "file_server.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include "middleware.hpp"

namespace fs = std::filesystem;

// MIME type map for efficient lookup
static const std::unordered_map<std::string, std::string> MIME_TYPES = {
    // Text formats
    {".html", "text/html; charset=utf-8"},
    {".htm", "text/html; charset=utf-8"},
    {".css", "text/css; charset=utf-8"},
    {".txt", "text/plain; charset=utf-8"},
    {".csv", "text/csv; charset=utf-8"},

    // Application formats
    {".js", "application/javascript; charset=utf-8"},
    {".json", "application/json; charset=utf-8"},
    {".xml", "application/xml; charset=utf-8"},
    {".pdf", "application/pdf"},

    // Image formats
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".webp", "image/webp"},
    {".ico", "image/x-icon"},

    // Video/Audio formats
    {".mp4", "video/mp4"},
    {".webm", "video/webm"},
    {".mp3", "audio/mpeg"},
    {".wav", "audio/wav"},

    // Archive formats
    {".zip", "application/zip"},
    {".tar", "application/x-tar"},
    {".gz", "application/gzip"},
    {".bz2", "application/x-bzip2"},
    {".7z", "application/x-7z-compressed"},

    // Font formats
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf", "font/ttf"},
    {".eot", "application/vnd.ms-fontobject"},
    {".otf", "font/otf"}
};

// Sanitize and validate file paths to prevent directory traversal attacks
std::string sanitize_path(const std::string& filename) {
    // Remove any leading/trailing whitespace
    std::string clean = filename;
    clean.erase(0, clean.find_first_not_of(" \t\r\n"));
    clean.erase(clean.find_last_not_of(" \t\r\n") + 1);

    // Check for dangerous patterns
    if (clean.empty() ||
        clean.find("..") != std::string::npos ||
        clean.find("~") != std::string::npos ||
        clean[0] == '/' ||
        clean[0] == '\\') {
        return "";  // Invalid path
    }

    // Additional check for absolute paths or drive letters (Windows)
    if (clean.find(':') != std::string::npos) {
        return "";  // Invalid path
    }

    // Build the full path and resolve it
    try {
        fs::path base_path = fs::absolute("../static");
        fs::path requested_file = base_path / clean;
        fs::path canonical_base = fs::weakly_canonical(base_path);
        fs::path canonical_requested = fs::weakly_canonical(requested_file);

        // Ensure the resolved path is still within our static directory
        if (canonical_requested.string().find(canonical_base.string()) != 0) {
            return "";  // Path escape attempt
        }

        return canonical_requested.string();
    } catch (const std::exception&) {
        return "";  // Any filesystem error means invalid path
    }
}

// Get MIME type based on file extension
std::string get_mime_type(const std::string& path) {
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }

    std::string extension = path.substr(dot_pos);
    // Convert to lowercase for case-insensitive comparison
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    auto it = MIME_TYPES.find(extension);
    if (it != MIME_TYPES.end()) {
        return it->second;
    }

    return "application/octet-stream";  // Default for unknown types
}

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
        std::string file_path = sanitize_path(filename);

        // Check if path sanitization failed (security check)
        if (file_path.empty()) {
            resp.status = "403 Forbidden";
            resp.body = "Access denied: Invalid file path.\r\n";
            resp.content_type = "text/plain; charset=utf-8";
        } else {
            std::ifstream file(file_path, std::ios::binary);
            if (file) {
                std::string file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                resp.body = file_content;
                // Set MIME type using efficient lookup
                resp.content_type = get_mime_type(path);
            } else {
                resp.status = "404 Not Found";
                resp.body = "File not found.\r\n";
                resp.content_type = "text/plain; charset=utf-8";
            }
        }
    } else {
        resp.status = "400 Bad Request";
        resp.body = "Not a static file request.\r\n";
    }
    cache_file(path, resp);
    return resp;
}