#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>
#include <set>

#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <exception>
#include <functional>
#include <thread>
#include <sys/time.h>
#include "WriteAheadLog.h"

#include "ConnectionQueue.h"
#include "CommandHandler.h"

class ActiveClients
{
public:
    bool add(int clientSocket)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (stopping_)
        {
            return false;
        }

        sockets_.insert(clientSocket);
        return true;
    }

    void finish(int clientSocket)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        sockets_.erase(clientSocket);
        close(clientSocket);
    }

    void stopAll()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        stopping_ = true;

        for (int clientSocket : sockets_)
        {
            shutdown(clientSocket, SHUT_RDWR);
        }
    }

private:
    std::mutex mutex_;
    std::set<int> sockets_;
    bool stopping_ = false;
};

bool sendAll(int clientSocket, const std::string& message);

// send() may send fewer bytes than requested.
// Keep sending until the entire message is sent.
void handleClient(int clientSocket, KeyValueStore& store)
{
    char buffer[1024];
    std::string line;

    const std::size_t maxLineLength = 4096;
    bool connected = true;

    while (connected)
    {
        ssize_t received = recv(
            clientSocket,
            buffer,
            sizeof(buffer),
            0
        );

        if (received == 0)
        {
            // The client closed its connection.
            break;
        }

        if (received < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::cout << "Client receive timeout\n";
                break;
            }

            perror("recv");
            break;
        }

        // Only process bytes actually received.
        for (ssize_t i = 0; i < received; i++)
        {
            if (buffer[i] == '\n')
            {
                // Also accept Windows-style line endings: \r\n.
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }

                std::string response;

                bool keepSession = processCommand(
                    store,
                    line,
                    response
                );

                bool sent = sendAll(
                    clientSocket,
                    response + "\n"
                );

                line.clear();

                if (!sent || !keepSession)
                {
                    connected = false;
                    break;
                }
            }
            else
            {
                if (line.size() >= maxLineLength)
                {
                    sendAll(
                        clientSocket,
                        "ERROR Command too long\n"
                    );

                    connected = false;
                    break;
                }

                line += buffer[i];
            }
        }
    }
}

bool configureClientTimeouts(int clientSocket, int seconds)
{
    timeval timeout{};
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;

    if (setsockopt(
            clientSocket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)) == -1)
    {
        perror("SO_RCVTIMEO");
        return false;
    }

    if (setsockopt(
            clientSocket,
            SOL_SOCKET,
            SO_SNDTIMEO,
            &timeout,
            sizeof(timeout)) == -1)
    {
        perror("SO_SNDTIMEO");
        return false;
    }

    return true;
}

