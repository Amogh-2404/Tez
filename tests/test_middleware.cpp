#include "middleware.hpp"
#include <atomic>
#include <gtest/gtest.h>
#include <sstream>
#include <thread>
#include <vector>

namespace {
Response make_response(const std::string &body) {
    Response result;
    result.status = "200 OK";
    result.content_type = "text/plain";
    result.body = body;
    return result;
}
using Clock = ResponseCache::Clock;
const auto epoch = Clock::time_point{};
} // namespace

TEST(MiddlewareTest, LoggingEscapesControlsAndOmitsQuery) {
    testing::internal::CaptureStderr();
    log_request("127.0.0.1", "GET", "/test\r\nforged\t\\line?token=secret");
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("127.0.0.1 - GET /test\\x0d\\x0aforged\\x09\\x5cline\n"),
              std::string::npos);
    EXPECT_EQ(output.find("secret"), std::string::npos);
    EXPECT_EQ(output.find('\n'), output.size() - 1);
}

TEST(MiddlewareTest, ConcurrentLoggingKeepsCompleteLines) {
    testing::internal::CaptureStderr();
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([i] {
            for (int j = 0; j < 12; ++j)
                log_request("client" + std::to_string(i), "GET", "/test");
        });
    }
    for (auto &worker : workers)
        worker.join();
    std::istringstream output(testing::internal::GetCapturedStderr());
    int lines = 0;
    for (std::string line; std::getline(output, line);) {
        EXPECT_NE(line.find(" - GET /test"), std::string::npos);
        EXPECT_EQ(line.find('['), 0u);
        ++lines;
    }
    EXPECT_EQ(lines, 48);
}

TEST(ResponseCacheTest, StoresIndependentCopy) {
    ResponseCache cache(2, 1024, std::chrono::seconds(60));
    auto value = make_response("content");
    cache.put("a", value, epoch);
    value.body = "changed";
    auto cached = cache.get("a", epoch);
    EXPECT_EQ(cached.body, "content");
    cached.body = "modified result";
    EXPECT_EQ(cache.get("a", epoch).body, "content");
}

TEST(ResponseCacheTest, ExpiresAtTtlWithoutRefreshingOnRead) {
    ResponseCache cache(2, 1024, std::chrono::seconds(60));
    cache.put("a", make_response("content"), epoch);
    EXPECT_EQ(cache.get("a", epoch + std::chrono::seconds(59)).body, "content");
    EXPECT_TRUE(cache.get("a", epoch + std::chrono::seconds(60)).status.empty());
}

TEST(ResponseCacheTest, EvictsLeastRecentlyUsedEntry) {
    ResponseCache cache(2, 1024, std::chrono::seconds(60));
    cache.put("a", make_response("a"), epoch);
    cache.put("b", make_response("b"), epoch);
    EXPECT_EQ(cache.get("a", epoch).body, "a");
    cache.put("c", make_response("c"), epoch);
    EXPECT_TRUE(cache.get("b", epoch).status.empty());
    EXPECT_EQ(cache.get("a", epoch).body, "a");
    EXPECT_EQ(cache.get("c", epoch).body, "c");
}

TEST(ResponseCacheTest, ByteBudgetIncludesKeyAndResponseMetadata) {
    // One-character key + status (6) + content type (10) + body (3) = 20.
    ResponseCache cache(10, 40, std::chrono::seconds(60));
    cache.put("a", make_response("aaa"), epoch);
    cache.put("b", make_response("bbb"), epoch);
    cache.put("c", make_response("ccc"), epoch);
    EXPECT_TRUE(cache.get("a", epoch).status.empty());
    EXPECT_EQ(cache.get("b", epoch).body, "bbb");
    EXPECT_EQ(cache.get("c", epoch).body, "ccc");
}

TEST(ResponseCacheTest, ReplacementReclaimsBudgetAndRefreshesTtl) {
    ResponseCache cache(10, 40, std::chrono::seconds(60));
    cache.put("a", make_response("aaa"), epoch);
    cache.put("a", make_response("AAA"), epoch + std::chrono::seconds(30));
    cache.put("b", make_response("bbb"), epoch + std::chrono::seconds(30));
    EXPECT_EQ(cache.get("a", epoch + std::chrono::seconds(60)).body, "AAA");
    EXPECT_EQ(cache.get("b", epoch + std::chrono::seconds(60)).body, "bbb");
}

TEST(ResponseCacheTest, OversizedReplacementInvalidatesOldValue) {
    ResponseCache cache(2, 40, std::chrono::seconds(60));
    cache.put("a", make_response("old"), epoch);
    cache.put("a", make_response(std::string(41, 'x')), epoch);
    EXPECT_TRUE(cache.get("a", epoch).status.empty());
    cache.put("b", make_response("new"), epoch);
    EXPECT_EQ(cache.get("b", epoch).body, "new");
}

TEST(ResponseCacheTest, ExpiredRecentEntryDoesNotEvictLiveEntry) {
    ResponseCache cache(2, 1024, std::chrono::seconds(60));
    cache.put("old", make_response("old"), epoch);
    cache.put("live", make_response("live"), epoch + std::chrono::seconds(30));
    cache.get("old", epoch + std::chrono::seconds(59));
    cache.put("new", make_response("new"), epoch + std::chrono::seconds(60));
    EXPECT_EQ(cache.get("live", epoch + std::chrono::seconds(60)).body, "live");
    EXPECT_TRUE(cache.get("old", epoch + std::chrono::seconds(60)).status.empty());
}

TEST(ResponseCacheTest, ZeroLimitsDisableCachingAndClearReclaimsBudget) {
    for (const auto limit : {0u, 1u}) {
        ResponseCache disabled(limit, 0, std::chrono::seconds(60));
        disabled.put("a", make_response("a"), epoch);
        EXPECT_TRUE(disabled.get("a", epoch).status.empty());
    }
    ResponseCache disabled(0, 1024, std::chrono::seconds(60));
    disabled.put("a", make_response("a"), epoch);
    EXPECT_TRUE(disabled.get("a", epoch).status.empty());
    ResponseCache cache(1, 20, std::chrono::seconds(60));
    cache.put("a", make_response("aaa"), epoch);
    cache.clear();
    EXPECT_TRUE(cache.get("a", epoch).status.empty());
    cache.put("b", make_response("bbb"), epoch);
    EXPECT_EQ(cache.get("b", epoch).body, "bbb");
}

TEST(ResponseCacheTest, ConcurrentReadersAndWritersRemainConsistent) {
    ResponseCache cache(16, 4096, std::chrono::seconds(60));
    std::atomic<bool> consistent{true};
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&, i] {
            for (int j = 0; j < 40; ++j) {
                const auto key = std::to_string((i + j) % 8);
                cache.put(key, make_response(key));
                const auto value = cache.get(key);
                if (!value.status.empty() && value.body != key)
                    consistent = false;
            }
        });
    }
    for (auto &worker : workers)
        worker.join();
    EXPECT_TRUE(consistent);
}

TEST(MiddlewareTest, FileAndResponseCachesAreIndependent) {
    cache_response("isolation", make_response("response"));
    cache_file("isolation", make_response("file"));
    EXPECT_EQ(get_cached_response("isolation").body, "response");
    EXPECT_EQ(get_cached_file("isolation").body, "file");
}
