#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>

struct Response {
    std::string status;
    std::string content_type;
    std::string body;
};

#endif