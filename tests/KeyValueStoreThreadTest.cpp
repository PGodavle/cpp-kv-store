#include <functional>
#include <iostream>
#include <string>
#include <thread>

#include "KeyValueStore.h"

void insertKeys(KeyValueStore& store, int start)
{
    for (int i = start; i < start + 1000; i++)
    {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);

        store.put(key, value);
    }
}

int main()
{
    KeyValueStore store;

    // Both threads write to the same store.
    // They use different keys so we can verify every result.
    std::thread first(insertKeys, std::ref(store), 0);
    std::thread second(insertKeys, std::ref(store), 1000);

    first.join();
    second.join();

    // Both threads have finished. Check every inserted entry.
    for (int i = 0; i < 2000; i++)
    {
        std::string key = "key" + std::to_string(i);
        std::string expected = "value" + std::to_string(i);
        std::string actual;

        if (!store.get(key, actual))
        {
            std::cout << "FAILED: Missing " << key << '\n';
            return 1;
        }

        if (actual != expected)
        {
            std::cout << "FAILED: Incorrect value for " << key << '\n';
            return 1;
        }
    }

    std::cout << "PASSED: All 2000 entries are correct\n";
    return 0;
}