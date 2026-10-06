#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include "WriteAheadLog.h"

class KeyValueStore
{
public:
    explicit KeyValueStore(WriteAheadLog* log = nullptr);

    void put(const std::string& key,
             const std::string& value,
             int ttlSeconds = 0);

    bool get(const std::string& key, std::string& value);

    bool remove(const std::string& key);

    void compact();

private:
    struct Entry
    {
        std::string value;
        bool hasExpiry = false;
        std::chrono::steady_clock::time_point expiresAt{};
    };

    void checkHealthy() const;
    void appendRecord(const std::string& record);
    void recover();
    std::unordered_map<std::string, Entry> data_;
    mutable std::mutex mutex_;

    WriteAheadLog* log_;
    bool healthy_ = true;
    std::uint64_t sequence_ = 0;
};