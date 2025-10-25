#include "middleware.hpp"
#include <fstream>
#include <ctime>
#include <unordered_map>
#include <chrono>

std::unordered_map<std::string, std::pair<Response, std::chrono::steady_clock::time_point>> cache;
const int CACHE_TTL_SECONDS = 60; // Cache duration in seconds

void log_request(const std::string& client_ip, const std::string& method, const std::string& path) {
    std::ofstream log_file("server.log", std::ios::app);
    if (log_file) {
        log_file << "[" << std::time(nullptr) << "] " << client_ip << " - " << method << " " << path << "\n";
    }
}

Response get_cached_response(const std::string& path){
    auto it = cache.find(path);
    if(it!=cache.end()){
        auto [response, timestamp] = it->second;
        if(std::chrono::steady_clock::now()-timestamp < std::chrono::seconds(CACHE_TTL_SECONDS)){
            return response;
        }
        cache.erase(it); // Expired, remove from cache
    }
    return {};
}


void cache_response(const std::string& path, const Response& response) {
    if (cache.size() > 100) cache.clear();  // Simple size limit to prevent memory bloat
    cache[path] = {response, std::chrono::steady_clock::now()};
}