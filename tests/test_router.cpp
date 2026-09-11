#include "router.hpp"
#include "test_support.hpp"
#include <atomic>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <thread>
#include <vector>

class RouterTest : public ::testing::Test {
  protected:
    void SetUp() override {
        config = {
            {"/test",
             {{"status", "200 OK"}, {"content_type", "text/plain"}, {"body", "Test response"}}},
            {"/json",
             {{"status", "200 OK"},
              {"content_type", "application/json"},
              {"body", "{\"test\":true}"}}}};
        reload();
    }
    void reload() {
        directory.write("config.json", config.dump());
        init_router_config((directory.path / "config.json").string());
    }
    TemporaryDirectory directory;
    nlohmann::json config;
};

TEST_F(RouterTest, ConfiguredRoutesUseTheSuppliedFile) {
    const auto result = handle_route("/test");
    EXPECT_EQ(result.status, "200 OK");
    EXPECT_EQ(result.content_type, "text/plain");
    EXPECT_EQ(result.body, "Test response");
    EXPECT_EQ(handle_route("/json").body, "{\"test\":true}");
}

TEST_F(RouterTest, HealthAndConfiguredRoutesSupportHeadAndIgnoreQueries) {
    for (const auto &path : {"/health", "/test", "/api/data"}) {
        const auto get = handle_route(path);
        const auto head = handle_route_with_method("HEAD", std::string(path) + "?x=1", "");
        EXPECT_EQ(head.status, get.status);
        EXPECT_EQ(head.body, get.body); // The HTTP serializer omits the HEAD body.
        EXPECT_EQ(head.content_type, get.content_type);
    }
    EXPECT_EQ(handle_route("/health").body, "{\"status\":\"ok\"}\n");
}

TEST_F(RouterTest, MissingRoutesRemainNotFoundForAnyMethod) {
    for (const auto &method : {"GET", "POST", "HEAD", "DELETE"}) {
        EXPECT_EQ(handle_route_with_method(method, "/nonexistent", "").status, "404 Not Found");
    }
}

TEST_F(RouterTest, UnsupportedMethodsAdvertiseResourceMethods) {
    EXPECT_EQ(handle_route_with_method("POST", "/health", "").allow, "GET, HEAD");
    EXPECT_EQ(handle_route_with_method("POST", "/test", "").allow, "GET, HEAD");
    EXPECT_EQ(handle_route_with_method("GET", "/echo", "").allow, "POST, PUT");
    EXPECT_EQ(handle_route_with_method("PATCH", "/api/data", "").allow,
              "GET, HEAD, POST, PUT, DELETE");
    EXPECT_EQ(handle_route_with_method("POST", "/test", "").status, "405 Method Not Allowed");
}

TEST_F(RouterTest, EchoPreservesTextAndOriginalByteCount) {
    const std::string body("hello\0world", 11);
    for (const auto &method : {"POST", "PUT"}) {
        const auto result = handle_route_with_method(method, "/echo", body);
        EXPECT_EQ(result.status, "200 OK");
        const auto json = nlohmann::json::parse(result.body);
        EXPECT_EQ(json["received_body"], body);
        EXPECT_EQ(json["body_length"], body.size());
        EXPECT_EQ(json["method"], method);
    }
}

TEST_F(RouterTest, EchoReplacesInvalidUtf8WithoutThrowing) {
    const auto result = handle_route_with_method("POST", "/echo", std::string("\xff", 1));
    EXPECT_EQ(result.status, "200 OK");
    const auto json = nlohmann::json::parse(result.body);
    EXPECT_EQ(json["received_body"], "\xef\xbf\xbd");
    EXPECT_EQ(json["body_length"], 1);
}

TEST_F(RouterTest, DemoApiReturnsExpectedResponsesWithoutPersistingData) {
    const auto before = handle_route("/api/data").body;
    EXPECT_EQ(handle_route_with_method("POST", "/api/data", "new").status, "201 Created");
    EXPECT_EQ(handle_route_with_method("PUT", "/api/data", "updated").status, "200 OK");
    EXPECT_EQ(handle_route_with_method("DELETE", "/api/data", "").status, "200 OK");
    EXPECT_EQ(handle_route("/api/data").body, before);
}

TEST_F(RouterTest, ReloadReplacesRoutesWithoutStaleCacheEntries) {
    ASSERT_EQ(handle_route("/test").body, "Test response");
    config["/test"]["body"] = "New response";
    config.erase("/json");
    reload();
    EXPECT_EQ(handle_route("/test").body, "New response");
    EXPECT_EQ(handle_route("/json").status, "404 Not Found");
}

