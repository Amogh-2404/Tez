#include <gtest/gtest.h>
#include "../include/router.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

class RouterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test config file
        nlohmann::json test_config;
        test_config["/test"] = {
            {"status", "200 OK"},
            {"content_type", "text/plain"},
            {"body", "Test response"}
        };
        test_config["/json"] = {
            {"status", "200 OK"},
            {"content_type", "application/json"},
            {"body", "{\"test\":true}"}
        };

        std::ofstream config_file("test_config.json");
        config_file << test_config.dump(4);
        config_file.close();
    }

    void TearDown() override {
        std::remove("test_config.json");
    }
};

TEST_F(RouterTest, HealthEndpoint) {
    Response resp = handle_route("/health");
    EXPECT_EQ(resp.status, "200 OK");
    EXPECT_EQ(resp.content_type, "application/json");
    EXPECT_EQ(resp.body, "{\"status\":\"ok\"}\n");
}

TEST_F(RouterTest, NotFoundRoute) {
    Response resp = handle_route("/nonexistent");
    EXPECT_EQ(resp.status, "404 Not Found");
    EXPECT_EQ(resp.content_type, "text/html; charset=utf-8");
    EXPECT_NE(resp.body.find("Page Not Found"), std::string::npos);
}

TEST_F(RouterTest, CachingWorks) {
    // First call - should load from config
    Response resp1 = handle_route("/health");

    // Second call - should come from cache
    Response resp2 = handle_route("/health");

    EXPECT_EQ(resp1.body, resp2.body);
    EXPECT_EQ(resp1.status, resp2.status);
}
