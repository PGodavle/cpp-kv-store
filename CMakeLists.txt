cmake_minimum_required(VERSION 3.16)

project(CppKeyValueStore LANGUAGES CXX)

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