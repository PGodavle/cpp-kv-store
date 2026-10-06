#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include "KeyValueStore.h"

int main()
{
    KeyValueStore store;
    std::string line;

    std::cout << "C++ Key-Value Store\n";
    std::cout << "Commands: PUT key value [ttl_seconds], "
             "GET key, DELETE key, EXIT\n";

    std::cout << "TTL is optional. Use 0 for no expiration.\n";
    std::cout << "Use single-word keys and values.\n";

    while (true)
    {
        std::cout << "\n> ";

        if (!std::getline(std::cin, line))
        {
            break;
        }

        std::istringstream input(line);

        std::string command;
        std::string key;
        std::string value;
        std::string extra;

        input >> command;

        if (command.empty())
        {
            continue;
        }

        if (command == "PUT")
        {
        // Key and value are required.
        if (!(input >> std::quoted(key) >> std::quoted(value)))
        {
            std::cout << "Usage: PUT key value [ttl_seconds]\n";
            continue;
        }

        int ttlSeconds = 0;
        std::string ttlText;

        // Read TTL only if another word is present.
        if (input >> ttlText)
        {
            std::istringstream ttlInput(ttlText);
            char leftover;

            if (!(ttlInput >> ttlSeconds)
                || ttlSeconds < 0
                || (ttlInput >> leftover))
            {
                std::cout << "TTL must be a non-negative integer "
                            "within the int range\n";
                continue;
            }
        }

        // Reject anything after the optional TTL.
        if (input >> extra)
        {
            std::cout << "Usage: PUT key value [ttl_seconds]\n";
            continue;
        }

        store.put(key, value, ttlSeconds);
        std::cout << "OK\n";
        }
        else if (command == "GET")
        {
            if (!(input >> std::quoted(key)) || (input >> extra))
            {
                std::cout << "Usage: GET key\n";
                continue;
            }

            if (store.get(key, value))
            {
                std::cout << value << '\n';
            }
            else
            {
                std::cout << "NOT_FOUND\n";
            }
        }
        else if (command == "DELETE")
        {
            if (!(input >> key) || (input >> extra))
            {
                std::cout << "Usage: DELETE key\n";
                continue;
            }

            if (store.remove(key))
            {
                std::cout << "DELETED\n";
            }
            else
            {
                std::cout << "NOT_FOUND\n";
            }
        }
        else if (command == "EXIT")
        {
            if (input >> extra)
            {
                std::cout << "Usage: EXIT\n";
                continue;
            }

            std::cout << "Goodbye!\n";
            break;
        }
        else
        {
            std::cout << "Unknown command\n";
        }
    }

    return 0;
}