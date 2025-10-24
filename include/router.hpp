#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <string>
#include "response.hpp"  // Add this

Response handle_route(const std::string& path);

#endif