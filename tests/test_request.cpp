#include "request.hpp"
#include <gtest/gtest.h>

#include <climits>
#include <string>
#include <vector>

namespace {
void rejects(const std::string &wire, unsigned status = 400) {
    try {
        parse_request(wire);
        FAIL() << "Accepted malformed request";
    } catch (const RequestError &error) {
        EXPECT_EQ(error.status, status) << error.what();
    }
}
} // namespace

TEST(RequestTest, ParsesCompleteBodyAndStripsQueryWithoutDecodingPath) {
    const auto request =
        parse_request("POST /a%20b?sort=asc HTTP/1.1\r\nHost: localhost\r\nContent-Length: "
                      "5\r\nX-Label: alpha\r\nX-Label: beta\r\n\r\nhello");
    EXPECT_EQ(request.method, "POST");
    EXPECT_EQ(request.path, "/a%20b");
    EXPECT_EQ(request.version, "HTTP/1.1");
    EXPECT_EQ(request.body, "hello");
    EXPECT_EQ(request.headers.at("x-label"), "alpha, beta");
}

TEST(RequestTest, ParsesBinaryBody) {
    std::string wire = "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\n";
    wire.append("a\0b", 3);
    EXPECT_EQ(parse_request(wire).body, std::string("a\0b", 3));
}

TEST(RequestTest, DecodesChunkedBodies) {
    const auto request =
        parse_request("POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: "
                      "chunked\r\n\r\n2\r\nab\r\n3\r\ncde\r\n0\r\n\r\n");
    EXPECT_EQ(request.body, "abcde");
}

TEST(RequestTest, Http10DoesNotRequireHost) {
    EXPECT_EQ(parse_request("GET /health HTTP/1.0\r\n\r\n").version, "HTTP/1.0");
}

TEST(RequestTest, AcceptsAbsoluteTargetsAndIpv6Host) {
    EXPECT_EQ(
        parse_request("GET http://example.com/health?x=1 HTTP/1.1\r\nHost: example.com\r\n\r\n")
            .path,
        "/health");
    EXPECT_EQ(
        parse_request("GET http://example.com?x=1 HTTP/1.1\r\nHost: example.com\r\n\r\n").path,
        "/");
    EXPECT_EQ(parse_request("GET /health HTTP/1.1\r\nHost: [::1]:8080\r\n\r\n").path, "/health");
}

TEST(RequestTest, RejectsMissingDuplicateOrInvalidHost) {
    rejects("GET / HTTP/1.1\r\n\r\n");
    rejects("GET / HTTP/1.1\r\nHost: a\r\nHost: a\r\n\r\n");
    for (const auto &host : {"", "a b", "user@host", "host/path", "host:abc", "[not-ip]",
                             "host#fragment", "host\\path"}) {
        SCOPED_TRACE(host);
        rejects("GET / HTTP/1.1\r\nHost: " + std::string(host) + "\r\n\r\n");
    }
}

