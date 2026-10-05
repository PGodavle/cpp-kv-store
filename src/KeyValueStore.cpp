#include "KeyValueStore.h"

void KeyValueStore::put(const std::string& key,
                        const std::string& value)
{
    data_[key] = value;
}

bool KeyValueStore::get(const std::string& key,
                        std::string& value) const
{
    auto it = data_.find(key);

    if (it == data_.end())
    {
        return false;
    }

    value = it->second;
    return true;
}

bool KeyValueStore::remove(const std::string& key)
{
    auto it = data_.find(key);

    if (it == data_.end())
    {
        return false;
    }

    data_.erase(it);
    return true;
}