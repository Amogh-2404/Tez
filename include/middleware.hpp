#ifndef MIDDLEWARE_HPP
#define MIDDLEWARE_HPP

#include <string>

void log_request(const std::string& client_ip, const std::string& method, const std::string& path);

#endif