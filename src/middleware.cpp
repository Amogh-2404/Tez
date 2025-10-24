#include "middleware.hpp"
#include <fstream>
#include <ctime>

void log_request(const std::string& client_ip, const std::string& method, const std::string& path) {
    std::ofstream log_file("server.log", std::ios::app);
    if (log_file) {
        log_file << "[" << std::time(nullptr) << "] " << client_ip << " - " << method << " " << path << "\n";
    }
}