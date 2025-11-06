# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Tez** is a high-performance HTTP/1.1 web server written in C++ using Boost.Asio for asynchronous I/O. The server supports persistent connections (keep-alive), static file serving, JSON-based route configuration, in-memory caching with TTL, multi-threaded request handling via thread pool, and full REST API support (GET/POST/PUT/DELETE).

## Build and Development Commands

### Building the Project
```bash
# From project root
mkdir -p build && cd build
cmake ..
make
```

### Running the Server
```bash
# From build/ directory
./Tez
```
The server listens on port 8080 by default.

### Running Tests
```bash
# Build and run unit tests (requires GTest)
cd build
make TezTests
./TezTests
```

To install GTest:
- macOS: `brew install googletest`
- Ubuntu: `apt-get install libgtest-dev`

### Testing the Server
```bash
# Basic health check
curl http://localhost:8080/health

# Test configured routes
curl http://localhost:8080/
curl http://localhost:8080/about

# Test static file serving
curl http://localhost:8080/static/style.css

# Test POST endpoint
curl -X POST http://localhost:8080/echo -H "Content-Type: application/json" -d '{"test":"data"}'

# Test RESTful API endpoints
curl http://localhost:8080/api/data                           # GET
curl -X POST http://localhost:8080/api/data -d '{"name":"Item"}'   # POST
curl -X PUT http://localhost:8080/api/data -d '{"name":"Update"}'  # PUT
curl -X DELETE http://localhost:8080/api/data                      # DELETE
```

## Architecture

### Core Components

**Request Flow**: `main.cpp:handle_request()` → Router/FileServer → Middleware (caching/logging) → Response

1. **main.cpp** - Entry point and async connection handling with thread pool
   - `main()`: Sets up thread pool (hardware_concurrency threads), Boost.Asio io_context, TCP acceptor on port 8080, signal handling (SIGINT/SIGTERM)
   - `handle_request()`: Handles HTTP request/response cycle with keep-alive loop, executed in thread pool workers
   - Parses HTTP headers to determine keep-alive semantics (HTTP/1.1 default, Connection header override)
   - Reads request body based on Content-Length header for POST/PUT requests
   - Implements persistent connection handling with proper socket lifecycle management

2. **thread_pool.cpp/hpp** - Multi-threaded request processing
   - `ThreadPool`: Fixed-size thread pool for concurrent connection handling
   - Automatically scales to hardware concurrency (number of CPU cores)
   - Thread-safe task queue with condition variables for efficient worker coordination
   - Graceful shutdown support with proper thread joining

3. **request.cpp/hpp** - HTTP request parsing
   - `parse_request()`: Parses raw HTTP request into structured Request object
   - Extracts method, path, version, headers (case-insensitive), and body
   - `get_content_length()`: Helper to extract Content-Length from headers for body reading

4. **router.cpp/hpp** - Route handling with full HTTP method support
   - `handle_route()`: Legacy GET-only route handler, loads `config.json` to map paths to responses
   - `handle_route_with_method()`: New method-aware router supporting GET/POST/PUT/DELETE
   - Special endpoints:
     - `/health` (GET): Returns `{"status":"ok"}`
     - `/echo` (POST/PUT): Echoes back request body with metadata
     - `/api/data`: Full REST API support (GET returns list, POST creates, PUT updates, DELETE removes)
   - Returns 405 Method Not Allowed for unsupported method/path combinations
   - Falls back to config-based routing for GET requests on undefined paths
   - Integrates with response caching for GET requests

5. **file_server.cpp/hpp** - Static file serving for `/static/*` paths
   - `serve_file()`: Serves files from `static/` directory relative to build folder
   - MIME type detection based on file extension (supports CSS, HTML, JS, images, fonts, media, archives)
   - Returns 404 for missing files, integrates with file caching layer

6. **middleware.cpp/hpp** - Cross-cutting concerns
   - **Logging**: `log_request()` appends to `server.log` with timestamp, client IP, method, path
   - **Response Caching**: In-memory cache with 60-second TTL, max 100 entries
   - **File Caching**: Separate cache for static files with 60-second TTL, max 50 entries
   - Thread-safe with mutex guards for concurrent access (safe for thread pool usage)

7. **response.hpp** - Response data structure
   - Simple struct: `{status, content_type, body}`
   - Used throughout request pipeline for consistency

8. **tests/** - Unit tests for components
   - `test_router.cpp`: Tests for health endpoint, 404 handling, caching
   - `test_middleware.cpp`: Tests for logging, response/file caching, TTL expiration
   - `test_file_server.cpp`: Tests for static file serving, MIME types, 404 handling
   - `test_response.cpp`: Tests for Response struct initialization and copying
   - Built with Google Test framework (optional dependency)

### Key Implementation Details

- **Thread Pool Architecture**:
  - Fixed-size pool created at startup based on `std::thread::hardware_concurrency()`
  - Each accepted connection is enqueued to thread pool for processing
  - Workers handle complete request/response cycle including keep-alive
  - Thread-safe caches and logging for concurrent access
- **HTTP Method Support**:
  - GET: Config-based routes, static files, API reads
  - POST: Request body parsed via Content-Length header, used for resource creation
  - PUT: Request body parsing, used for resource updates
  - DELETE: Resource deletion
  - Returns 405 Method Not Allowed for unsupported combinations
- **Request Body Handling**:
  - Reads Content-Length header to determine body size
  - Allocates buffer and reads exact body length from socket
  - Body available in Request struct for route handlers
- **Path Resolution**: Routes read from `../config.json` (relative to build/ directory)
- **Static Files**: Served from `../static/` (relative to build/ directory)
- **Connection Handling**:
  - HTTP/1.1 defaults to keep-alive with timeout=5s, max=1000 requests
  - Respects client Connection header (case-insensitive)
  - Socket shutdown and close on Connection: close
  - Keep-alive loop continues until socket closes or Connection: close received
- **Async Model**: Boost.Asio async_accept with thread pool delegation
- **Error Handling**: 400 for malformed requests, 404 for missing resources, 405 for wrong method, 500 for server errors
- **Response Headers**: Includes Date (GMT), Server: Tez, Content-Length, Content-Type, Connection, Keep-Alive

### Dependencies

- **Boost** (required): Used for Boost.Asio async I/O and networking
- **nlohmann_json** (required): JSON parsing for config.json and API responses
- **GTest** (optional): Google Test framework for unit tests

## Configuration

`config.json` defines routes with structure:
```json
{
  "/path": {
    "status": "200 OK",
    "content_type": "text/html; charset=utf-8",
    "body": "..."
  }
}
```

Routes not in config.json return 404. The `/health` endpoint bypasses config and always returns JSON.

## Metrics and Logging

- **server.log**: Request logs written by middleware (format: `[timestamp] client_ip - METHOD path`)
- **metrics/**: Contains performance benchmark results from historical tests (not auto-generated)
