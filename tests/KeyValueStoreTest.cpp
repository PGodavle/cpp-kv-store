#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

#include "KeyValueStore.h"

namespace
{
void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        path_ = std::filesystem::temp_directory_path()
            / ("cpp-kv-store-test-" + std::to_string(getpid()) + "-"
               + std::to_string(
                   std::chrono::steady_clock::now()
                       .time_since_epoch()
                       .count()));

        std::filesystem::create_directory(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    std::filesystem::path file(const std::string& name) const
    {
        return path_ / name;
    }

private:
    std::filesystem::path path_;
};
}

int main()
{
    try
    {
        TemporaryDirectory directory;
        const std::string walPath = directory.file("store.wal").string();

        {
            WriteAheadLog log(walPath);
            KeyValueStore store(&log);

            store.put("persisted key", "value with spaces");
            store.put("temporary", "expires", 1);
            store.put("deleted", "old value");
            require(store.remove("deleted"), "delete should succeed");
                store.put("snapshot-only", "checkpointed");
                store.compact();
                store.put("after-snapshot", "replayed");
                require(store.remove("snapshot-only"),
                    "delete after compaction should succeed");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1100));

        {
            WriteAheadLog log(walPath);
            KeyValueStore store(&log);
            std::string value;

            require(store.get("persisted key", value),
                    "persisted value should recover");
            require(value == "value with spaces",
                    "recovered value should match");
            require(!store.get("temporary", value),
                    "expired value should not recover");
            require(!store.get("deleted", value),
                    "deleted value should not recover");
                require(!store.get("snapshot-only", value),
                    "post-snapshot delete should recover");
                require(store.get("after-snapshot", value) &&
                    value == "replayed",
                    "post-snapshot update should recover");
        }

            const std::string incompleteWalPath =
                directory.file("incomplete.wal").string();

            {
                WriteAheadLog log(incompleteWalPath);
            }

            {
                std::ofstream incompleteWal(
                    incompleteWalPath,
                    std::ios::binary | std::ios::app
                );
                incompleteWal << "PUT \"unfinished\"";
            }

            bool rejectedIncompleteWal = false;

            try
            {
                WriteAheadLog log(incompleteWalPath);
                KeyValueStore store(&log);
            }
            catch (const std::runtime_error&)
            {
                rejectedIncompleteWal = true;
            }

            require(rejectedIncompleteWal,
                    "incomplete WAL tail should fail recovery");

            std::cout << "PASSED: persistence, compaction, deletion, TTL, and torn-WAL recovery\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}