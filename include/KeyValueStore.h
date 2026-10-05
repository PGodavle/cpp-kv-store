#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

class KeyValueStore
{
public:
     void put(const std::string& key,
             const std::string& value,
             int ttlSeconds = 0);

    bool get(const std::string& key, std::string& value);

    bool remove(const std::string& key);

private:
    struct Entry{
        std::string value;
        bool hasExpiry = false;
        std::chrono::steady_clock::time_point expiresAt{};
    };
    std::unordered_map<std::string, Entry> data_;
    mutable std::mutex mutex_;
    //why
};