# C++ Key-Value Store

A C++17 key-value store project exploring concurrency, TCP networking, and durable local storage.

## Features

- `PUT`, `GET`, and `DELETE` operations with optional TTL.
- Mutex-protected in-memory storage shared by concurrent server workers.
- TCP server with a fixed-size worker pool and bounded connection queue.
- Write-ahead log (WAL) with synchronous writes and startup recovery.
- Snapshots and WAL compaction, triggered with the `COMPACT` command.
- Quoted keys and values can contain spaces; unquoted single-word commands remain supported.
- Interactive local CLI and a TCP client.
- CTest coverage for storage recovery, command parsing, thread safety, and queue behavior.

## Requirements

- Linux (the server uses Linux APIs such as `signalfd`).
- A C++17-compatible compiler.
- CMake 3.16 or newer.
- Make or another CMake-supported build tool.

## Build and Test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the local in-memory CLI:

```bash
./build/kv_store
```

Run the server and client in separate terminals:

```bash
./build/kv_server
./build/kv_client
```

The server defaults to `127.0.0.1:9090` and stores data in `data/store.wal`.
It creates a sibling `.snapshot` file during compaction and a `.lock` file to prevent multiple servers from using the same WAL.

Server options:

```text
kv_server [--host IPv4] [--port PORT] [--workers COUNT]
          [--queue-capacity COUNT] [--wal PATH]
```

Defaults are host `127.0.0.1`, port `9090`, four workers, queue capacity `8`, and WAL path `data/store.wal`. Use `--help` to print the usage line. The server accepts an IPv4 address; bind to `0.0.0.0` only when access from other machines is intended.

The client accepts an optional host and port:

```bash
./build/kv_client [HOST [PORT]]
```

## Commands

Commands are uppercase. Keys and values are case-sensitive. Quote tokens containing spaces; backslash and quote characters can be escaped inside quoted tokens.

| Command | Description |
|---------|-------------|
| `PUT key value [ttl_seconds]` | Insert or replace a value; TTL is optional and must be non-negative. |
| `GET key` | Return the value or `NOT_FOUND`. |
| `DELETE key` | Delete a key or return `NOT_FOUND`. |
| `COMPACT` | Write a snapshot and replace the WAL with a checkpoint. |
| `EXIT` | End the TCP client session. |

Example:

```text
PUT "first name" "Ada Lovelace"
OK
GET "first name"
Ada Lovelace
PUT session temporary 60
COMPACT
OK
EXIT
BYE
```

The local CLI supports `PUT`, `GET`, `DELETE`, and `EXIT`. Its data is in-memory only; persistence is enabled by the TCP server.

## Storage Behavior

Mutations are appended to the WAL and synchronized before the in-memory map is updated. On startup, the store loads the latest snapshot and replays later WAL records. TTL expiration uses a monotonic clock while running and persisted wall-clock expiry timestamps for recovery. Expired entries are removed lazily when accessed; compaction omits already-expired entries.

Compaction is currently manual through the TCP `COMPACT` command. The project does not yet implement automatic compaction, eviction, a total storage limit, authentication, or TLS. The newline-delimited protocol does not support literal newline characters inside a key or value.