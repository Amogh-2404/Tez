#include "middleware.hpp"
#include <fstream>
#include <ctime>
#include <unordered_map>
#include <list>
#include <chrono>
#include <mutex> // For thread safety

// LRU Cache implementation
template<typename Value>
struct LRUCache {
    struct CacheEntry {
        Value value;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::list<std::string> access_order;  // Most recent at front
    std::unordered_map<std::string, std::pair<CacheEntry, typename std::list<std::string>::iterator>> data;
    size_t max_size;
    int ttl_seconds;

    LRUCache(size_t max_sz, int ttl) : max_size(max_sz), ttl_seconds(ttl) {}

    Value get(const std::string& key) {
        auto it = data.find(key);
        if (it == data.end()) {
            return {};  // Not found
        }

        auto& [entry, list_it] = it->second;

        // Check if expired
        if (std::chrono::steady_clock::now() - entry.timestamp >= std::chrono::seconds(ttl_seconds)) {
            // Remove expired entry
            access_order.erase(list_it);
            data.erase(it);
            return {};
        }

        // Move to front (most recently used)
        access_order.erase(list_it);
        access_order.push_front(key);
        it->second.second = access_order.begin();

        return entry.value;
    }

    void put(const std::string& key, const Value& value) {
        auto it = data.find(key);

        // If key exists, update it and move to front
        if (it != data.end()) {
            access_order.erase(it->second.second);
            access_order.push_front(key);
            it->second.first.value = value;
            it->second.first.timestamp = std::chrono::steady_clock::now();
            it->second.second = access_order.begin();
            return;
        }

        // If cache is full, evict least recently used (LRU)
        if (data.size() >= max_size) {
            std::string lru_key = access_order.back();
            access_order.pop_back();
            data.erase(lru_key);
        }

        // Insert new entry at front
        access_order.push_front(key);
        CacheEntry entry{value, std::chrono::steady_clock::now()};
        data[key] = {entry, access_order.begin()};
    }

    void clear() {
        access_order.clear();
        data.clear();
    }

    size_t size() const {
        return data.size();
    }
};

// Global caches with LRU eviction
LRUCache<Response> cache(100, 60);       // Response cache: 100 entries, 60s TTL
LRUCache<Response> file_cache(50, 60);   // File cache: 50 entries, 60s TTL
const int CACHE_TTL_SECONDS = 60;         // Cache duration in seconds
std::mutex cache_mutex;
std::mutex file_cache_mutex;

void log_request(const std::string& client_ip, const std::string& method, const std::string& path) {
    std::ofstream log_file("server.log", std::ios::app);
    if (log_file) {
        log_file << "[" << std::time(nullptr) << "] " << client_ip << " - " << method << " " << path << "\n";
    }
}

Response get_cached_response(const std::string& path){
    std::lock_guard<std::mutex> lock(cache_mutex);
    return cache.get(path);
}

void cache_response(const std::string& path, const Response& response) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    cache.put(path, response);
}

Response get_cached_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(file_cache_mutex);
    return file_cache.get(path);
}

void cache_file(const std::string& path, const Response& response) {
    std::lock_guard<std::mutex> lock(file_cache_mutex);
    file_cache.put(path, response);
}