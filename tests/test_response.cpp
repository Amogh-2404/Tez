#include <gtest/gtest.h>
#include "../include/response.hpp"

TEST(ResponseTest, StructInitialization) {
    Response resp;
    resp.status = "200 OK";
    resp.content_type = "text/html";
    resp.body = "<html></html>";

    EXPECT_EQ(resp.status, "200 OK");
    EXPECT_EQ(resp.content_type, "text/html");
    EXPECT_EQ(resp.body, "<html></html>");
}

TEST(ResponseTest, EmptyResponse) {
    Response resp;
    EXPECT_TRUE(resp.status.empty());
    EXPECT_TRUE(resp.content_type.empty());
    EXPECT_TRUE(resp.body.empty());
}

TEST(ResponseTest, CopyResponse) {
    Response resp1;
    resp1.status = "404 Not Found";
    resp1.content_type = "text/plain";
    resp1.body = "Not found";

    Response resp2 = resp1;
    EXPECT_EQ(resp2.status, resp1.status);
    EXPECT_EQ(resp2.content_type, resp1.content_type);
    EXPECT_EQ(resp2.body, resp1.body);
}
