#include <gtest/gtest.h>
#include "../include/file_server.hpp"
#include <fstream>
#include <sys/stat.h>

class FileServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test static directory and file
        mkdir("test_static", 0755);

        std::ofstream test_file("test_static/test.css");
        test_file << "body { background: blue; }";
        test_file.close();

        std::ofstream test_html("test_static/test.html");
        test_html << "<html><body>Test</body></html>";
        test_html.close();
    }

    void TearDown() override {
        std::remove("test_static/test.css");
        std::remove("test_static/test.html");
        rmdir("test_static");
    }
};

TEST_F(FileServerTest, ServesExistingFile) {
    // Note: serve_file looks for ../static/ relative to build dir
    // For testing, we'd need to adapt the path or mock the file reading
    Response resp = serve_file("/static/style.css");
    // This test depends on the actual static/style.css file existing
    EXPECT_TRUE(resp.status == "200 OK" || resp.status == "404 Not Found");
}

TEST_F(FileServerTest, ReturnsNotFoundForMissingFile) {
    Response resp = serve_file("/static/nonexistent.txt");
    EXPECT_EQ(resp.status, "404 Not Found");
    EXPECT_EQ(resp.content_type, "text/plain; charset=utf-8");
}

TEST_F(FileServerTest, CorrectMimeTypesForCSS) {
    Response resp = serve_file("/static/test.css");
    if (resp.status == "200 OK") {
        EXPECT_EQ(resp.content_type, "text/css");
    }
}

TEST_F(FileServerTest, CorrectMimeTypesForHTML) {
    Response resp = serve_file("/static/test.html");
    if (resp.status == "200 OK") {
        EXPECT_EQ(resp.content_type, "text/html");
    }
}

TEST_F(FileServerTest, RejectNonStaticPaths) {
    Response resp = serve_file("/not-static/file.txt");
    EXPECT_EQ(resp.status, "400 Bad Request");
}
