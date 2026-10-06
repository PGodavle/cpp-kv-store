#include "KeyValueStore.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

KeyValueStore::KeyValueStore(WriteAheadLog* log)
    : log_(log)
{
    if (log_ != nullptr)
    {
        recover();
    }
}

void KeyValueStore::recover()
{
    std::vector<std::string> records = log_->readRecords();

    auto wallNow = std::chrono::system_clock::now();
    auto steadyNow = std::chrono::steady_clock::now();

    long long wallNowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            wallNow.time_since_epoch()
        ).count();

    auto maxRemainingMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::time_point::max()
            - steadyNow
        ).count();

    std::size_t lineNumber = 0;

    for (const std::string& line : records)
    {
        lineNumber++;

        std::istringstream input(line);

        std::string command;
        std::string key;
        std::string value;
        std::string extra;

        input >> command;

        if (command == "PUT")
        {
            long long expiryUnixMs = 0;

            if (!(input >> std::quoted(key)
                        >> std::quoted(value)
                        >> expiryUnixMs)
                || expiryUnixMs < 0
                || (input >> extra))
            {
                throw std::runtime_error(
                    "Invalid PUT record at WAL line "
                    + std::to_string(lineNumber)
                );
            }

            if (expiryUnixMs != 0 &&
                expiryUnixMs <= wallNowMs)
            {
                // This expired update replaces any older value.
                data_.erase(key);
                continue;
            }

            Entry entry;
            entry.value = value;

            if (expiryUnixMs != 0)
            {
                long long remainingMs =
                    expiryUnixMs - wallNowMs;

                if (remainingMs > maxRemainingMs)
                {
                    throw std::runtime_error(
                        "WAL expiration is out of range"
                    );
                }

                entry.hasExpiry = true;

                entry.expiresAt =
                    steadyNow
                    + std::chrono::milliseconds(remainingMs);
            }

            data_[key] = entry;
        }
        else if (command == "DELETE")
        {
            if (!(input >> std::quoted(key)) ||
                (input >> extra))
            {
                throw std::runtime_error(
                    "Invalid DELETE record at WAL line "
                    + std::to_string(lineNumber)
                );
            }

            data_.erase(key);
        }
        else
        {
            throw std::runtime_error(
                "Unknown operation at WAL line "
                + std::to_string(lineNumber)
            );
        }
    }
}

void KeyValueStore::checkHealthy() const
{
    if (!healthy_)
    {
        throw std::runtime_error(
            "Store unavailable after a storage failure"
        );
    }
}

void KeyValueStore::appendRecord(const std::string& record)
{
    if (log_ == nullptr)
    {
        return;
    }

    try
    {
        if (!log_->append(record))
        {
            throw std::runtime_error("WAL write failed");
        }
    }
    catch (...)
    {
        healthy_ = false;
        throw;
    }
}

void KeyValueStore::put(const std::string& key,
                        const std::string& value,
                        int ttlSeconds)
{
    std::lock_guard<std::mutex> lock(mutex_);
    checkHealthy();

    if (log_ != nullptr &&
        (key.find_first_of("\r\n") != std::string::npos ||
         value.find_first_of("\r\n") != std::string::npos))
    {
        throw std::invalid_argument(
            "WAL keys and values cannot contain line breaks"
        );
    }

    Entry entry;
    entry.value = value;

    long long expiryUnixMs = 0;

    if (ttlSeconds > 0)
    {
        entry.hasExpiry = true;

        entry.expiresAt =
            std::chrono::steady_clock::now()
            + std::chrono::seconds(ttlSeconds);

        auto expiry =
            std::chrono::system_clock::now()
            + std::chrono::seconds(ttlSeconds);

        expiryUnixMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                expiry.time_since_epoch()
            ).count();
    }

    if (log_ != nullptr)
    {
        std::ostringstream record;

        record << "PUT "
               << std::quoted(key) << ' '
               << std::quoted(value) << ' '
               << expiryUnixMs;

        appendRecord(record.str());
    }

    try
    {
        data_[key] = entry;
    }
    catch (...)
    {
        // The WAL may already contain this update.
        // Do not continue serving potentially inconsistent data.
        healthy_ = false;
        throw;
    }
}

bool KeyValueStore::get(const std::string& key,
                        std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    checkHealthy();

    auto it = data_.find(key);

    if (it == data_.end())
    {
        return false;
    }

    if (it->second.hasExpiry &&
        std::chrono::steady_clock::now() >= it->second.expiresAt)
    {
        data_.erase(it);
        return false;
    }

    value = it->second.value;
    return true;
}

bool KeyValueStore::remove(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    checkHealthy();

    auto it = data_.find(key);

    if (it == data_.end())
    {
        return false;
    }

    if (it->second.hasExpiry &&
        std::chrono::steady_clock::now() >= it->second.expiresAt)
    {
        data_.erase(it);
        return false;
    }

    if (log_ != nullptr)
    {
        std::ostringstream record;

        record << "DELETE " << std::quoted(key);

        appendRecord(record.str());
    }

    data_.erase(it);
    return true;
}