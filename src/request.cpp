#include "request.hpp"

#include <algorithm>
#include <array>
#include <boost/asio/ip/address.hpp>
#include <charconv>
#include <climits>
#include <cstdint>
#include <string_view>

namespace {
namespace http = boost::beast::http;

constexpr std::array<http::field, 17> forbidden_trailer_fields = {http::field::host,
                                                                  http::field::content_length,
                                                                  http::field::transfer_encoding,
                                                                  http::field::connection,
                                                                  http::field::proxy_connection,
                                                                  http::field::keep_alive,
                                                                  http::field::authorization,
                                                                  http::field::proxy_authorization,
                                                                  http::field::cookie,
                                                                  http::field::content_type,
                                                                  http::field::content_encoding,
                                                                  http::field::content_range,
                                                                  http::field::expect,
                                                                  http::field::trailer,
                                                                  http::field::upgrade,
                                                                  http::field::te,
                                                                  http::field::range};

bool ascii_digit(char ch) {
    return ch >= '0' && ch <= '9';
}
bool ascii_hex(char ch) {
    return ascii_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}
bool ascii_alpha(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool valid_path_or_query(std::string_view value, bool query) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        // RFC 3986: pchar = unreserved / pct-encoded / sub-delims / ":" / "@".
        // Paths additionally contain "/"; queries additionally allow "/" and "?".
        if (ascii_alpha(ch) || ascii_digit(ch) ||
            std::string_view("-._~!$&'()*+,;=:@/").find(ch) != std::string_view::npos ||
            (query && ch == '?'))
            continue;
        if (ch == '%' && i + 2 < value.size() && ascii_hex(value[i + 1]) &&
            ascii_hex(value[i + 2])) {
            i += 2;
            continue;
        }
        return false;
    }
    return true;
}

bool valid_authority(std::string_view authority) {
    if (authority.empty())
        return false;
    std::string_view port;
    if (authority.front() == '[') {
        const auto end = authority.find(']');
        if (end == std::string_view::npos)
            return false;
        boost::system::error_code ec;
        const auto literal = authority.substr(1, end - 1);
        if (literal.find('%') != std::string_view::npos)
            return false;
        boost::asio::ip::make_address_v6(std::string(literal), ec);
        if (ec)
            return false;
        const auto rest = authority.substr(end + 1);
        if (rest.empty())
            return true;
        if (rest.front() != ':')
            return false;
        port = rest.substr(1);
    } else {
        const auto colon = authority.find(':');
        const auto host = authority.substr(0, colon);
        if (host.empty())
            return false;
        for (std::size_t i = 0; i < host.size(); ++i) {
            const char ch = host[i];
            if (ascii_alpha(ch) || ascii_digit(ch) ||
                std::string_view("-._~!$&'()*+,;=").find(ch) != std::string_view::npos)
                continue;
            if (ch == '%' && i + 2 < host.size() && ascii_hex(host[i + 1]) &&
                ascii_hex(host[i + 2])) {
                i += 2;
                continue;
            }
            return false;
        }
        if (colon == std::string_view::npos)
            return true;
        port = authority.substr(colon + 1);
    }
    // An empty port is valid URI syntax (e.g. "localhost:").
    return std::all_of(port.begin(), port.end(), ascii_digit);
}

std::string request_path(const http::request_header<> &request) {
    const auto raw = request.target();
    std::string_view target(raw.data(), raw.size());
    for (std::size_t i = 0; i < target.size(); ++i) {
        const auto ch = static_cast<unsigned char>(target[i]);
        if (ch <= 0x20 || ch >= 0x7f || ch == '#' || ch == '\\')
            throw RequestError(400, "Invalid request target");
        if (ch == '%') {
            if (i + 2 >= target.size() || !ascii_hex(target[i + 1]) || !ascii_hex(target[i + 2]))
                throw RequestError(400, "Invalid percent encoding");
            i += 2;
        }
    }
    if (request.method() == http::verb::connect) {
        if (!valid_authority(target) || target.find(':') == std::string_view::npos)
            throw RequestError(400, "Invalid CONNECT target");
        return std::string(target);
    }
    if (target == "*" && request.method() == http::verb::options)
        return "*";
    // Origin servers must also accept absolute-form request targets (RFC 9112, 3.2.2).
    const auto scheme_end = target.find("://");
    if (scheme_end != std::string_view::npos && target.front() != '/') {
        const auto scheme = target.substr(0, scheme_end);
        if (!boost::beast::iequals(boost::beast::string_view(scheme.data(), scheme.size()),
                                   "http") &&
            !boost::beast::iequals(boost::beast::string_view(scheme.data(), scheme.size()),
                                   "https"))
            throw RequestError(400, "Unsupported target scheme");
        target.remove_prefix(scheme_end + 3);
        const auto authority_end = target.find_first_of("/?");
        if (!valid_authority(target.substr(0, authority_end)))
            throw RequestError(400, "Invalid target authority");
        if (authority_end == std::string_view::npos)
            return "/";
        target.remove_prefix(authority_end);
        if (target.front() == '?') {
            if (!valid_path_or_query(target.substr(1), true))
                throw RequestError(400, "Invalid request query");
            return "/";
        }
    }
    if (target.empty() || target.front() != '/')
        throw RequestError(400, "Expected an origin-form request target");
    const auto query = target.find('?');
    const auto path = target.substr(0, query);
    if (!valid_uri_path(path) ||
        (query != std::string_view::npos && !valid_path_or_query(target.substr(query + 1), true)))
        throw RequestError(400, "Invalid request path or query");
    return std::string(path);
}

} // namespace

