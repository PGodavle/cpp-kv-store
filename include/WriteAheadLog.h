#pragma once

#include <mutex>
#include <cstdint>
#include <string>
#include <vector>


class WriteAheadLog
{
public:
    explicit WriteAheadLog(const std::string& path);
    ~WriteAheadLog();

    bool append(const std::string& record);
    bool isEmpty();
    std::vector<std::string> readRecords();
    std::string readSnapshot();
    void writeSnapshot(const std::string& contents);
    void reset(std::uint64_t baseSequence);
    WriteAheadLog(const WriteAheadLog&) = delete;
    WriteAheadLog& operator=(const WriteAheadLog&) = delete;

private:
    int fileDescriptor_;
    int lockDescriptor_;
    std::string path_;
    bool failed_ = false;
    std::mutex mutex_;
};