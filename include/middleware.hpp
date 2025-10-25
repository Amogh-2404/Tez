#ifndef MIDDLEWARE_HPP
#define MIDDLEWARE_HPP
#include "response.hpp"
#include <chrono>
#include <unordered_map>
#include <fstream>
#include <ctime>
#include <string>

void log_request(const std::string& client_ip, const std::string& method, const std::string& path);
Response get_cached_response(const std::string& path);
void cache_response(const std::string& path, const Response& response);
Response get_cached_file(const std::string& path);  // New for static files
void cache_file(const std::string& path, const Response& response);  // New for static files

#endif