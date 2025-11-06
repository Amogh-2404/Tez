#include "request.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

Request parse_request(const std::string& raw_request) {
    Request req;
    std::istringstream stream(raw_request);
    std::string line;

    // Parse request line
    if (std::getline(stream, line)) {
        // Remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream request_line(line);
        request_line >> req.method >> req.path >> req.version;
    }

    // Parse headers
    while (std::getline(stream, line) && line != "\r") {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) break;

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // Trim whitespace
            auto ltrim = [](std::string& s) {
                s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) {
                    return !std::isspace(c);
                }));
            };
            auto rtrim = [](std::string& s) {
                s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) {
                    return !std::isspace(c);
                }).base(), s.end());
            };

            ltrim(value);
            rtrim(value);

            // Store header in lowercase for case-insensitive lookup
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
                return std::tolower(c);
            });

            req.headers[key] = value;
        }
    }

    // Body is everything after headers
    std::string remaining((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    req.body = remaining;

    return req;
}

int get_content_length(const std::unordered_map<std::string, std::string>& headers) {
    auto it = headers.find("content-length");
    if (it != headers.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return 0;
        }
    }
    return 0;
}
