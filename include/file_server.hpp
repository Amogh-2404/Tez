#ifndef FILE_SERVER_HPP
#define FILE_SERVER_HPP

#include <string>
#include "response.hpp"  // Add this

Response serve_file(const std::string& path);

#endif