#ifndef FILE_SERVER_HPP
#define FILE_SERVER_HPP

#include "response.hpp"
#include <cstddef>
#include <string>

inline constexpr std::size_t MAX_STATIC_FILE_BYTES = 16 * 1024 * 1024;

// Pin the trusted root directory before serving requests. An empty argument
// checks static/ then ../static/; a missing implicit root disables static files.
// Explicit roots must exist. Symlinks below the root are never served.
void configure_static_root(const std::string &path = "");
Response serve_file(const std::string &path);

#endif
