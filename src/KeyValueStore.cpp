#include "KeyValueStore.h"

void KeyValueStore::put(const std::string& key,
                        const std::string& value,
                        int ttlSeconds)
{
    std::lock_guard<std::mutex> lock(mutex_);

    Entry entry;
    entry.value = value;

    if (ttlSeconds > 0)
    {
        entry.hasExpiry = true;

        entry.expiresAt =
            std::chrono::steady_clock::now()
            + std::chrono::seconds(ttlSeconds);
    }

    data_[key] = entry;
}

bool KeyValueStore::get(const std::string& key,
                        std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = data_.find(key);

    if (it == data_.end())
    {
        return false;
    }

    if (it->second.hasExpiry)
    {
        auto now = std::chrono::steady_clock::now();

        if (now >= it->second.expiresAt)
        {
            data_.erase(it);
            return false;
        }
    }

    value = it->second.value;
    return true;
}

bool KeyValueStore::remove(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = data_.find(key);

    if (it == data_.end())
    {
        return false;
    }

    bool expired = false;

    if (it->second.hasExpiry)
    {
        auto now = std::chrono::steady_clock::now();

        if (now >= it->second.expiresAt)
        {
            expired = true;
        }
    }

    data_.erase(it);

    if (expired)
    {
        return false;
    }

    return true;
}