TEST_F(RouterTest, RejectsInvalidSchemaWithoutReplacingExistingRoutes) {
    const auto valid = config;
    const std::vector<nlohmann::json> invalid = {
        nlohmann::json::array(),
        {{"/broken", {{"status", "200 OK"}}}},
        {{"/broken", {{"status", 200}, {"body", "text"}, {"content_type", "text/plain"}}}},
        {{"/broken",
          {{"status", "200 OK\r\nInjected: yes"}, {"body", ""}, {"content_type", "text/plain"}}}},
        {{"/broken",
          {{"status", "200 OK"}, {"body", ""}, {"content_type", "text/plain\nInjected: yes"}}}},
        {{"/broken",
          {{"status", "101 Switching Protocols"}, {"body", ""}, {"content_type", "text/plain"}}}},
        {{"/broken",
          {{"status", "204 No Content"}, {"body", "unexpected"}, {"content_type", "text/plain"}}}},
        {{"/broken", {{"status", "200 OK"}, {"body", ""}, {"content_type", ""}}}}};
    for (const auto &value : invalid) {
        config = value;
        EXPECT_ANY_THROW(reload());
        EXPECT_EQ(handle_route("/test").body, "Test response");
    }
    config = valid;
}

TEST_F(RouterTest, RejectsReservedAndInvalidRoutePaths) {
    const auto route = config["/test"];
    for (const auto &path :
         {"relative", "/bad path", "/query?x=1", "/fragment#x", "/health", "/echo", "/api/data",
          "/static/file.txt", "/bad\\path", "/bad%", "/bad%xy", "/caf\xc3\xa9"}) {
        config = {{path, route}};
        EXPECT_THROW(reload(), std::runtime_error);
    }
}

TEST_F(RouterTest, RejectsDuplicateKeysExcessiveNestingAndNonRegularConfiguration) {
    for (
        const auto &content :
        {R"({"/x":{"status":"200 OK","status":"404 Not Found","content_type":"text/plain","body":""}})",
         R"({"/x":{"status":"200 OK","content_type":"text/plain","body":""},"/x":{"status":"200 OK","content_type":"text/plain","body":""}})",
         R"({"/x":{"body":{"nested":{"deeper":true}}}})"}) {
        directory.write("invalid.json", content);
        EXPECT_THROW(init_router_config((directory.path / "invalid.json").string()),
                     std::runtime_error);
    }
    ASSERT_EQ(::mkfifo((directory.path / "fifo.json").c_str(), 0600), 0);
    EXPECT_THROW(init_router_config((directory.path / "fifo.json").string()), std::runtime_error);
    EXPECT_THROW(init_router_config(directory.path.string()), std::runtime_error);
    EXPECT_EQ(handle_route("/test").body, "Test response");
}

TEST_F(RouterTest, PercentEncodedConfigurationRoutesAreReachableWithoutDecoding) {
    config["/caf%C3%A9"] = config["/test"];
    reload();
    EXPECT_EQ(handle_route("/caf%C3%A9").body, "Test response");
}

TEST_F(RouterTest, RejectsMissingMalformedAndOversizedConfiguration) {
    EXPECT_THROW(init_router_config((directory.path / "missing.json").string()),
                 std::runtime_error);
    directory.write("broken.json", "{");
    EXPECT_ANY_THROW(init_router_config((directory.path / "broken.json").string()));
    directory.write("oversized.json", "{}");
    std::filesystem::resize_file(directory.path / "oversized.json", MAX_CONFIG_BYTES + 1);
    EXPECT_THROW(init_router_config((directory.path / "oversized.json").string()),
                 std::runtime_error);
    EXPECT_EQ(handle_route("/test").body, "Test response");
}

TEST_F(RouterTest, ConcurrentReadersSeeCompleteConfigurationSnapshots) {
    std::atomic<bool> complete{true};
    std::thread reader([&] {
        for (int i = 0; i < 100; ++i) {
            const auto result = handle_route("/test");
            if (result.status != "200 OK" ||
                (result.body != "Test response" && result.body != "Reloaded response"))
                complete = false;
        }
    });
    config["/test"]["body"] = "Reloaded response";
    reload();
    reader.join();
    EXPECT_TRUE(complete);
    EXPECT_EQ(handle_route("/test").body, "Reloaded response");
}

TEST_F(RouterTest, ConfigurationUsesTheSameUriPathGrammarAsRequests) {
    const auto route = config["/test"];
    for (const char ch : std::string("<>\"{}[]^`|")) {
        config = {{std::string("/bad") + ch, route}};
        EXPECT_THROW(reload(), std::runtime_error);
    }
    const std::string valid = "/AZaz09-._~!$&'()*+,;=:@/%5B%5D";
    config = {{valid, route}};
    reload();
    EXPECT_EQ(handle_route(valid).body, "Test response");
}