TEST(RequestTest, RejectsAmbiguousFramingAndInvalidLengths) {
    for (const auto &length : {"-1", "+1", "1x", "1, 1", "1.0", "18446744073709551616"}) {
        SCOPED_TRACE(length);
        rejects("POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: " + std::string(length) +
                "\r\n\r\na");
    }
    rejects(
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n");
    rejects("POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nTransfer-Encoding: "
            "chunked\r\n\r\n0\r\n\r\n");
    rejects("POST /echo HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n");
    rejects("POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip, "
            "chunked\r\n\r\n0\r\n\r\n",
            501);
}

TEST(RequestTest, RejectsIncompleteOrTrailingMessages) {
    rejects("GET / HTTP/1.1\r\nHost: localhost\r\n");
    rejects("POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nab");
    rejects("POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nab");
    rejects("GET / HTTP/1.1\r\nHost: localhost\r\n\r\nextra");
    rejects("GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
}

TEST(RequestTest, RejectsMalformedSyntaxAndTargets) {
    rejects("GET / HTTP/1.1\nHost: localhost\n\n");
    rejects("GET / HTTP/1.1 extra\r\nHost: localhost\r\n\r\n");
    rejects("GET / HTTP/1.1\r\nHost : localhost\r\n\r\n");
    rejects("GET / HTTP/1.1\r\nBrokenHeader\r\n\r\n");
    for (const auto &target :
         {"/a#fragment", "/a%", "/a%xy", "/a\\b", "relative", "http://user@host/", "ftp://host/"}) {
        SCOPED_TRACE(target);
        rejects("GET " + std::string(target) + " HTTP/1.1\r\nHost: localhost\r\n\r\n");
    }
}

TEST(RequestTest, AppliesHeaderAndBodyLimits) {
    rejects("GET / HTTP/1.1\r\nHost: localhost\r\nX-Large: " + std::string(MAX_HEADER_SIZE, 'a') +
                "\r\n\r\n",
            431);
    rejects("POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: " +
                std::to_string(MAX_CONTENT_LENGTH + 1) + "\r\n\r\n",
            413);
}

TEST(RequestTest, RejectsUnsupportedExpectations) {
    rejects("POST /echo HTTP/1.1\r\nHost: localhost\r\nExpect: something-else\r\nContent-Length: "
            "0\r\n\r\n",
            417);
}

TEST(RequestTest, ContentLengthHelperRequiresEntireDecimalValue) {
    EXPECT_EQ(get_content_length({}), 0);
    EXPECT_EQ(get_content_length({{"content-length", "0"}}), 0);
    EXPECT_EQ(get_content_length({{"content-length", std::to_string(INT_MAX)}}), INT_MAX);
    for (const auto &value :
         {"", "-1", "+1", "1x", "1 2", "1,1", "2147483648", "9999999999999999999999"}) {
        SCOPED_TRACE(value);
        EXPECT_EQ(get_content_length({{"content-length", value}}), -1);
    }
}

TEST(RequestTest, UnsupportedVersionDiffersFromMalformedVersion) {
    rejects("GET / HTTP/1.2\r\nHost: localhost\r\n\r\n", 505);
    rejects("GET / HTTP/2.0\r\nHost: localhost\r\n\r\n", 505);
    rejects("GET / HTTP/1.1 extra\r\nHost: localhost\r\n\r\n", 400);
}

TEST(RequestTest, RejectsFieldsThatChangeRequestSemanticsInTrailers) {
    for (const auto &field :
         {"Host: another", "Content-Length: 1", "Transfer-Encoding: chunked", "Connection: close",
          "Authorization: secret", "Content-Type: text/plain", "Proxy-Connection: close",
          "Cookie: session=secret", "Proxy-Authorization: secret"}) {
        SCOPED_TRACE(field);
        for (const bool advertised : {false, true}) {
            SCOPED_TRACE(advertised);
            const auto declaration =
                advertised
                    ? "Trailer: " + std::string(field).substr(0, std::string(field).find(':')) +
                          "\r\n"
                    : "";
            rejects("POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n" +
                    declaration + "\r\n1\r\na\r\n0\r\n" + std::string(field) + "\r\n\r\n");
        }
    }
    for (const auto &declaration : {"", "Trailer: X-Checksum\r\n"}) {
        const auto request = parse_request(
            "POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n" +
            std::string(declaration) + "\r\n1\r\na\r\n0\r\nX-Checksum: test\r\n\r\n");
        EXPECT_EQ(request.body, "a");
        EXPECT_EQ(request.headers.count("x-checksum"), 0u);
    }
}

TEST(RequestTest, RejectsRawPunctuationOutsideUriPathAndQueryGrammar) {
    for (const char ch : std::string("<>\"{}[]^`|")) {
        for (const auto &prefix : {"/a", "/a?x=", "http://[::1]/a", "http://[::1]?x="}) {
            const std::string target = std::string(prefix) + ch;
            SCOPED_TRACE(target);
            rejects("GET " + target + " HTTP/1.1\r\nHost: localhost\r\n\r\n");
        }
    }
}

TEST(RequestTest, PreservesUriDelimitersPercentEncodingAndIpv6Authorities) {
    const std::string path = "/AZaz09-._~!$&'()*+,;=:@/%5B%5D";
    const std::string query = "?q=AZaz09-._~!$&'()*+,;=:@/?next=%22";
    for (const auto &prefix : {"", "http://[::1]:8080"}) {
        EXPECT_EQ(parse_request("GET " + std::string(prefix) + path + query +
                                " HTTP/1.1\r\nHost: [::1]:8080\r\n\r\n")
                      .path,
                  path);
    }
    EXPECT_EQ(parse_request("GET http://[::1]?q=%5B1%5D HTTP/1.1\r\nHost: [::1]\r\n\r\n").path,
              "/");
}

TEST(RequestTest, TrailersCannotOverwriteOrAugmentOriginalHeaderFields) {
    const auto request = parse_request(
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n"
        "Trailer: X-Label, If-Match, Digest\r\nX-Label: original\r\nIf-Match: initial\r\n\r\n"
        "1\r\na\r\n0\r\nX-Label: trailer\r\nIf-Match: trailer\r\nDigest: ignored\r\n\r\n");
    EXPECT_EQ(request.body, "a");
    EXPECT_EQ(request.headers.at("x-label"), "original");
    EXPECT_EQ(request.headers.at("if-match"), "initial");
    EXPECT_EQ(request.headers.count("digest"), 0u);
}
