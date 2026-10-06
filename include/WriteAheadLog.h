#pragma once

#include <mutex>
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
    WriteAheadLog(const WriteAheadLog&) = delete;
    WriteAheadLog& operator=(const WriteAheadLog&) = delete;

private:
    int fileDescriptor_;
    bool failed_ = false;
    std::mutex mutex_;
};