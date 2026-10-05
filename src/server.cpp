#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "CommandHandler.h"

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

int main()
{
    // IPv4 + TCP.
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

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

        // One store shared across successive client connections.
    KeyValueStore store;

    std::cout << "Server listening on 127.0.0.1:9090\n";

    while (true)
    {
        int clientSocket = accept(
            serverSocket,
            nullptr,
            nullptr
        );

        if (clientSocket == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("accept");
            close(serverSocket);
            return 1;
        }

        std::cout << "Client connected\n";

        if (sendAll(
                clientSocket,
                "Welcome! Use PUT, GET, DELETE, EXIT\n"))
        {
            handleClient(clientSocket, store);
        }

        close(clientSocket);
        std::cout << "Client disconnected\n";
    }
}