#if BOOST_BEAST_VERSION >= 359
void RequestParser::on_trailer_field_impl(boost::beast::http::field name, boost::beast::string_view,
                                          boost::beast::string_view,
                                          boost::system::error_code &error) {
    if (std::find(forbidden_trailer_fields.begin(), forbidden_trailer_fields.end(), name) !=
        forbidden_trailer_fields.end()) {
        error = http::error::bad_field;
        return;
    }
    // Tez does not consume trailer metadata. RFC 9112 section 7.1.2 prohibits
    // merging arbitrary trailers into the initial header section, so discard them.
}
#endif

bool valid_uri_path(std::string_view path) {
    return !path.empty() && path.front() == '/' && valid_path_or_query(path, false);
}

unsigned request_error_status(const boost::system::error_code &ec, const std::string &unconsumed) {
    if (ec == http::error::header_limit)
        return 431;
    if (ec == http::error::body_limit)
        return 413;
    if (ec == http::error::bad_version) {
        const auto end = unconsumed.find("\r\n");
        const auto line = unconsumed.substr(0, end);
        const auto space = line.rfind(' ');
        const auto version = line.substr(space == std::string::npos ? 0 : space + 1);
        if (version.size() == 8 && version.compare(0, 5, "HTTP/") == 0 && ascii_digit(version[5]) &&
            version[6] == '.' && ascii_digit(version[7]) && version != "HTTP/1.0" &&
            version != "HTTP/1.1")
            return 505;
    }
    return 400;
}

void validate_request_trailers(const boost::beast::http::request_header<> &request,
                               const boost::beast::http::request_header<> &initial) {
    for (const auto field : forbidden_trailer_fields) {
        if (request.count(field) != initial.count(field))
            throw RequestError(400, "Forbidden request trailer field");
    }
}

void validate_request_header(const boost::beast::http::request_header<> &request) {
    namespace http = boost::beast::http;
    if (request.version() != 10 && request.version() != 11)
        throw RequestError(505, "HTTP version not supported");
    if (request.count(http::field::host) > 1 ||
        (request.version() == 11 && request.count(http::field::host) != 1))
        throw RequestError(400, "Exactly one Host header is required");
    if (request.count(http::field::host) != 0) {
        const auto host = request[http::field::host];
        if (!valid_authority(std::string_view(host.data(), host.size())))
            throw RequestError(400, "Invalid Host header");
    }
    if (request.count(http::field::content_length) > 1)
        throw RequestError(400, "Duplicate Content-Length");
    if (request.count(http::field::content_length)) {
        const auto length = request[http::field::content_length];
        if (length.empty() || !std::all_of(length.begin(), length.end(), ascii_digit))
            throw RequestError(400, "Invalid Content-Length");
    }
    if (request.count(http::field::transfer_encoding)) {
        if (request.version() == 10 || request.count(http::field::content_length))
            throw RequestError(400, "Ambiguous message framing");
        if (request.count(http::field::transfer_encoding) != 1 ||
            !boost::beast::iequals(request[http::field::transfer_encoding], "chunked"))
            throw RequestError(501, "Transfer coding not supported");
    }
    if (request.count(http::field::expect)) {
        if (request.version() == 10 || request.count(http::field::expect) != 1 ||
            !boost::beast::iequals(request[http::field::expect], "100-continue"))
            throw RequestError(417, "Expectation failed");
    }
    request_path(request);
}

Request make_request(const boost::beast::http::request_header<> &initial_header,
                     std::string &&body) {
    validate_request_header(initial_header);
    Request request;
    request.method = std::string(initial_header.method_string());
    request.path = request_path(initial_header);
    request.version = initial_header.version() == 11 ? "HTTP/1.1" : "HTTP/1.0";
    for (const auto &field : initial_header) {
        std::string name(field.name_string());
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
            return static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch);
        });
        auto [it, inserted] = request.headers.emplace(std::move(name), std::string(field.value()));
        if (!inserted) {
            it->second += ", ";
            it->second += std::string(field.value());
        }
    }
    request.body = std::move(body);
    return request;
}

Request parse_request(const std::string &raw_request) {
    namespace http = boost::beast::http;
    RequestParser parser;
    parser.header_limit(MAX_HEADER_SIZE);
    parser.body_limit(MAX_CONTENT_LENGTH);
    boost::system::error_code ec;
    const auto header_bytes = parser.put(boost::asio::buffer(raw_request), ec);
    if (ec || !parser.is_header_done())
        throw RequestError(request_error_status(ec, raw_request),
                           "Invalid or incomplete HTTP headers");
    validate_request_header(parser.get().base());
    const auto initial_header = parser.get().base();
    parser.eager(true);
    auto consumed = header_bytes;
    if (!parser.is_done()) {
        consumed += parser.put(
            boost::asio::buffer(raw_request.data() + consumed, raw_request.size() - consumed), ec);
        if (ec || !parser.is_done())
            throw RequestError(request_error_status(ec), "Invalid or incomplete HTTP body");
    }
    if (consumed != raw_request.size())
        throw RequestError(400, "Trailing data after request");
    validate_request_trailers(parser.get().base(), initial_header);
    return make_request(initial_header, std::move(parser.get().body()));
}

int get_content_length(const std::unordered_map<std::string, std::string> &headers) {
    const auto it = headers.find("content-length");
    if (it == headers.end())
        return 0;
    const auto &value = it->second;
    if (value.empty() || !std::all_of(value.begin(), value.end(), ascii_digit))
        return -1;
    int result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        return -1;
    return result;
}
