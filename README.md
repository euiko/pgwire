# PGWire - PostgreSQL Wire Protocol Server

`pgwire` is a general-purpose PostgreSQL wire protocol server implementation that can be embedded into your application or database.

By using `pgwire`, you can enable PostgreSQL-compatible client to connect to your own database, allowing remote interaction through standard PostgreSQL drivers and tools.

## Background

The PostgreSQL wire protocol is widely used and supported, making it a valuable addition to any database system that needs to interact with PostgreSQL-based tools and clients.

By integrating `pgwire`, you can provide a familiar, standardized interface, enhancing interoperability and broadening the usability of your application or own database.

Initially developed for `duckdb-pgwire` (enabling DuckDB to support remote PostgreSQL connections), `pgwire` has since been refactored into a standalone component that can be added to any environment requiring PostgreSQL-compatible server capabilities.

## Features

- [ ] Protocol
  - [x] Simple Query
  - [ ] Extended Query
  - [ ] Copy
  - [ ] Streaming Replication
- [ ] Format
  - [x] Text
  - [ ] Binary
- [ ] Startup
  - [ ] SSL
  - [x] No Authentication
  - [ ] Clear-text Authentication
  - [ ] MD5 Authentication

## Prerequisites
- C++17 compatible compiler
- CMake 3.5 or newer

## Usage

To use `pgwire`, integrate it within your application or database to expose data and functionality through the PostgreSQL protocol you can try :
1. Add this repo as your project submodule.
```bash
git submodule add https://github.com/euiko/pgwire.git <target_dir>
```
2. Add subdirectory in your `CMakeLists.txt`
```cmake
add_subdirectory(<target_dir>)
```
3. Implement and integrate `pgwire` into your codes. You can refer to [this demo program](src/demo/main.cpp).

## Demo

1. Clone the repository:

    ```bash
    git clone --recurse-submodules https://github.com/euiko/pgwire.git
    ```

2. Build the demo:

    ```bash
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE -B build -DCMAKE_BUILD_TYPE=Release .
    cmake --build build -j $(nproc) --config Release
    ```

3. Run:

    ```bash
    # show help
    ./build/src/demo/pgwire-demo --help
    # start the server
    ./build/src/demo/pgwire-demo
    ```

4. On the other terminal, connect to the server with `psql`:

    ```bash
    psql 'postgresql://localhost:15432/main' -c 'SELECT * FROM dummy_query'
    ```

5. Run tests:

    ```bash
    ctest --test-dir build/test -C Release
    ```
