#ifndef MIDDLEWARE_HPP
#define MIDDLEWARE_HPP

#include "response.hpp"
#include <chrono>
#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

// Budgets count key and response string bytes, excluding allocator overhead.
class ResponseCache {
  public:
    using Clock = std::chrono::steady_clock;

    ResponseCache(std::size_t max_entries, std::size_t max_bytes, Clock::duration ttl);
    Response get(const std::string &key, Clock::time_point now = Clock::now());
    void put(const std::string &key, const Response &response,
             Clock::time_point now = Clock::now());
    void clear();

  private:
    struct Entry {
        std::shared_ptr<const Response> response;
        Clock::time_point inserted_at;
        std::size_t bytes;
        std::list<std::string>::iterator position;
    };

    using Entries = std::unordered_map<std::string, Entry>;
    void erase(Entries::iterator entry);

    const std::size_t max_entries_;
    const std::size_t max_bytes_;
    const Clock::duration ttl_;
    std::size_t bytes_ = 0;
    std::list<std::string> order_;
    Entries entries_;
    std::mutex mutex_;
};

// Writes one escaped line to stderr. Query strings are intentionally omitted.
void log_request(const std::string &client_ip, const std::string &method, const std::string &path);
Response get_cached_response(const std::string &path);
void cache_response(const std::string &path, const Response &response);
Response get_cached_file(const std::string &key);
void cache_file(const std::string &key, const Response &response);

#endif
