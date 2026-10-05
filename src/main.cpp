#include <iostream>
#include <sstream>
#include <string>

#include "KeyValueStore.h"

int main()
{
    KeyValueStore store;
    std::string line;

    std::cout << "C++ Key-Value Store\n";
    std::cout << "Commands: PUT key value, GET key, DELETE key, EXIT\n";
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
            if (!(input >> key >> value) || (input >> extra))
            {
                std::cout << "Usage: PUT key value\n";
                continue;
            }

            store.put(key, value);
            std::cout << "OK\n";
        }
        else if (command == "GET")
        {
            if (!(input >> key) || (input >> extra))
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