#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <unordered_map>

struct Request {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

// Parse HTTP request into Request struct
Request parse_request(const std::string& raw_request);

// Extract Content-Length from headers
int get_content_length(const std::unordered_map<std::string, std::string>& headers);

#endif
