#include "WriteAheadLog.h"

#include <cerrno>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

static void syncDescriptor(int descriptor)
{
    while (fsync(descriptor) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }

        throw std::runtime_error(
            "Failed to sync file or directory"
        );
    }
}

bool WriteAheadLog::isEmpty()
{
    std::lock_guard<std::mutex> lock(mutex_);

    struct stat info{};

    if (fstat(fileDescriptor_, &info) == -1)
    {
        throw std::runtime_error("Cannot inspect WAL");
    }

    return info.st_size == 0;
}

WriteAheadLog::WriteAheadLog(const std::string& path)
{
    fileDescriptor_ = open(
        path.c_str(),
        O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC,
        0600
    );

    if (fileDescriptor_ == -1)
    {
        throw std::runtime_error("Cannot open WAL: " + path);
    }

    int directoryFd = -1;

    try
    {
        // Only one cooperating server can own this WAL.
        while (flock(fileDescriptor_, LOCK_EX | LOCK_NB) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                throw std::runtime_error(
                    "WAL is already in use: " + path
                );
            }

            throw std::runtime_error("Cannot lock WAL: " + path);
        }

        // Sync the file before its directory entry.
        syncDescriptor(fileDescriptor_);

        std::string directory = ".";
        std::size_t slash = path.find_last_of('/');

        if (slash != std::string::npos)
        {
            if (slash == 0)
            {
                directory = "/";
            }
            else
            {
                directory = path.substr(0, slash);
            }
        }

        directoryFd = open(
            directory.c_str(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC
        );

        if (directoryFd == -1)
        {
            throw std::runtime_error(
                "Cannot open WAL directory: " + directory
            );
        }

        syncDescriptor(directoryFd);

        close(directoryFd);
        directoryFd = -1;
    }
    catch (...)
    {
        if (directoryFd != -1)
        {
            close(directoryFd);
        }

        close(fileDescriptor_);
        throw;
    }
}

WriteAheadLog::~WriteAheadLog()
{
    close(fileDescriptor_);
}

bool WriteAheadLog::append(const std::string& record)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (failed_)
    {
        return false;
    }

    // Each record must occupy exactly one non-empty line.
    if (record.empty() ||
        record.find('\n') != std::string::npos ||
        record.find('\r') != std::string::npos)
    {
        return false;
    }

    std::string line = record + "\n";
    std::size_t totalWritten = 0;

    while (totalWritten < line.size())
    {
        ssize_t written = write(
            fileDescriptor_,
            line.data() + totalWritten,
            line.size() - totalWritten
        );

        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            failed_ = true;
            return false;
        }

        if (written == 0)
        {
            failed_ = true;
            return false;
        }

        totalWritten += static_cast<std::size_t>(written);
    }

    // Request synchronization with persistent storage.
    while (fsync(fileDescriptor_) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }

        failed_ = true;
        return false;
    }

    return true;
}
std::vector<std::string> WriteAheadLog::readRecords()
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> records;
    std::string line;

    char buffer[4096];
    off_t offset = 0;

    while (true)
    {
        ssize_t bytesRead = pread(
            fileDescriptor_,
            buffer,
            sizeof(buffer),
            offset
        );

        if (bytesRead == 0)
        {
            break;
        }

        if (bytesRead < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            throw std::runtime_error("Cannot read WAL");
        }

        offset += bytesRead;

        for (ssize_t i = 0; i < bytesRead; i++)
        {
            if (buffer[i] == '\n')
            {
                records.push_back(line);
                line.clear();
            }
            else
            {
                if (buffer[i] == '\r')
                {
                    throw std::runtime_error(
                        "Unexpected carriage return in WAL"
                    );
                }

                line += buffer[i];
            }
        }
    }

    if (!line.empty())
    {
        throw std::runtime_error(
            "WAL ends with an incomplete record"
        );
    }

    return records;
}