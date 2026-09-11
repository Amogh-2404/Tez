#include "middleware.hpp"
#include <ctime>
#include <iostream>
#include <string_view>

namespace {
ResponseCache response_cache(100, 8 * 1024 * 1024, std::chrono::seconds(60));
ResponseCache file_cache(50, 32 * 1024 * 1024, std::chrono::seconds(60));
std::mutex log_mutex;

std::string escape_log_field(std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    constexpr std::size_t limit = 2048;
    std::string escaped;
    for (std::size_t i = 0; i < value.size() && i < limit; ++i) {
        const auto c = static_cast<unsigned char>(value[i]);
        if (c <= 0x20 || c >= 0x7f || c == '\\') {
            escaped += "\\x";
            escaped += hex[c >> 4];
            escaped += hex[c & 0xf];
        } else {
            escaped += static_cast<char>(c);
        }
    }
    if (value.size() > limit)
        escaped += "...";
    return escaped;
}
} // namespace

ResponseCache::ResponseCache(std::size_t max_entries, std::size_t max_bytes, Clock::duration ttl)
    : max_entries_(max_entries), max_bytes_(max_bytes), ttl_(ttl) {}

void ResponseCache::erase(Entries::iterator entry) {
    bytes_ -= entry->second.bytes;
    order_.erase(entry->second.position);
    entries_.erase(entry);
}

Response ResponseCache::get(const std::string &key, Clock::time_point now) {
    std::shared_ptr<const Response> response;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto entry = entries_.find(key);
        if (entry == entries_.end())
            return {};
        if (now - entry->second.inserted_at >= ttl_) {
            erase(entry);
            return {};
        }
        order_.splice(order_.begin(), order_, entry->second.position);
        response = entry->second.response;
    }
    // Large response copies do not hold the cache's mutex.
    return *response;
}

void ResponseCache::put(const std::string &key, const Response &response, Clock::time_point now) {
    std::size_t bytes = 0;
    bool fits = max_entries_ != 0 && ttl_ > Clock::duration::zero();
    for (const auto *field :
         {&key, &response.status, &response.content_type, &response.body, &response.allow}) {
        if (field->size() > max_bytes_ - bytes) {
            fits = false;
            break;
        }
        bytes += field->size();
    }
    // Build the value before locking or replacing a valid entry.
    const auto value = fits ? std::make_shared<const Response>(response) : nullptr;
    std::lock_guard<std::mutex> lock(mutex_);
    const auto old = entries_.find(key);
    if (old != entries_.end())
        erase(old);
    if (!fits)
        return;

    // Expired entries should not force eviction of live, less recent entries.
    for (auto entry = entries_.begin(); entry != entries_.end();) {
        if (now - entry->second.inserted_at >= ttl_) {
            const auto expired = entry++;
            erase(expired);
        } else {
            ++entry;
        }
    }
    while (entries_.size() >= max_entries_ || bytes > max_bytes_ - bytes_) {
        erase(entries_.find(order_.back()));
    }
    order_.push_front(key);
    try {
        entries_.emplace(key, Entry{value, now, bytes, order_.begin()});
    } catch (...) {
        order_.pop_front();
        throw;
    }
    bytes_ += bytes;
}

void ResponseCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    order_.clear();
    bytes_ = 0;
}

void log_request(const std::string &client_ip, const std::string &method, const std::string &path) {
    const auto pathname = std::string_view(path).substr(0, path.find('?'));
    const auto line = "[" + std::to_string(std::time(nullptr)) + "] " +
                      escape_log_field(client_ip) + " - " + escape_log_field(method) + " " +
                      escape_log_field(pathname) + "\n";
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cerr << line;
}

Response get_cached_response(const std::string &path) {
    return response_cache.get(path);
}

void cache_response(const std::string &path, const Response &response) {
    response_cache.put(path, response);
}

Response get_cached_file(const std::string &key) {
    return file_cache.get(key);
}

void cache_file(const std::string &key, const Response &response) {
    file_cache.put(key, response);
}
