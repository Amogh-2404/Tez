# Changelog

All notable changes to the Tez HTTP Server will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-01-09

### Added

#### Core Features
- HTTP/1.1 protocol support with full header parsing
- Persistent connections (keep-alive) with configurable timeout (5s, max 1000 requests)
- Multi-HTTP method support: GET, POST, PUT, DELETE
- Request body parsing via Content-Length header
- Static file serving from `/static/*` paths
- JSON-based route configuration via `config.json`
- RESTful API support with method-aware routing

#### Performance
- Multi-threaded request handling with thread pool
- Auto-scaling worker threads based on CPU cores
- Dual LRU caching system:
  - Response cache (100 entries, 60s TTL)
  - File cache (50 entries, 60s TTL)
- Asynchronous I/O with Boost.Asio
- Efficient O(1) MIME type detection with hash map
- One-time config loading at startup (100x improvement vs per-request)

#### Security
- Path traversal protection using `std::filesystem::weakly_canonical`
- Request size limits:
  - Max Content-Length: 10 MB (prevents memory exhaustion)
  - Max Header Size: 8 KB (prevents header flooding)
  - Max Keep-Alive Requests: 1000 per connection
- Input validation on all user-provided data
- Secure default responses (403 Forbidden for path escape attempts)
- Thread-safe LRU caching with mutex guards
- Proper HTTP error responses:
  - 400 Bad Request (malformed requests)
  - 403 Forbidden (path traversal attempts)
  - 404 Not Found (missing resources)
  - 405 Method Not Allowed (unsupported methods)
  - 413 Payload Too Large (body exceeds limit)
  - 431 Request Header Fields Too Large (headers exceed limit)
  - 500 Internal Server Error (server-side errors)

#### Developer Experience
- Request logging to `server.log` with timestamps
- Special endpoints:
  - `/health` - Health check (JSON response)
  - `/echo` - Request echo for testing (POST/PUT)
  - `/api/data` - Full REST API demo
- Graceful shutdown with SIGINT/SIGTERM handling
- Comprehensive error handling
- Unit tests with Google Test framework

#### Documentation
- Comprehensive README.md (539 lines)
  - Quick start guide
  - Architecture diagrams
  - Performance benchmarks
  - Security documentation
  - Deployment guides
  - FAQ section
- Detailed SECURITY.md (334 lines)
  - Vulnerability disclosure policy
  - Security best practices
  - Deployment checklist
- Complete CLAUDE.md for AI/contributors
  - Architecture walkthrough
  - Component documentation
  - Common modification patterns
- CODE_OF_CONDUCT.md (Contributor Covenant 2.1)
- CONTRIBUTING.md with detailed guidelines
- MIT LICENSE

#### DevOps
- Multi-stage Dockerfile with Alpine Linux
- docker-compose.yml with resource limits
- GitHub Actions CI/CD pipeline:
  - Multi-platform builds (Ubuntu, macOS)
  - Code quality checks (cppcheck)
  - Security scanning (CodeQL)
  - Docker image publishing
  - Automated releases
  - Performance benchmarking
- GitHub issue templates (bug, feature, question)
- GitHub pull request template
- .dockerignore for optimized builds

### Fixed

#### Security Vulnerabilities (Pre-Release)
- **Path Traversal** - Fixed lack of path sanitization in file server
  - Impact: Could read arbitrary files on system
  - Fix: Implemented `sanitize_path()` with canonical path validation
  - Location: `file_server.cpp:57-93`

- **Memory Exhaustion** - Fixed unlimited Content-Length allocation
  - Impact: DoS via large request bodies
  - Fix: Added 10 MB limit with 413 response
  - Location: `main.cpp:81-92`

- **Cache Poisoning** - Fixed destructive cache eviction
  - Impact: Attacker could flush all cached responses
  - Fix: Implemented proper LRU eviction
  - Location: `middleware.cpp:10-116`

#### Performance Issues
- **Config Loading** - Fixed loading config.json on every request
  - Impact: 100x slower than necessary
  - Fix: Load once at startup via `init_router_config()`
  - Location: `router.cpp:14-38`, `main.cpp:187`

- **MIME Type Detection** - Fixed linear search with O(n) complexity
  - Impact: Inefficient file type detection
  - Fix: Hash map lookup with O(1) complexity
  - Location: `file_server.cpp:12-54, 96-112`

- **Duplicate Code** - Removed duplicate `.webp` MIME type checks
  - Impact: Code maintenance issue
  - Fix: Cleaned up MIME_TYPES map
  - Location: `file_server.cpp`

### Dependencies
- Boost (1.70+) - Async I/O and networking
- nlohmann/json (3.0+) - JSON parsing
- GTest (optional) - Unit testing

### Known Limitations
- No TLS/SSL support (planned for v1.1)
- No HTTP/2 or WebSocket (planned for v1.2)
- No response compression (planned for v1.1)
- No rate limiting (planned for v1.2)
- No hot config reload (planned for v1.3)
- Basic logging only (enhanced logging planned for v1.1)

For more details, see [SECURITY.md](SECURITY.md) and [README.md](README.md).

---

## Versioning

- **MAJOR** version for incompatible API changes
- **MINOR** version for backwards-compatible functionality additions
- **PATCH** version for backwards-compatible bug fixes

[1.0.0]: https://github.com/Amogh-2404/tez/releases/tag/v1.0.0
