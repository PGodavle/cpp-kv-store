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

static std::string directoryFor(const std::string& path)
{
    std::size_t slash = path.find_last_of('/');

    if (slash == std::string::npos)
    {
        return ".";
    }

    return slash == 0 ? "/" : path.substr(0, slash);
}

static void syncDirectory(const std::string& path)
{
    std::string directory = directoryFor(path);
    int descriptor = open(
        directory.c_str(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC
    );

    if (descriptor == -1)
    {
        throw std::runtime_error(
            "Cannot open directory: " + directory
        );
    }

    try
    {
        syncDescriptor(descriptor);
    }
    catch (...)
    {
        close(descriptor);
        throw;
    }

    close(descriptor);
}

static void writeAll(int descriptor, const std::string& contents)
{
    std::size_t totalWritten = 0;

    while (totalWritten < contents.size())
    {
        ssize_t written = write(
            descriptor,
            contents.data() + totalWritten,
            contents.size() - totalWritten
        );

        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            throw std::runtime_error("Failed to write durable file");
        }

        if (written == 0)
        {
            throw std::runtime_error("Failed to write durable file");
        }

        totalWritten += static_cast<std::size_t>(written);
    }
}

static void atomicWrite(const std::string& path,
                        const std::string& contents)
{
    std::string temporaryPath = path + ".tmp";
    int descriptor = open(
        temporaryPath.c_str(),
        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
        0600
    );

    if (descriptor == -1)
    {
        throw std::runtime_error(
            "Cannot create temporary file: " + temporaryPath
        );
    }

    try
    {
        writeAll(descriptor, contents);
        syncDescriptor(descriptor);

        if (close(descriptor) == -1)
        {
            descriptor = -1;
            throw std::runtime_error("Cannot close temporary file");
        }
        descriptor = -1;

        if (rename(temporaryPath.c_str(), path.c_str()) == -1)
        {
            throw std::runtime_error("Cannot replace durable file: " + path);
        }

        syncDirectory(path);
    }
    catch (...)
    {
        if (descriptor != -1)
        {
            close(descriptor);
        }

        unlink(temporaryPath.c_str());
        throw;
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

std::string WriteAheadLog::readSnapshot()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string snapshotPath = path_ + ".snapshot";
    int descriptor = open(snapshotPath.c_str(), O_RDONLY | O_CLOEXEC);

    if (descriptor == -1)
    {
        if (errno == ENOENT)
        {
            return {};
        }

        throw std::runtime_error("Cannot open snapshot: " + snapshotPath);
    }

    std::string contents;
    char buffer[4096];

    while (true)
    {
        ssize_t bytesRead = read(descriptor, buffer, sizeof(buffer));

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

            close(descriptor);
            throw std::runtime_error("Cannot read snapshot");
        }

        contents.append(buffer, static_cast<std::size_t>(bytesRead));
    }

    close(descriptor);
    return contents;
}

void WriteAheadLog::writeSnapshot(const std::string& contents)
{
    std::lock_guard<std::mutex> lock(mutex_);
    atomicWrite(path_ + ".snapshot", contents);
}

void WriteAheadLog::reset(std::uint64_t baseSequence)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (failed_)
    {
        throw std::runtime_error("WAL unavailable after a write failure");
    }

    std::string contents = "BASE " + std::to_string(baseSequence) + "\n";
    atomicWrite(path_, contents);

    int replacementDescriptor = open(
        path_.c_str(),
        O_RDWR | O_APPEND | O_CLOEXEC
    );

    if (replacementDescriptor == -1)
    {
        failed_ = true;
        throw std::runtime_error("Cannot reopen compacted WAL");
    }

    int oldDescriptor = fileDescriptor_;
    fileDescriptor_ = replacementDescriptor;

    if (close(oldDescriptor) == -1)
    {
        failed_ = true;
        throw std::runtime_error("Cannot close replaced WAL");
    }
}

WriteAheadLog::WriteAheadLog(const std::string& path)
    : fileDescriptor_(-1),
      lockDescriptor_(-1),
      path_(path)
{
    std::string lockPath = path_ + ".lock";
    lockDescriptor_ = open(
        lockPath.c_str(),
        O_RDWR | O_CREAT | O_CLOEXEC,
        0600
    );

    if (lockDescriptor_ == -1)
    {
        throw std::runtime_error("Cannot open WAL lock: " + lockPath);
    }

    try
    {
        while (flock(lockDescriptor_, LOCK_EX | LOCK_NB) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                throw std::runtime_error(
                    "WAL is already in use: " + path_
                );
            }

            throw std::runtime_error("Cannot lock WAL: " + path_);
        }

        fileDescriptor_ = open(
            path_.c_str(),
            O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC,
            0600
        );

        if (fileDescriptor_ == -1)
        {
            throw std::runtime_error("Cannot open WAL: " + path_);
        }

        syncDescriptor(fileDescriptor_);
        syncDirectory(path_);
    }
    catch (...)
    {
        if (fileDescriptor_ != -1)
        {
            close(fileDescriptor_);
            fileDescriptor_ = -1;
        }

        if (lockDescriptor_ != -1)
        {
            close(lockDescriptor_);
            lockDescriptor_ = -1;
        }

        throw;
    }
}

WriteAheadLog::~WriteAheadLog()
{
    if (fileDescriptor_ != -1)
    {
        close(fileDescriptor_);
    }

    if (lockDescriptor_ != -1)
    {
        close(lockDescriptor_);
    }
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