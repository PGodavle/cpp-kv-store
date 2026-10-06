#include <exception>
#include <iostream>

#include "WriteAheadLog.h"

int main()
{
    try
    {
        {
            WriteAheadLog log("build/wal-demo.log");

            if (!log.append("PUT name Parikshit 0"))
            {
                std::cerr << "Could not log name\n";
                return 1;
            }

            if (!log.append("PUT city Pune 0"))
            {
                std::cerr << "Could not log city\n";
                return 1;
            }
        }

        // The first log object has closed its file.
        // Reopen it and append another record.
        {
            WriteAheadLog log("build/wal-demo.log");

            if (!log.append("DELETE city"))
            {
                std::cerr << "Could not log deletion\n";
                return 1;
            }
        }

        std::cout << "WAL records written successfully\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}