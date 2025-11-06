#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <string>
#include "response.hpp"
#include "request.hpp"

Response handle_route(const std::string& path);
Response handle_route_with_method(const std::string& method, const std::string& path, const std::string& body);

#endif