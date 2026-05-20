# tiny webserver (C++)

A minimal HTTP server that accepts TCP connections, parses the HTTP request line, and returns a plain text response.

## Features

- Listen on a configurable port (default `8080`)
- Accept and handle incoming HTTP requests in a blocking loop
- Parse request line: `METHOD PATH VERSION`
- Return:
  - `200 OK` for `GET`
  - `405 Method Not Allowed` for non-`GET`
  - `400 Bad Request` for malformed request line

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/webserver_for_ai
# or choose a custom port
./build/webserver_for_ai 9090
```

## Quick test

```bash
curl -i http://127.0.0.1:8080/
curl -i -X POST http://127.0.0.1:8080/
```

