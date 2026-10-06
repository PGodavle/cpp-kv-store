#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

class ConnectionQueue
{
public:
    explicit ConnectionQueue(std::size_t capacity);

    bool tryPush(int clientSocket);

    bool pop(int& clientSocket);

    void close();

private:
    std::queue<int> sockets_;
    std::size_t capacity_;
    bool closed_;

    std::mutex mutex_;
    std::condition_variable condition_;
};