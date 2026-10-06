#include <cerrno>
#include <iostream>
#include <string>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

static int connectToServer(const std::string& host,
                           const std::string& port)
{
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* addresses = nullptr;
    int result = getaddrinfo(
        host.c_str(),
        port.c_str(),
        &hints,
        &addresses
    );

    if (result != 0)
    {
        std::cerr << "Could not resolve host: "
                  << gai_strerror(result) << '\n';
        return -1;
    }

    int clientSocket = -1;

    for (addrinfo* address = addresses;
         address != nullptr;
         address = address->ai_next)
    {
        clientSocket = socket(
            address->ai_family,
            address->ai_socktype,
            address->ai_protocol
        );

        if (clientSocket != -1 &&
            connect(
                clientSocket,
                address->ai_addr,
                address->ai_addrlen
            ) == 0)
        {
            break;
        }

        if (clientSocket != -1)
        {
            close(clientSocket);
            clientSocket = -1;
        }
    }

    freeaddrinfo(addresses);

    if (clientSocket == -1)
    {
        perror("connect");
    }

    return clientSocket;
}

static bool sendAll(int clientSocket, const std::string& message)
{
    std::size_t sentTotal = 0;

    while (sentTotal < message.size())
    {
        ssize_t sent = send(
            clientSocket,
            message.data() + sentTotal,
            message.size() - sentTotal,
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

        sentTotal += static_cast<std::size_t>(sent);
    }

    return true;
}

static bool receiveLine(int clientSocket, std::string& line)
{
    line.clear();
    char character;

    while (true)
    {
        ssize_t received = recv(clientSocket, &character, 1, 0);

        if (received == 0)
        {
            return !line.empty();
        }

        if (received < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return false;
        }

        if (character == '\n')
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            return true;
        }

        line += character;
    }
}

int main(int argc, char** argv)
{
    if (argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " [HOST [PORT]]\n";
        return 2;
    }

    std::string host = argc >= 2 ? argv[1] : "127.0.0.1";
    std::string port = argc >= 3 ? argv[2] : "9090";
    int clientSocket = connectToServer(host, port);

    if (clientSocket == -1)
    {
        return 1;
    }

    std::string response;

    if (!receiveLine(clientSocket, response))
    {
        std::cerr << "Server closed before sending its greeting\n";
        close(clientSocket);
        return 1;
    }

    std::cout << response << '\n';
    std::string command;

    while (std::getline(std::cin, command))
    {
        if (!sendAll(clientSocket, command + "\n"))
        {
            std::cerr << "Failed to send command\n";
            close(clientSocket);
            return 1;
        }

        if (!receiveLine(clientSocket, response))
        {
            std::cerr << "Server closed the connection\n";
            close(clientSocket);
            return 1;
        }

        std::cout << response << '\n';

        if (response == "BYE")
        {
            break;
        }
    }

    close(clientSocket);
    return 0;
}