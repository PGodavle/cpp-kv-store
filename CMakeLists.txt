cmake_minimum_required(VERSION 3.16)

project(CppKeyValueStore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(kv_store
    src/main.cpp
    src/KeyValueStore.cpp
)

target_include_directories(kv_store PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_options(kv_store PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)