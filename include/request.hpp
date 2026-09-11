#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <boost/beast/http.hpp>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

inline constexpr std::size_t MAX_HEADER_SIZE = 8 * 1024;
inline constexpr std::size_t DEFAULT_BODY_LIMIT = 1024 * 1024;
inline constexpr std::size_t MAX_CONTENT_LENGTH = 10 * 1024 * 1024;
inline constexpr std::size_t MAX_KEEPALIVE_REQUESTS = 1000;

struct Request {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

class RequestError : public std::runtime_error {
  public:
    RequestError(unsigned status, const std::string &message)
        : std::runtime_error(message), status(status) {}
    unsigned status;
};

// RFC 3986 path characters, with a leading slash and no query or fragment.
bool valid_uri_path(std::string_view path);

// Validate semantics not checked by Beast's wire parser. Throws RequestError.
void validate_request_header(const boost::beast::http::request_header<> &request);

// Trailers may not introduce fields that affect routing, framing, or authentication.
void validate_request_trailers(const boost::beast::http::request_header<> &request,
                               const boost::beast::http::request_header<> &initial);

unsigned request_error_status(const boost::system::error_code &error,
                              const std::string &unconsumed = "");

// Move a complete, validated Beast message into the application representation.
Request make_request(boost::beast::http::request<boost::beast::http::string_body> &&request);

// Parse exactly one complete message; rejects malformed, incomplete, or trailing data.
Request parse_request(const std::string &raw_request);

// Extract a strictly decimal Content-Length, or -1 if invalid/out of int range.
int get_content_length(const std::unordered_map<std::string, std::string> &headers);

#endif
