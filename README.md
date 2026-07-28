# myweb

`myweb` is a small C++11 HTTP server for learning and experimenting with Linux network programming. It uses non-blocking sockets, epoll, a worker thread pool, timers, logging, and an optional MySQL connection pool.

## Features

- HTTP/1.0 and HTTP/1.1 request parsing for `GET` and `POST`.
- epoll-based event loop with configurable LT/ET trigger modes.
- Proactor and Reactor execution modes.
- Worker thread pool and connection timeout management.
- Plain-text routes for health checks and examples.
- MySQL-backed `/register` and `/login` endpoints.
- Synchronous or asynchronous logging.

## Routes

| Method | Path | Description |
| --- | --- | --- |
| `GET` | `/` | Server status message |
| `GET` | `/hello` | Hello response |
| `GET` | `/about` | Project description |
| `POST` | `/echo` | Returns the request body |
| `POST` | `/register` | Creates a user with form fields `username` and `password` |
| `POST` | `/login` | Checks a user's password |

The database endpoints expect `application/x-www-form-urlencoded` data. They use a `user(username, passwd)` table and store passwords as plain text, so the implementation is for local learning only.

## Requirements

The server depends on Linux APIs and is intended to be built on Linux:

- C++11 compiler
- GNU Make
- pthreads
- MySQL client development headers and library, such as `libmysqlclient-dev`
- `curl` for smoke tests
- `webbench` for the optional benchmark script

## Build and Run

```bash
./build.sh
./server
```

The default HTTP port is `9006`. To run without a MySQL connection pool:

```bash
./server -s 0
```

The default database settings are `127.0.0.1:3306`, user `root`, password `123456`, and database `yourdb`. Pass database settings explicitly in real environments instead of relying on these development defaults.

Example database setup:

```sql
CREATE DATABASE yourdb;
USE yourdb;
CREATE TABLE user (
    username VARCHAR(64) PRIMARY KEY,
    passwd VARCHAR(64) NOT NULL
);
```

Smoke tests:

```bash
curl -i http://127.0.0.1:9006/hello
curl -i -X POST http://127.0.0.1:9006/echo -d 'hello myweb'
curl -i -X POST http://127.0.0.1:9006/register -d 'username=alice&password=secret'
curl -i -X POST http://127.0.0.1:9006/login -d 'username=alice&password=secret'
```

## Command-Line Options

| Option | Meaning | Default |
| --- | --- | --- |
| `-p` | HTTP listen port | `9006` |
| `-l` | Log mode: `0` sync, `1` async | `0` |
| `-c` | Disable logging when set to `1` | `0` |
| `-t` | Worker thread count | `4` |
| `-T` | Connection timeout in seconds | `15` |
| `-s` | MySQL connection pool size; `0` disables it | `4` |
| `-m` | Trigger mode: `0` LT/LT, `1` LT/ET, `2` ET/LT, `3` ET/ET | `0` |
| `-a` | Actor model: `0` Proactor, `1` Reactor | `0` |
| `-H` | MySQL host | `127.0.0.1` |
| `-U` | MySQL user | `root` |
| `-W` | MySQL password | `123456` |
| `-D` | MySQL database | `yourdb` |
| `-P` | MySQL port | `3306` |

Example high-throughput configuration:

```bash
./server -p 9006 -c 1 -s 0 -m 3 -a 0 -t 8
```

## Benchmarking

`benchmark_5000.sh` runs four combinations of trigger and actor modes with `webbench`. Build the server first, then set `WB_BIN` if the benchmark binary is not at the script's default path:

```bash
WB_BIN=/path/to/webbench SERVER_BIN=./server bash benchmark_5000.sh
```

The client count, duration, thread count, and output file can be overridden with `CLIENTS`, `DURATION`, `THREADS`, and `OUT_FILE`. See [PERFORMANCE.md](PERFORMANCE.md) for recorded results and caveats.

## Project Files

- `webserver.cpp`: socket, epoll, event-loop, and mode configuration.
- `http_conn.cpp`: HTTP parsing, routing, and response construction.
- `threadpool.cpp`: worker queue and task execution.
- `timer.cpp`: connection timeout management.
- `sql_connection_pool.cpp`: MySQL connection pool and RAII wrapper.
- `logger.cpp`: synchronous and asynchronous logging.

## Security Notes

This project is not production-ready. Passwords are stored and compared as plain text, transport is unencrypted, authentication is minimal, and the default database credentials are intentionally simple development values.
