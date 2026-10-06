cmake_minimum_required(VERSION 3.16)

project(CppKeyValueStore LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(kv_store
    src/main.cpp
    src/KeyValueStore.cpp
    src/WriteAheadLog.cpp
)

target_include_directories(kv_store PRIVATE include)
target_link_libraries(kv_store PRIVATE Threads::Threads)
target_compile_options(kv_store PRIVATE -Wall -Wextra -Wpedantic)

add_executable(kv_store_thread_test
    tests/KeyValueStoreThreadTest.cpp
    src/KeyValueStore.cpp
    src/WriteAheadLog.cpp
)

target_include_directories(kv_store_thread_test PRIVATE include)
target_link_libraries(kv_store_thread_test PRIVATE Threads::Threads)
target_compile_options(kv_store_thread_test PRIVATE
    -Wall -Wextra -Wpedantic
)

add_executable(kv_server
    src/server.cpp
    src/KeyValueStore.cpp
    src/CommandHandler.cpp
    src/ConnectionQueue.cpp
    src/WriteAheadLog.cpp
)

target_include_directories(kv_server PRIVATE include)

target_link_libraries(kv_server PRIVATE Threads::Threads)

target_compile_options(kv_server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(kv_client src/client.cpp)
target_compile_options(kv_client PRIVATE -Wall -Wextra -Wpedantic)

add_executable(connection_queue_test
    tests/ConnectionQueueTest.cpp
    src/ConnectionQueue.cpp
)

target_include_directories(connection_queue_test PRIVATE include)

target_link_libraries(connection_queue_test PRIVATE Threads::Threads)

target_compile_options(connection_queue_test PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(wal_demo
    tests/WalDemo.cpp
    src/WriteAheadLog.cpp
)

target_include_directories(wal_demo PRIVATE include)

target_link_libraries(wal_demo PRIVATE Threads::Threads)

target_compile_options(wal_demo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(key_value_store_test
    tests/KeyValueStoreTest.cpp
    src/KeyValueStore.cpp
    src/WriteAheadLog.cpp
)

target_include_directories(key_value_store_test PRIVATE include)
target_link_libraries(key_value_store_test PRIVATE Threads::Threads)
target_compile_options(key_value_store_test PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_test(NAME key_value_store_test COMMAND key_value_store_test)
add_test(NAME key_value_store_thread_test COMMAND kv_store_thread_test)
add_test(NAME connection_queue_test COMMAND connection_queue_test)

add_executable(command_handler_test
    tests/CommandHandlerTest.cpp
    src/CommandHandler.cpp
    src/KeyValueStore.cpp
    src/WriteAheadLog.cpp
)

target_include_directories(command_handler_test PRIVATE include)
target_link_libraries(command_handler_test PRIVATE Threads::Threads)
target_compile_options(command_handler_test PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_test(NAME command_handler_test COMMAND command_handler_test)