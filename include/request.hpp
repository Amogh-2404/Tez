#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

inline constexpr std::size_t MAX_HEADER_SIZE = 8 * 1024;
inline constexpr std::size_t DEFAULT_BODY_LIMIT = 1024 * 1024;
inline constexpr std::size_t MAX_CONTENT_LENGTH = 10 * 1024 * 1024;
inline constexpr std::size_t MAX_KEEPALIVE_REQUESTS = 1000;

// Beast 359 (Boost 1.90) split trailer callbacks from ordinary fields. Inspect
// trailers before its message parser can discard fields that Tez must reject.
#if BOOST_BEAST_VERSION >= 359
class RequestParser : public boost::beast::http::request_parser<boost::beast::http::string_body> {
  private:
    void on_trailer_field_impl(boost::beast::http::field name,
                               boost::beast::string_view name_string,
                               boost::beast::string_view value,
                               boost::system::error_code &error) override;
};
#else
using RequestParser = boost::beast::http::request_parser<boost::beast::http::string_body>;
#endif

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

// Build from the original header section and a completed body. Permitted trailers
// are ignored rather than merged into application headers (RFC 9112, section 7.1.2).
Request make_request(const boost::beast::http::request_header<> &initial_header,
                     std::string &&body);

// Parse exactly one complete message; rejects malformed, incomplete, or trailing data.
Request parse_request(const std::string &raw_request);

// Extract a strictly decimal Content-Length, or -1 if invalid/out of int range.
int get_content_length(const std::unordered_map<std::string, std::string> &headers);

#endif
