#include "file_server.hpp"
#include "test_support.hpp"
#include <atomic>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

class FileServerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fs::create_directory(directory.path / "static");
        configure_static_root((directory.path / "static").string());
        directory.write("static/test.css", "body { background: blue; }");
        directory.write("static/test.html", "<html><body>Test</body></html>");
    }
    TemporaryDirectory directory;
};

TEST_F(FileServerTest, ServesConfiguredRootAndCorrectMimeTypes) {
    const auto css = serve_file("/static/test.css");
    ASSERT_EQ(css.status, "200 OK");
    EXPECT_EQ(css.body, "body { background: blue; }");
    EXPECT_EQ(css.content_type, "text/css; charset=utf-8");
    const auto html = serve_file("/static/test.html");
    ASSERT_EQ(html.status, "200 OK");
    EXPECT_EQ(html.content_type, "text/html; charset=utf-8");
}

TEST_F(FileServerTest, HandlesBinaryEmptyNestedAndEncodedNames) {
    const std::string binary("\0\x01\xff\r\n", 5);
    directory.write("static/nested/data.bin", binary);
    directory.write("static/empty", "");
    directory.write("static/a file.CSS", "css");
    directory.write("static/release..txt", "dots inside a name are safe");
    directory.write("static/~name.txt", "tilde is a literal filename");
    EXPECT_EQ(serve_file("/static/nested/data.bin").body, binary);
    EXPECT_EQ(serve_file("/static/empty").status, "200 OK");
    EXPECT_EQ(serve_file("/static/empty").body, "");
    EXPECT_EQ(serve_file("/static/empty").content_type, "application/octet-stream");
    const auto css = serve_file("/static/a%20file.CSS?version=2");
    EXPECT_EQ(css.status, "200 OK");
    EXPECT_EQ(css.content_type, "text/css; charset=utf-8");
    EXPECT_EQ(serve_file("/static/release..txt").status, "200 OK");
    EXPECT_EQ(serve_file("/static/~name.txt").status, "200 OK");
}

TEST_F(FileServerTest, MissingFilesAndNonStaticPathsHaveSpecificErrors) {
    EXPECT_EQ(serve_file("/static/nonexistent.txt").status, "404 Not Found");
    EXPECT_EQ(serve_file("/not-static/test.css").status, "400 Bad Request");
}

TEST_F(FileServerTest, RejectsTraversalHiddenFilesAndAmbiguousEncoding) {
    directory.write("secret.txt", "outside");
    directory.write("static/.env", "private");
    const std::vector<std::string> paths = {"/static/../secret.txt",
                                            "/static/%2e%2e/secret.txt",
                                            "/static/./test.css",
                                            "/static/.env",
                                            "/static/%2Eenv",
                                            "/static//test.css",
                                            "/static/",
                                            "/static/nested/",
                                            "/static/%2fsecret.txt",
                                            "/static/nested%2Fsecret.txt",
                                            "/static/nested%5csecret.txt",
                                            "/static/C:secret.txt",
                                            "/static/te%00st.css",
                                            "/static/test%0d.css",
                                            "/static/test%0A.css",
                                            "/static/%",
                                            "/static/%2",
                                            "/static/%xz",
                                            "/static/..\\secret.txt",
                                            std::string("/static/test\0.css", 17)};
    for (const auto &path : paths) {
        SCOPED_TRACE(path);
        EXPECT_EQ(serve_file(path).status, "403 Forbidden");
    }
}

TEST_F(FileServerTest, DecodesPercentEscapesExactlyOnce) {
    directory.write("static/%2e%2e.txt", "literal percent filename");
    const auto result = serve_file("/static/%252e%252e.txt");
    EXPECT_EQ(result.status, "200 OK");
    EXPECT_EQ(result.body, "literal percent filename");
}

TEST_F(FileServerTest, RejectsFinalAndIntermediateSymlinks) {
    directory.write("secret.txt", "outside");
    fs::create_symlink(directory.path / "secret.txt", directory.path / "static/link.txt");
    fs::create_directory_symlink(directory.path, directory.path / "static/linkdir");
    fs::create_symlink("test.css", directory.path / "static/internal.css");
    EXPECT_EQ(serve_file("/static/link.txt").status, "403 Forbidden");
    EXPECT_EQ(serve_file("/static/linkdir/secret.txt").status, "403 Forbidden");
    EXPECT_EQ(serve_file("/static/internal.css").status, "403 Forbidden");
}

TEST_F(FileServerTest, RejectsDirectoriesAndFifosWithoutBlocking) {
    fs::create_directory(directory.path / "static/nested");
    ASSERT_EQ(::mkfifo((directory.path / "static/pipe").c_str(), 0600), 0);
    EXPECT_EQ(serve_file("/static/nested").status, "403 Forbidden");
    EXPECT_EQ(serve_file("/static/pipe").status, "403 Forbidden");
}

