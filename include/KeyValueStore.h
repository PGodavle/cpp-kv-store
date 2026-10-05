#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

class KeyValueStore
{
public:
    void put(const std::string& key, const std::string& value);

    bool get(const std::string& key, std::string& value) const;

    bool remove(const std::string& key);

private:
    std::unordered_map<std::string, std::string> data_;
    mutable std::mutex mutex_;
    /*why mutable? Because we want to allow const member functions (like get) to modify the mutex 
     for thread safety, even though they don't modify the actual data. This allows us to lock the
     mutex in const functions without violating const-correctness.*/
};