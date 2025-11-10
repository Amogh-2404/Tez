# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Tez** is a production-ready, high-performance HTTP/1.1 web server written in C++ using Boost.Asio for asynchronous I/O. The server supports persistent connections (keep-alive), static file serving, JSON-based route configuration, LRU in-memory caching with TTL, multi-threaded request handling via thread pool, and full REST API support (GET/POST/PUT/DELETE).

**Key Features**:
- Security-hardened with path traversal protection and request size limits
- LRU cache eviction (not destructive clear-all)
- One-time config loading at startup (not per-request)
- Efficient hash-based MIME type detection
- Thread-safe throughout with proper mutex protection

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

### Docker Deployment
```bash
# Build and run with Docker
docker build -t tez .
docker run -p 8080:8080 tez

# Or use Docker Compose
docker-compose up -d
```

### Performance Benchmarking
```bash
# Install ApacheBench
brew install httpd  # macOS
sudo apt-get install apache2-utils  # Ubuntu

# Run benchmark (10,000 requests, 100 concurrent)
ab -n 10000 -c 100 http://localhost:8080/health
```

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
   - `init_router_config()`: **MUST be called once at startup** - loads `config.json` into global memory (thread-safe)
   - `handle_route()`: GET-only route handler using pre-loaded config (no file I/O per request)
   - `handle_route_with_method()`: Method-aware router supporting GET/POST/PUT/DELETE
   - Special endpoints:
     - `/health` (GET): Returns `{"status":"ok"}`
     - `/echo` (POST/PUT): Echoes back request body with metadata
     - `/api/data`: Full REST API support (GET returns list, POST creates, PUT updates, DELETE removes)
   - Returns 405 Method Not Allowed for unsupported method/path combinations
   - Falls back to config-based routing for GET requests on undefined paths
   - Integrates with LRU response caching for GET requests
   - **Critical**: Config is loaded ONCE at startup, not per-request (100x performance improvement)

5. **file_server.cpp/hpp** - Static file serving for `/static/*` paths
   - `sanitize_path()`: **Security-critical** - prevents path traversal attacks using `std::filesystem::weakly_canonical`
   - Blocks `../`, `~`, absolute paths, drive letters - returns 403 Forbidden for invalid paths
   - `get_mime_type()`: Efficient O(1) hash map lookup (not linear search)
   - `serve_file()`: Serves files from `static/` directory relative to build folder
   - MIME type detection based on file extension (supports CSS, HTML, JS, images, fonts, media, archives)
   - Returns 404 for missing files, 403 for path escape attempts
   - Integrates with LRU file caching layer

6. **middleware.cpp/hpp** - Cross-cutting concerns
   - **LRU Cache Implementation**: Template-based LRU cache with list + unordered_map
   - O(1) get/put operations, evicts least-recently-used (not clear-all)
   - Prevents cache poisoning attacks by evicting oldest entries only
   - **Logging**: `log_request()` appends to `server.log` with timestamp, client IP, method, path
   - **Response Caching**: LRU cache with 60-second TTL, max 100 entries
   - **File Caching**: Separate LRU cache for static files with 60-second TTL, max 50 entries
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

- **Security Limits** (main.cpp:20-24):
  - `MAX_CONTENT_LENGTH = 10 MB`: Prevents memory exhaustion attacks
  - `MAX_HEADER_SIZE = 8 KB`: Prevents header flooding
  - `MAX_KEEPALIVE_REQUESTS = 1000`: Prevents infinite connection abuse
  - All limits are configurable constants at top of main.cpp

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

- **Request Body Handling** (with security validation):
  - Validates Content-Length before allocating memory (prevents DoS)
  - Returns 413 Payload Too Large if body exceeds MAX_CONTENT_LENGTH
  - Returns 431 Request Header Fields Too Large if headers exceed MAX_HEADER_SIZE
  - Reads exact body length from socket after validation
  - Body available in Request struct for route handlers

- **Path Resolution**: Routes read from `../config.json` (relative to build/ directory)
  - **IMPORTANT**: `init_router_config()` must be called in main() before accepting connections
  - Config is loaded once into global `g_config` with mutex protection

- **Static Files**: Served from `../static/` (relative to build/ directory)
  - **SECURITY**: All paths sanitized through `sanitize_path()` before file access
  - Canonical path checking prevents escaping static directory

- **Connection Handling**:
  - HTTP/1.1 defaults to keep-alive with timeout=5s, max=1000 requests
  - Respects client Connection header (case-insensitive)
  - Request counter prevents infinite keep-alive abuse
  - Socket shutdown and close on Connection: close or max requests reached
  - Keep-alive loop continues until socket closes or Connection: close received

- **Async Model**: Boost.Asio async_accept with thread pool delegation

- **Error Handling**:
  - 400 Bad Request: Malformed requests, invalid Content-Length
  - 403 Forbidden: Path traversal attempts
  - 404 Not Found: Missing resources
  - 405 Method Not Allowed: Wrong HTTP method
  - 413 Payload Too Large: Body exceeds limit
  - 431 Request Header Fields Too Large: Headers exceed limit
  - 500 Internal Server Error: Server-side errors

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

## Security Considerations

When modifying code, maintain these security properties:

1. **Path Sanitization**: Always use `sanitize_path()` for user-provided file paths
2. **Size Validation**: Check Content-Length and header sizes before allocating memory
3. **LRU Eviction**: Never use cache.clear() - always use LRU eviction to prevent cache poisoning
4. **Thread Safety**: All shared state must be protected with mutexes
5. **Config Loading**: `init_router_config()` must be called exactly once at startup
6. **Input Validation**: Validate all user input before processing

## Common Modification Patterns

### Adding a New Route
```cpp
// In router.cpp, handle_route_with_method()
if (path == "/myroute") {
    if (method == "GET") {
        resp.status = "200 OK";
        resp.content_type = "application/json";
        resp.body = "{\"data\":\"value\"}\n";
    } else {
        resp.status = "405 Method Not Allowed";
        // ...
    }
    return resp;
}
```

### Changing Security Limits
```cpp
// In main.cpp (lines 20-24)
constexpr size_t MAX_CONTENT_LENGTH = 20 * 1024 * 1024;  // 20 MB
constexpr size_t MAX_HEADER_SIZE = 16 * 1024;             // 16 KB
constexpr size_t MAX_KEEPALIVE_REQUESTS = 2000;           // 2000 requests
```

### Adjusting Cache Sizes
```cpp
// In middleware.cpp (lines 85-86)
LRUCache<Response> cache(200, 60);       // 200 entries, 60s TTL
LRUCache<Response> file_cache(100, 60);  // 100 entries, 60s TTL
```

### Adding MIME Types
```cpp
// In file_server.cpp, MIME_TYPES map (lines 12-54)
static const std::unordered_map<std::string, std::string> MIME_TYPES = {
    // Add new type here
    {".wasm", "application/wasm"},
    // ...
};
```

## Metrics and Logging

- **server.log**: Request logs written by middleware (format: `[timestamp] client_ip - METHOD path`)
- **metrics/**: Contains performance benchmark results from historical tests (not auto-generated)

## CI/CD and Deployment

- **GitHub Actions**: `.github/workflows/ci.yml` - Multi-platform builds, tests, Docker, security scans
- **Docker**: Multi-stage Dockerfile with Alpine Linux, non-root user, health checks
- **docker-compose.yml**: Production-ready setup with resource limits and security options
