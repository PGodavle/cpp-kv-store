#include <sstream>
#include "CommandHandler.h"
#include <exception>
#include <iostream>

static bool processCommandImpl(
    KeyValueStore& store,
    const std::string& line,
    std::string& response)
{
    std::istringstream input(line);

    std::string command;
    std::string key;
    std::string value;
    std::string extra;

    input >> command;

    if (command == "PUT")
    {
        if (!(input >> key >> value))
        {
            response = "ERROR Usage: PUT key value [ttl_seconds]";
            return true;
        }

        int ttlSeconds = 0;
        std::string ttlText;

        if (input >> ttlText)
        {
            std::istringstream ttlInput(ttlText);
            char leftover;

            if (!(ttlInput >> ttlSeconds)
                || ttlSeconds < 0
                || (ttlInput >> leftover))
            {
                response = "ERROR Invalid TTL";
                return true;
            }
        }

        if (input >> extra)
        {
            response = "ERROR Usage: PUT key value [ttl_seconds]";
            return true;
        }

        store.put(key, value, ttlSeconds);
        response = "OK";
    }
    else if (command == "GET" || command == "DELETE")
    {
        if (!(input >> key) || (input >> extra))
        {
            response = "ERROR Usage: " + command + " key";
            return true;
        }

        if (command == "GET")
        {
            if (store.get(key, value))
            {
                response = value;
            }
            else
            {
                response = "NOT_FOUND";
            }
        }
        else
        {
            if (store.remove(key))
            {
                response = "DELETED";
            }
            else
            {
                response = "NOT_FOUND";
            }
        }
    }
    else if (command == "EXIT")
    {
        if (input >> extra)
        {
            response = "ERROR Usage: EXIT";
            return true;
        }

        response = "BYE";
        return false;
    }
    else
    {
        response = "ERROR Unknown command";
    }

    return true;
}
bool processCommand(
    KeyValueStore& store,
    const std::string& line,
    std::string& response)
{
    try
    {
        return processCommandImpl(store, line, response);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Command failed: "
                  << error.what() << '\n';

        response = "ERROR SERVER_ERROR";

        // Send the error, then close this client's session.
        return false;
    }
}