# C++ Key-Value Store

A key-value store built from scratch in C++17 to explore storage,
concurrency, networking, and persistence.

The current version provides an in-memory store with an interactive
command-line interface.

## Current Features

- Insert and update values using `PUT`.
- Retrieve values using `GET`.
- Delete entries using `DELETE`.
- Report missing keys.
- Validate command arguments.
- Mutex-protected storage operations.
- Optional TTL with lazy expiration.
- TCP server supporting PUT, GET, DELETE, and EXIT.
- Newline-delimited commands with partial-read/write handling.
- Shared in-memory data across successive client connections.

## Requirements

- Linux or Ubuntu on WSL2
- A C++17-compatible compiler
- CMake 3.16 or newer
- Make

Install the development tools on Ubuntu:

```bash
sudo apt update
sudo apt install build-essential gdb cmake git
```

## Build and Run

Run these commands from the project directory:

```bash
cmake -S . -B build
cmake --build build
./build/kv_store
```

## Supported Commands

| Command | Description |
|---------|-------------|
| `PUT key value` | Insert a key or replace its existing value |
| `GET key` | Print the value, or `NOT_FOUND` |
| `DELETE key` | Print `DELETED`, or `NOT_FOUND` |
| `EXIT` | Exit the program |

Commands are uppercase. Keys and values are case-sensitive and
must be single words in this version.

### Example Session

```text
> PUT name Parikshit
OK

> GET name
Parikshit

> PUT name Rudra
OK

> GET name
Rudra

> DELETE name
DELETED

> GET name
NOT_FOUND

> EXIT
Goodbye!
```

## Project Structure

```text
cpp-kv-store/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── include/
│   └── KeyValueStore.h
└── src/
    ├── KeyValueStore.cpp
    └── main.cpp
```

## Implementation

- `KeyValueStore` manages data using
  `std::unordered_map<std::string, std::string>`.
- `main.cpp` handles command parsing and terminal output.
- `get` returns a boolean and copies the retrieved value into an
  output parameter. When the key is missing, the output parameter
  remains unchanged.
- The storage class does not perform terminal input or output.

## Current Limitations

- The TCP server serves one connected client at a time.
- All data is lost when the server stops.
- Expired entries remain in memory until accessed or deleted.
- Keys and values must be single words.
- No eviction or configured total storage limit.

## Planned Improvements

- [ ] Thread-safe access
- [ ] Automated tests
- [ ] TTL expiration
- [ ] TCP server and client
- [ ] Thread pool and bounded request queue
- [ ] Write-ahead logging and crash recovery
- [ ] Snapshots
- [ ] LRU eviction
- [ ] Performance benchmarks