TEST_F(FileServerTest, RejectsOversizedFileBeforeReadingIt) {
    directory.write("static/large.bin", "");
    fs::resize_file(directory.path / "static/large.bin", MAX_STATIC_FILE_BYTES + 1);
    EXPECT_EQ(serve_file("/static/large.bin").status, "413 Content Too Large");
}

TEST_F(FileServerTest, CachedFileChangesAndDeletionAreImmediatelyVisible) {
    ASSERT_EQ(serve_file("/static/test.css").status, "200 OK");
    directory.write("static/test.css", "changed content");
    EXPECT_EQ(serve_file("/static/test.css").body, "changed content");
    fs::remove(directory.path / "static/test.css");
    EXPECT_EQ(serve_file("/static/test.css").status, "404 Not Found");
}

TEST_F(FileServerTest, CacheDetectsSameSizeEditsAndAtomicReplacement) {
    directory.write("static/value.txt", "first");
    ASSERT_EQ(serve_file("/static/value.txt").body, "first");
    const auto original_time = fs::last_write_time(directory.path / "static/value.txt");
    directory.write("static/value.txt", "other");
    fs::last_write_time(directory.path / "static/value.txt",
                        original_time + std::chrono::seconds(1));
    EXPECT_EQ(serve_file("/static/value.txt").body, "other");
    directory.write("static/replacement.txt", "third");
    fs::last_write_time(directory.path / "static/replacement.txt", original_time);
    fs::rename(directory.path / "static/replacement.txt", directory.path / "static/value.txt");
    EXPECT_EQ(serve_file("/static/value.txt").body, "third");
}

TEST_F(FileServerTest, ReplacingCachedFileWithSymlinkCannotExposeItsOldContent) {
    ASSERT_EQ(serve_file("/static/test.css").status, "200 OK");
    fs::remove(directory.path / "static/test.css");
    directory.write("secret.txt", "outside");
    fs::create_symlink(directory.path / "secret.txt", directory.path / "static/test.css");
    EXPECT_EQ(serve_file("/static/test.css").status, "403 Forbidden");
}

TEST_F(FileServerTest, ConcurrentSymlinkReplacementNeverServesOutsideRoot) {
    directory.write("secret.txt", "outside root");
    directory.write("static/changing.txt", "inside root");
    const auto target = directory.path / "static/changing.txt";
    const auto staging = directory.path / "static/staging.txt";
    std::atomic<bool> correct{true};
    std::thread writer([&] {
        for (int i = 0; i < 24; ++i) {
            std::error_code error;
            fs::create_symlink(directory.path / "secret.txt", staging, error);
            if (error) {
                correct = false;
                return;
            }
            fs::rename(staging, target, error);
            if (error) {
                correct = false;
                return;
            }
            directory.write("static/staging.txt", "inside root");
            fs::rename(staging, target, error);
            if (error) {
                correct = false;
                return;
            }
        }
    });
    for (int i = 0; i < 48; ++i) {
        const auto result = serve_file("/static/changing.txt");
        if (result.status == "200 OK" && result.body != "inside root")
            correct = false;
    }
    writer.join();
    EXPECT_TRUE(correct);
}

TEST_F(FileServerTest, RootChangesDoNotSharePathOnlyCacheEntries) {
    EXPECT_EQ(serve_file("/static/test.css").body, "body { background: blue; }");
    directory.write("other/test.css", "other root");
    configure_static_root((directory.path / "other").string());
    EXPECT_EQ(serve_file("/static/test.css").body, "other root");
}

TEST_F(FileServerTest, PinsRootAgainstLaterPathReplacement) {
    fs::rename(directory.path / "static", directory.path / "original");
    directory.write("static/test.css", "replacement root");
    EXPECT_EQ(serve_file("/static/test.css").body, "body { background: blue; }");
}

TEST_F(FileServerTest, InvalidExplicitRootDoesNotReplaceWorkingRoot) {
    EXPECT_THROW(configure_static_root((directory.path / "missing").string()), std::system_error);
    EXPECT_EQ(serve_file("/static/test.css").status, "200 OK");
}

TEST_F(FileServerTest, ConcurrentFileReadersReturnCompleteContent) {
    std::atomic<bool> correct{true};
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&] {
            for (int j = 0; j < 16; ++j) {
                const auto result = serve_file("/static/test.css");
                if (result.status != "200 OK" || result.body != "body { background: blue; }")
                    correct = false;
            }
        });
    }
    for (auto &reader : readers)
        reader.join();
    EXPECT_TRUE(correct);
}
