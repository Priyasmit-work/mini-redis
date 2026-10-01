# Redis Clone (C)

A Redis server clone built from scratch in C, following the standard
"build your own Redis" project shape: raw TCP sockets → RESP protocol
parsing → command dispatch → non-blocking event loop → extended data
types.

This is a learning project. The goal is to understand how Redis-style
servers actually work under the hood — sockets, the RESP wire protocol,
and event-driven I/O — not just to reproduce Redis's feature set.

## Status

**Stage 1b complete — Basic read/write loop with epoll infrastructure.**

The server currently:
- Opens a TCP socket bound to port `6379` (Redis's default port)
- Listens for incoming connections using `epoll` event loop
- Accepts multiple client connections sequentially
- Reads data from each client and echoes a response
- Closes each connection after the first read/write cycle

The epoll infrastructure is in place but not yet fully utilized (still uses blocking accept and synchronous read/write). See [Roadmap](#roadmap) below for what's next.

## Requirements

This project targets **Linux socket APIs** (`sys/socket.h`,
`arpa/inet.h`, `epoll`), which are not available on native Windows.

- **Linux** — works natively
- **Windows** — use **WSL** (Windows Subsystem for Linux). Native
  MSYS2/MinGW builds will fail, since headers like `arpa/inet.h` don't
  exist outside a POSIX environment, and later stages depend on
  `epoll`, which is Linux-only.

Toolchain: `gcc`, `make` (install via `sudo apt install build-essential`
if missing inside WSL).

## Build & Run

```bash
make
./Redis_clone
```

The server will print:

```
Listening on port 6379......
```

and then block, waiting for a connection. Test it from a second
terminal:

```bash
nc localhost 6379
```

On a successful connection, the server prints `Client Connected !!`,
reads any data sent, prints it, and responds with "Hello from server",
then closes that connection and waits for the next client (multi-connection
loop — the server stays running).

## Project Structure

```
Redis_clone/
├── src/
│   ├── main.c        # entry point — socket/bind/listen/accept lifecycle
│   └── datatypes/     # (planned) data structure implementations
├── cpp-tools/
│   └── client/         # (planned) test client tooling
├── tests/                # (planned) test cases
├── docs/                 # design notes
├── Makefile
└── README.md
```

Note: `datatypes/`, `cpp-tools/client/`, and `tests/` are scaffolded
but intentionally unused for now. The project is being built
incrementally, stage by stage, rather than filling in the full
structure up front.

## Roadmap

- [x] **Stage 0** — sockets & TCP basics (concepts)
- [x] **Stage 1** — TCP connection lifecycle (`socket`/`bind`/`listen`/`accept`)
- [x] **Stage 1b** — `read()`/`write()` on the accepted connection; loop to accept multiple clients sequentially
- [x] **Stage 2** — parse the RESP protocol (Redis's wire format)
- [x] **Stage 3** — command dispatch (`PING`, `GET`, `SET`, `DEL`) backed by a hash table
- [x] **Stage 4** — event loop with `epoll` for handling many clients on one thread, non-blocking I/O
- [x] **Stage 5** — key expiry (`EXPIRE`/TTL), additional data types (e.g. lists)
- [x] **Stage 6** — validate against a real client (`redis-cli`)

## Known Issues / Notes

- Compiled binaries built under MSYS2/MinGW (`.exe`) are **not**
  compatible with WSL/Linux and vice versa — always rebuild with `make`
  after switching environments.
- `accept()` is currently called with `NULL, NULL` for the client
  address — the server does not yet log which client IP/port connected.
