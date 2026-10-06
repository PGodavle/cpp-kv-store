#include "ConnectionQueue.h"

ConnectionQueue::ConnectionQueue(std::size_t capacity)
    : capacity_(capacity),
      closed_(false)
{
}

bool ConnectionQueue::tryPush(int clientSocket)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (closed_)
    {
        return false;
    }

    if (sockets_.size() >= capacity_)
    {
        return false;
    }

    sockets_.push(clientSocket);

    // Wake one worker that may be waiting for work.
    condition_.notify_one();

    return true;
}

bool ConnectionQueue::pop(int& clientSocket)
{
    std::unique_lock<std::mutex> lock(mutex_);

    while (sockets_.empty() && !closed_)
    {
        condition_.wait(lock);
    }

    // The queue is closed and all pending work is finished.
    if (sockets_.empty())
    {
        return false;
    }

    clientSocket = sockets_.front();
    sockets_.pop();

    return true;
}

void ConnectionQueue::close()
{
    std::lock_guard<std::mutex> lock(mutex_);

    closed_ = true;

    // Every waiting worker must get a chance to stop.
    condition_.notify_all();
}