void workerLoop(
    ConnectionQueue& queue,
    KeyValueStore& store,
    ActiveClients& clients)
{
    int clientSocket;

    while (queue.pop(clientSocket))
    {
        bool registered = false;

        try
        {
            registered = clients.add(clientSocket);

            if (registered &&
                configureClientTimeouts(clientSocket, 30))
            {
                if (sendAll(
                        clientSocket,
                        "Welcome! Use PUT, GET, DELETE, EXIT\n"))
                {
                    handleClient(clientSocket, store);
                }
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "Client handling failed: "
                      << error.what() << '\n';
        }

        if (registered)
        {
            clients.finish(clientSocket);
        }
        else
        {
            // Shutdown has started, or registration failed.
            close(clientSocket);
        }
    }
}
bool sendAll(int clientSocket, const std::string& message)
{
    std::size_t totalSent = 0;

    while (totalSent < message.size())
    {
        ssize_t sent = send(
            clientSocket,
            message.data() + totalSent,
            message.size() - totalSent,
            MSG_NOSIGNAL
        );

        if (sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return false;
        }

        if (sent == 0)
        {
            return false;
        }

        totalSent += static_cast<std::size_t>(sent);
    }

    return true;
}

int runServer(KeyValueStore& store)
{
    // IPv4 + TCP.
    int serverSocket = socket(
        AF_INET,
        SOCK_STREAM | SOCK_NONBLOCK,
        0
    );

    if (serverSocket == -1)
    {
        perror("socket");
        return 1;
    }

    // Allow convenient reuse of the address after restarting.
    int reuseAddress = 1;

    if (setsockopt(
            serverSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuseAddress,
            sizeof(reuseAddress)) == -1)
    {
        perror("setsockopt");
        close(serverSocket);
        return 1;
    }

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_port = htons(9090);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) == -1)
    {
        perror("bind");
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 5) == -1)
    {
        perror("listen");
        close(serverSocket);
        return 1;
    }

          // Block these signals before creating workers.
    // Workers inherit the blocked signal mask.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);

    int maskError = pthread_sigmask(
        SIG_BLOCK,
        &signals,
        nullptr
    );

    if (maskError != 0)
    {
        // pthread_sigmask returns the error number directly.
        errno = maskError;
        perror("pthread_sigmask");
        close(serverSocket);
        return 1;
    }

    int signalFd = signalfd(
        -1,
        &signals,
        SFD_CLOEXEC | SFD_NONBLOCK
    );

    if (signalFd == -1)
    {
        perror("signalfd");
        close(serverSocket);
        return 1;
    }

    ConnectionQueue queue(8);
    ActiveClients clients;

    const int workerCount = 4;
    std::thread workers[workerCount];

    try
    {
        for (int i = 0; i < workerCount; i++)
        {
            workers[i] = std::thread(
                workerLoop,
                std::ref(queue),
                std::ref(store),
                std::ref(clients)
            );
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "Could not start workers: "
                  << error.what() << '\n';

        clients.stopAll();
        queue.close();

        for (int i = 0; i < workerCount; i++)
        {
            if (workers[i].joinable())
            {
                workers[i].join();
            }
        }

        close(signalFd);
        close(serverSocket);
        return 1;
    }

    // Watch for either a shutdown request or a new connection.
    pollfd watched[2]{};

    watched[0].fd = signalFd;
    watched[0].events = POLLIN;

    watched[1].fd = serverSocket;
    watched[1].events = POLLIN;

    int exitCode = 1;

    std::cout << "Server listening on 127.0.0.1:9090\n";
    std::cout << "Press Ctrl+C to stop\n";

    while (true)
    {
        int ready = poll(watched, 2, -1);

        if (ready == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("poll");
            break;
        }

        if ((watched[0].revents | watched[1].revents)
            & (POLLERR | POLLHUP | POLLNVAL))
        {
            std::cerr << "Error on a monitored descriptor\n";
            break;
        }

        // Handle shutdown before accepting another connection.
        if (watched[0].revents & POLLIN)
        {
            signalfd_siginfo info{};

            ssize_t count = read(
                signalFd,
                &info,
                sizeof(info)
            );

            if (count == sizeof(info))
            {
                std::cout << "\nShutdown requested\n";
                exitCode = 0;
                break;
            }

            if (count < 0 &&
                (errno == EINTR || errno == EAGAIN))
            {
                continue;
            }

            std::cerr << "Could not read shutdown signal\n";
            break;
        }

        if (watched[1].revents & POLLIN)
        {
            int clientSocket = accept(
                serverSocket,
                nullptr,
                nullptr
            );

            if (clientSocket == -1)
            {
                if (errno == EINTR ||
                    errno == EAGAIN ||
                    errno == EWOULDBLOCK ||
                    errno == ECONNABORTED)
                {
                    continue;
                }

                perror("accept");
                break;
            }

            bool queued = false;

            try
            {
                queued = queue.tryPush(clientSocket);
            }
            catch (const std::exception& error)
            {
                std::cerr << "Could not queue client: "
                          << error.what() << '\n';

                close(clientSocket);
                break;
            }

            if (!queued)
            {
                const char message[] = "SERVER_BUSY\n";

                send(
                    clientSocket,
                    message,
                    sizeof(message) - 1,
                    MSG_NOSIGNAL | MSG_DONTWAIT
                );

                close(clientSocket);
            }
        }
    }

    // Shutdown order matters.
    close(serverSocket);   // Stop accepting connections.
    clients.stopAll();     // Interrupt active socket operations.
    queue.close();         // Wake workers and drain queued sockets.

    for (int i = 0; i < workerCount; i++)
    {
        workers[i].join();
    }

    close(signalFd);

    std::cout << "Server stopped cleanly\n";
    return exitCode;

}
int main()
{
    try
    {
        WriteAheadLog log("data/store.wal");
        KeyValueStore store(&log);

        return runServer(store);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Server failed: "
                  << error.what() << '\n';

        return 1;
    }
}