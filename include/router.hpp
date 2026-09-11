#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "response.hpp"
#include <cstddef>
#include <string>

inline constexpr std::size_t MAX_CONFIG_BYTES = 1024 * 1024;

// Atomically replace the route snapshot after fully validating the file.
// Empty path checks config.json then ../config.json, allowing a missing file.
// Explicit missing files and malformed configurations throw std::runtime_error.
void init_router_config(const std::string &path = "");

Response handle_route(const std::string &path);
Response handle_route_with_method(const std::string &method, const std::string &path,
                                  const std::string &body);

#endif
