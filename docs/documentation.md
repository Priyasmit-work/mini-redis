# Redis Clone - Project Documentation

## Overview

This is a learning project that builds a Redis-compatible server from scratch in C. The goal is to understand how Redis-style servers work: TCP sockets, RESP protocol, event-driven I/O, and data structures.

## Architecture

### Network Layer
- **TCP Server**: Listens on port 6379 (Redis default)
- **Event Loop**: Uses Linux `epoll` for scalable I/O multiplexing
- **Non-blocking**: Socket file descriptors can be set to non-blocking mode

### Data Flow
```
Client → TCP Socket → epoll → accept() → read() → process → write() → close()
```

## Key Concepts

### Socket Programming
| Function | Purpose |
|----------|---------|
| `socket()` | Create a TCP socket file descriptor |
| `bind()` | Associate socket with port 6379 |
| `listen()` | Start listening for connections |
| `accept()` | Accept incoming client connection |
| `read()` / `write()` | Read/write data on socket |

### epoll API
| Function | Purpose |
|----------|---------|
| `epoll_create1()` | Create epoll instance |
| `epoll_ctl()` | Add/remove/modify file descriptors to watch |
| `epoll_wait()` | Wait for I/O events on monitored descriptors |

### Event Flags
- `EPOLLIN` — Data available to read
- `EPOLLOUT` — Can write data
- `EPOLLET` — Edge-triggered mode (not yet used)

## Implementation Details

### main.c Structure

```
main()
├── Socket creation (socket())
├── Address setup (sockaddr_in)
├── Bind (bind())
├── Listen (listen())
├── epoll setup
│   ├── epoll_create1()
│   ├── epoll_event setup
│   └── epoll_ctl(ADD server_fd)
└── Main loop
    ├── accept() client connection
    ├── read() client data
    ├── write() response
    └── close() client
```

### Current Behavior

1. Server starts on port 6379
2. Sets up epoll to monitor listening socket
3. Enters infinite loop accepting clients
4. For each client: reads data, prints it, sends response, closes connection
5. Continues running until killed (Ctrl+C)

### Code Issues to Fix

- Line 10: `F_GETFl` should be `F_GETFL` (typo - letter l not digit 1)
- Line 16: `FSETFL` should be `F_SETFL` (missing underscore)
- Line 70: Returns `-1` instead of proper error handling for accept failure

## Running the Project

### Build
```bash
make
```

### Run Server
```bash
./Redis_clone
```

### Test with netcat
```bash
nc localhost 6379
# Type some text and press Enter
```

### Test with redis-cli (future stage)
```bash
redis-cli -p 6379
```

## RESP Protocol (Stage 2+)

Redis uses the **REdis Serialization Protocol (RESP)**:

| Type | Format | Example |
|------|--------|---------|
| Simple String | `+text\r\n` | `+OK\r\n` |
| Error | `-ERR text\r\n` | `-ERR unknown command\r\n` |
| Integer | `:number\r\n` | `:1000\r\n` |
| Bulk String | `$len\r\ndata\r\n` | `$5\r\nhello\r\n` |
| Array | `*count\r\n` | `*2\r\n$3\r\nget\r\n$3\r\nkey\r\n` |

### Common Commands to Implement
- `PING` — Simple health check
- `SET key value` — Store a key-value pair
- `GET key` — Retrieve a value by key
- `DEL key` — Delete a key

## Roadmap

1. **Stage 1b** ✅ — Read/write loop with epoll (current)
2. **Stage 2** ⏳ — RESP protocol parsing
3. **Stage 3** ⏳ — Command dispatch (PING, SET, GET, DEL)
4. **Stage 4** ⏳ — Full epoll event loop with non-blocking I/O
5. **Stage 5** ⏳ — Key expiry (TTL), additional data types
6. **Stage 6** ⏳ — Test with real redis-cli

## Project Structure

```
Redis_clone/
├── src/
│   └── main.c          # Server implementation
├── cpp-tools/
│   └── client/         # Test clients (future)
├── tests/              # Test cases (future)
├── docs/               # Design notes
├── Makefile            # Build configuration
├── README.md           # Project overview
└── documentation.md    # This file
```

## Dependencies

- **Linux** — Uses Linux-specific APIs (`epoll`, `sys/socket.h`)
- **Windows** — Use WSL (Windows Subsystem for Linux)
- **Toolchain**: gcc, make

## References

- [Redis Protocol Specification](https://redis.io/docs/reference/protocol-spec/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [epoll(7) man page](https://man7.org/linux/man-pages/man7/epoll.7.html)