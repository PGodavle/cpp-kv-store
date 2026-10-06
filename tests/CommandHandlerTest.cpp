#include <iostream>
#include <stdexcept>
#include <string>

#include "CommandHandler.h"

namespace
{
void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
}

int main()
{
    try
    {
        KeyValueStore store;
        std::string response;

        require(processCommand(
                    store,
                    "PUT \"hello world\" \"value with spaces\"",
                    response),
                "PUT should keep the client session open");
        require(response == "OK", "quoted PUT should succeed");

        require(processCommand(store, "GET \"hello world\"", response),
                "GET should keep the client session open");
        require(response == "value with spaces",
                "quoted GET should return the full value");

        require(processCommand(store, "COMPACT", response),
                "COMPACT should keep the client session open");
        require(response == "OK", "COMPACT should succeed without a WAL");

        require(!processCommand(store, "EXIT", response),
                "EXIT should close the client session");
        require(response == "BYE", "EXIT should return BYE");

        std::cout << "PASSED: quoted commands and COMPACT\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}