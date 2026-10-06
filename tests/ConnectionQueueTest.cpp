#include <functional>
#include <iostream>
#include <thread>

#include "ConnectionQueue.h"

void consumeConnections(ConnectionQueue& queue)
{
    int clientSocket;

    while (queue.pop(clientSocket))
    {
        std::cout << "Worker received: "
                  << clientSocket << '\n';
    }

    std::cout << "Worker stopped\n";
}

int main()
{
    ConnectionQueue queue(2);

    // Sample numbers only, not real sockets.
    bool first = queue.tryPush(101);
    bool second = queue.tryPush(102);
    bool third = queue.tryPush(103);

    if (!first || !second || third)
    {
        std::cout << "FAILED: Queue capacity check\n";
        return 1;
    }

    int value;

    if (!queue.pop(value) || value != 101)
    {
        std::cout << "FAILED: Queue order check\n";
        return 1;
    }

    queue.close();

    // Closing must still allow already-queued work to be taken.
    if (!queue.pop(value) || value != 102)
    {
        std::cout << "FAILED: Queue drain check\n";
        return 1;
    }

    if (queue.pop(value) || queue.tryPush(104))
    {
        std::cout << "FAILED: Closed queue check\n";
        return 1;
    }

    std::cout << "PASSED: Capacity, order, and closure checks\n";

    // Exercise communication between two threads.
    ConnectionQueue workerQueue(2);

    std::thread worker(
        consumeConnections,
        std::ref(workerQueue)
    );

    bool accepted = workerQueue.tryPush(201);

    workerQueue.close();
    worker.join();

    if (!accepted)
    {
        std::cout << "FAILED: Worker queue rejected work\n";
        return 1;
    }

    return 0;
}