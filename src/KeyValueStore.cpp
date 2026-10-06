#include "KeyValueStore.h"

#include <iomanip>
#include <limits>
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

    auto parsePut = [&](const std::string& line,
                        std::string& key,
                        Entry& entry)
    {
        std::istringstream input(line);
        std::string command;
        std::string extra;
        long long expiryUnixMs = 0;

        input >> command;

        if (command != "PUT" ||
            !(input >> std::quoted(key)
                    >> std::quoted(entry.value)
                    >> expiryUnixMs)
            || expiryUnixMs < 0
            || (input >> extra))
        {
            throw std::runtime_error("Invalid PUT record during recovery");
        }

        if (expiryUnixMs != 0 && expiryUnixMs <= wallNowMs)
        {
            return false;
        }

        if (expiryUnixMs != 0)
        {
            long long remainingMs = expiryUnixMs - wallNowMs;

            if (remainingMs > maxRemainingMs)
            {
                throw std::runtime_error(
                    "WAL expiration is out of range"
                );
            }

            entry.hasExpiry = true;
            entry.expiresAt =
                steadyNow + std::chrono::milliseconds(remainingMs);
        }

        return true;
    };

    auto parseSequence = [](const std::string& text,
                            std::uint64_t& sequence)
    {
        if (text.empty() || text.front() == '-')
        {
            return false;
        }

        try
        {
            std::size_t consumed = 0;
            unsigned long long parsed = std::stoull(text, &consumed);

            if (consumed != text.size() ||
                parsed > std::numeric_limits<std::uint64_t>::max())
            {
                return false;
            }

            sequence = static_cast<std::uint64_t>(parsed);
            return true;
        }
        catch (...)
        {
            return false;
        }
    };

    std::string snapshot = log_->readSnapshot();
    std::uint64_t snapshotSequence = 0;
    bool hasSnapshot = !snapshot.empty();

    if (hasSnapshot)
    {
        std::istringstream snapshotInput(snapshot);
        std::string line;
        std::string tag;
        std::string sequenceText;
        std::string extra;

        if (!std::getline(snapshotInput, line))
        {
            throw std::runtime_error("Snapshot is empty");
        }

        std::istringstream header(line);

        if (!(header >> tag >> sequenceText) || tag != "SNAPSHOT" ||
            (header >> extra) ||
            !parseSequence(sequenceText, snapshotSequence))
        {
            throw std::runtime_error("Invalid snapshot header");
        }

        std::size_t lineNumber = 1;

        while (std::getline(snapshotInput, line))
        {
            lineNumber++;
            std::string key;
            Entry entry;

            try
            {
                if (parsePut(line, key, entry))
                {
                    data_[key] = entry;
                }
            }
            catch (const std::exception&)
            {
                throw std::runtime_error(
                    "Invalid snapshot record at line "
                    + std::to_string(lineNumber)
                );
            }
        }
    }

    std::vector<std::string> records = log_->readRecords();
    bool hasBase = false;
    std::size_t lineNumber = 0;

    for (const std::string& line : records)
    {
        lineNumber++;
        std::istringstream input(line);
        std::string command;
        std::string key;
        std::string sequenceText;
        std::string extra;
        input >> command;

        if (command == "BASE")
        {
            std::uint64_t baseSequence = 0;

            if (lineNumber != 1 || hasBase ||
                !(input >> sequenceText) || (input >> extra) ||
                !parseSequence(sequenceText, baseSequence) ||
                !hasSnapshot || baseSequence > snapshotSequence)
            {
                throw std::runtime_error(
                    "Invalid WAL base at line " + std::to_string(lineNumber)
                );
            }

            sequence_ = baseSequence;
            hasBase = true;
            continue;
        }

        if (command == "CHECKPOINT")
        {
            std::uint64_t checkpointSequence = 0;

            if (!(input >> sequenceText) || (input >> extra) ||
                !parseSequence(sequenceText, checkpointSequence) ||
                checkpointSequence != sequence_)
            {
                throw std::runtime_error(
                    "Invalid WAL checkpoint at line "
                    + std::to_string(lineNumber)
                );
            }

            continue;
        }

        if (command != "PUT" && command != "DELETE")
        {
            throw std::runtime_error(
                "Unknown WAL operation at line " + std::to_string(lineNumber)
            );
        }

        if (sequence_ == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::runtime_error("WAL sequence is out of range");
        }

        sequence_++;

        if (command == "PUT")
        {
            Entry entry;
            bool unexpired = false;

            try
            {
                unexpired = parsePut(line, key, entry);
            }
            catch (const std::exception&)
            {
                throw std::runtime_error(
                    "Invalid PUT record at WAL line "
                    + std::to_string(lineNumber)
                );
            }

            if (sequence_ > snapshotSequence)
            {
                if (unexpired)
                {
                    data_[key] = entry;
                }
                else
                {
                    data_.erase(key);
                }
            }
        }
        else
        {
            if (!(input >> std::quoted(key)) || (input >> extra))
            {
                throw std::runtime_error(
                    "Invalid DELETE record at WAL line "
                    + std::to_string(lineNumber)
                );
            }

            if (sequence_ > snapshotSequence)
            {
                data_.erase(key);
            }
        }
    }

    if (records.empty() && hasSnapshot)
    {
        sequence_ = snapshotSequence;
    }
    else if (sequence_ < snapshotSequence)
    {
        throw std::runtime_error(
            "Snapshot is newer than the available WAL"
        );
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
        if (sequence_ == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::runtime_error("WAL sequence is out of range");
        }

        std::ostringstream record;

        record << "PUT "
               << std::quoted(key) << ' '
               << std::quoted(value) << ' '
               << expiryUnixMs;

        appendRecord(record.str());
        sequence_++;
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
        if (sequence_ == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::runtime_error("WAL sequence is out of range");
        }

        std::ostringstream record;

        record << "DELETE " << std::quoted(key);

        appendRecord(record.str());
        sequence_++;
    }

    data_.erase(it);
    return true;
}

void KeyValueStore::compact()
{
    std::lock_guard<std::mutex> lock(mutex_);
    checkHealthy();

    if (log_ == nullptr)
    {
        return;
    }

    auto steadyNow = std::chrono::steady_clock::now();
    long long wallNowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    std::ostringstream snapshot;
    snapshot << "SNAPSHOT " << sequence_ << '\n';

    for (const auto& [key, entry] : data_)
    {
        long long expiryUnixMs = 0;

        if (entry.hasExpiry)
        {
            if (steadyNow >= entry.expiresAt)
            {
                continue;
            }

            long long remainingMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    entry.expiresAt - steadyNow
                ).count();

            if (remainingMs <= 0)
            {
                continue;
            }

            expiryUnixMs = wallNowMs + remainingMs;
        }

        snapshot << "PUT "
                 << std::quoted(key) << ' '
                 << std::quoted(entry.value) << ' '
                 << expiryUnixMs << '\n';
    }

    try
    {
        log_->writeSnapshot(snapshot.str());
        log_->reset(sequence_);
    }
    catch (...)
    {
        healthy_ = false;
        throw;
    }
}