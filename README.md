# Tez - High-Performance HTTP/1.1 Web Server

<div align="center">

**A lightweight, high-performance HTTP/1.1 web server written in modern C++**

[![CI/CD Pipeline](https://github.com/Amogh-2404/Tez/actions/workflows/ci.yml/badge.svg?branch=v1)](https://github.com/Amogh-2404/Tez/actions/workflows/ci.yml)
[![Docker Pulls](https://img.shields.io/docker/pulls/ramogh2404/tez)](https://hub.docker.com/r/ramogh2404/tez)
[![GitHub Release](https://img.shields.io/github/v/release/Amogh-2404/Tez)](https://github.com/Amogh-2404/Tez/releases/latest)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

[Features](#features) •
[Quick Start](#quick-start) •
[Documentation](#documentation) •
[Performance](#performance) •
[Contributing](#contributing)

</div>

---

## Overview

Tez is a production-ready, asynchronous HTTP/1.1 web server built with C++ and Boost.Asio. Designed for **developers who need embedded HTTP server capabilities** in their C++ applications, Tez provides excellent performance, security, and ease of use without the complexity of larger web servers.

### Why Tez?

- **🚀 High Performance**: Async I/O with thread pool architecture, handling 10,000+ req/sec
- **🔒 Secure by Default**: Path traversal protection, request size limits, LRU caching
- **⚡ Low Latency**: In-memory caching with 60s TTL, optimized MIME type detection
- **🛠️ Developer Friendly**: Simple JSON configuration, comprehensive logging
- **📦 Lightweight**: ~700 lines of clean C++17 code, minimal dependencies
- **🔧 Hackable**: Clear architecture, easy to understand and modify

Perfect for:
- Embedded systems and IoT devices
- Game servers requiring HTTP APIs
- Microservices written in C++
- Educational projects learning async I/O
- Performance-critical applications

---

## ⚠️ Production Use Notice

Tez is an **educational-quality HTTP server** designed for learning, embedded systems, and portfolio projects.

**✅ Great For:**
- Learning async I/O and C++ networking patterns
- Embedded systems and IoT prototypes (internal networks)
- Portfolio and interview demonstrations
- Understanding web server internals
- Educational projects and tutorials

**❌ NOT Recommended for Public-Facing Production:**
- **No TLS/SSL support** - All traffic is unencrypted (use reverse proxy if needed)
- **Performance limitations** - 3-10x slower than production servers
- **Missing features** - No HTTP/2, WebSocket, or compression

**For production web services**, consider battle-tested alternatives:
- [nginx](https://nginx.org/) or [Caddy](https://caddyserver.com/) - Industry-standard reverse proxies
- [Drogon](https://github.com/drogonframework/drogon) - Full-featured C++ framework (HTTP/2, WebSocket)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) - Header-only, zero dependencies, TLS support

---

## Features

### Core HTTP Support
- ✅ **HTTP/1.1 Protocol** with full header parsing
- ✅ **Persistent Connections** (Keep-Alive) with configurable timeout
- ✅ **Multiple HTTP Methods**: GET, POST, PUT, DELETE
- ✅ **Request Body Parsing** via Content-Length header
- ✅ **Static File Serving** from `/static/*` paths
- ✅ **JSON-based Routing** via `config.json`
- ✅ **RESTful API Support** with method-aware routing

### Performance Features
- ⚡ **Multi-threaded Request Handling** with thread pool
- ⚡ **Auto-scaling Workers** based on CPU cores
- ⚡ **Dual LRU Caching System**:
  - Response cache (100 entries, 60s TTL)
  - File cache (50 entries, 60s TTL)
- ⚡ **Asynchronous I/O** with Boost.Asio
- ⚡ **Efficient MIME Type Detection** with hash map lookup
- ⚡ **One-time Config Loading** at startup

### Security Features
- 🔒 **Path Traversal Protection** with sanitized file paths
- 🔒 **Request Size Limits**:
  - Max Content-Length: 10 MB
  - Max Header Size: 8 KB
  - Max Keep-Alive Requests: 1000
- 🔒 **Input Validation** on all user-provided data
- 🔒 **Secure Default Responses** (403 Forbidden for invalid paths)
- 🔒 **Thread-safe Caching** with mutex guards

### Developer Features
- 🛠️ **Request Logging** to `server.log` with timestamps
- 🛠️ **Special Endpoints**:
  - `/health` - Health check (JSON)
  - `/echo` - Request echo (POST/PUT)
  - `/api/data` - Full REST API demo
- 🛠️ **Graceful Shutdown** (SIGINT/SIGTERM handling)
- 🛠️ **Comprehensive Error Handling** with proper HTTP status codes
- 🛠️ **Unit Tests** with Google Test framework

---

## Quick Start

### Prerequisites

- **C++17 compiler** (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake 3.10+**
- **Boost** (1.70+) for Asio
- **nlohmann/json** for JSON parsing
- **Google Test** (optional, for unit tests)

### Installation

#### macOS
```bash
brew install boost nlohmann-json cmake
brew install googletest  # Optional, for tests
```

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libboost-all-dev nlohmann-json3-dev
sudo apt-get install libgtest-dev  # Optional, for tests
```

### Build & Run

```bash
# Clone the repository
git clone https://github.com/Amogh-2404/tez.git
cd tez

# Build
mkdir -p build && cd build
cmake ..
make

# Run the server
./Tez
```

The server will start on `http://localhost:8080` 🎉

### First Request

```bash
# Health check
curl http://localhost:8080/health
# {"status":"ok"}

# Serve static file
curl http://localhost:8080/static/style.css

# Test POST endpoint
curl -X POST http://localhost:8080/echo \
  -H "Content-Type: application/json" \
  -d '{"message":"Hello, Tez!"}'

# REST API
curl http://localhost:8080/api/data
# {"data":["item1","item2","item3"]}
```

---

## Documentation

### Configuration

Routes are defined in `config.json` (located in project root):

```json
{
  "/": {
    "status": "200 OK",
    "content_type": "text/html; charset=utf-8",
    "body": "<h1>Welcome to Tez!</h1>"
  },
  "/about": {
    "status": "200 OK",
    "content_type": "text/html; charset=utf-8",
    "body": "<h1>About Tez</h1><p>High-performance C++ web server</p>"
  }
}
```

### Static Files

Place static files in the `static/` directory:

```
static/
├── style.css
├── script.js
├── images/
│   └── logo.png
└── index.html
```

Access via: `http://localhost:8080/static/style.css`

Supported MIME types: HTML, CSS, JS, PNG, JPEG, GIF, SVG, WebP, MP4, MP3, PDF, ZIP, fonts, and more.

### Special Endpoints

#### Health Check
```bash
GET /health
```
Returns: `{"status":"ok"}`

#### Echo Endpoint
```bash
POST /echo
Content-Type: application/json

{"test": "data"}
```
Returns:
```json
{
  "method": "POST",
  "received_body": "{\"test\": \"data\"}",
  "body_length": 16
}
```

#### REST API Demo
```bash
# List resources
GET /api/data

# Create resource
POST /api/data -d '{"name":"New Item"}'

# Update resource
PUT /api/data -d '{"name":"Updated Item"}'

# Delete resource
DELETE /api/data
```

### Architecture

```
Client Request
     ↓
TCP Acceptor (port 8080)
     ↓
Thread Pool Workers (N = CPU cores)
     ↓
handle_request()
     ├→ Parse HTTP headers
     ├→ Validate request size
     ├→ Read request body
     ├→ Route to handler:
     │    ├→ /static/* → FileServer (with cache)
     │    ├→ /health → Health endpoint
     │    ├→ /echo → Echo endpoint
     │    ├→ /api/* → REST API
     │    └→ Other → Router (with cache)
     ↓
Response
     ├→ Build HTTP response
     ├→ Set headers (Date, Server, Connection, Keep-Alive)
     ├→ Send to client
     └→ Loop for keep-alive or close
```

**Key Components:**
- **main.cpp**: Entry point, async connection handling, thread pool management
- **router.cpp**: Route handling, config loading, method-aware routing
- **file_server.cpp**: Static file serving, path sanitization, MIME detection
- **middleware.cpp**: Logging, LRU caching (response + file)
- **thread_pool.cpp**: Fixed-size thread pool for concurrent requests
- **request.cpp**: HTTP request parsing

---

## Performance

### Benchmarks

Tested on MacBook Pro (M1, 8 cores):

| Scenario | Requests/sec | Avg Latency | p99 Latency |
|----------|-------------|-------------|-------------|
| `/health` (cached) | ~10,896 | 0.5ms | 2ms |
| Static file (cached) | ~8,500 | 1.2ms | 5ms |
| JSON POST echo | ~7,200 | 1.8ms | 8ms |
| Config route (cached) | ~10,500 | 0.6ms | 3ms |

**Configuration**: Default settings, 8 worker threads

### Running Benchmarks

```bash
# Install ApacheBench
brew install httpd  # macOS
sudo apt-get install apache2-utils  # Ubuntu

# Run benchmark
ab -n 10000 -c 100 http://localhost:8080/health
```

### Optimization Tips

1. **Increase worker threads** for CPU-bound workloads
2. **Enable file caching** for frequently accessed static files
3. **Use config.json** for simple routes instead of file I/O
4. **Tune cache sizes** in middleware.cpp (default: 100 response, 50 file)
5. **Adjust keep-alive limits** in main.cpp (timeout, max requests)

---

## Security

### Security Features

Tez includes multiple layers of security protection:

1. **Path Traversal Protection**
   - Sanitizes all file paths
   - Prevents directory escape (`../`, `~`, etc.)
   - Validates paths stay within static directory

2. **Request Size Limits**
   - Content-Length: 10 MB max (configurable in main.cpp:21)
   - Header size: 8 KB max (configurable in main.cpp:22)
   - Keep-alive requests: 1000 max per connection

3. **Input Validation**
   - Validates Content-Length header
   - Rejects malformed requests (400 Bad Request)
   - Returns 413 Payload Too Large for oversized bodies

4. **LRU Cache Security**
   - Prevents cache poisoning attacks
   - Evicts least-recently-used entries (not all entries)
   - Thread-safe with mutex protection

### Reporting Security Issues

Please report security vulnerabilities to: [ramogh2404@gmail.com](mailto:ramogh2404@gmail.com)

See [SECURITY.md](SECURITY.md) for our security policy and disclosure process.

---

## Testing

### Running Tests

```bash
cd build
make TezTests  # Build test suite
./TezTests     # Run all tests
```

### Test Coverage

Current test files:
- `test_router.cpp`: Health endpoint, 404 handling, caching
- `test_middleware.cpp`: Logging, LRU caching, TTL expiration
- `test_file_server.cpp`: Static serving, MIME types, path security
- `test_response.cpp`: Response struct initialization

### Manual Testing

```bash
# Test keep-alive
curl -v http://localhost:8080/health

# Test large POST (should fail at 10MB+)
dd if=/dev/zero bs=1M count=11 | curl -X POST http://localhost:8080/echo --data-binary @-

# Test path traversal (should return 403)
curl http://localhost:8080/static/../../etc/passwd

# Test concurrent requests
seq 1 100 | xargs -I{} -P 10 curl -s http://localhost:8080/health > /dev/null
```

---

## Deployment

### Docker (Recommended)

```dockerfile
FROM alpine:latest
RUN apk add --no-cache boost-dev nlohmann-json g++ cmake make

WORKDIR /app
COPY . .
RUN mkdir -p build && cd build && cmake .. && make

EXPOSE 8080
CMD ["./build/Tez"]
```

Build and run:
```bash
docker build -t tez .
docker run -p 8080:8080 tez
```

### Systemd Service (Linux)

Create `/etc/systemd/system/tez.service`:

```ini
[Unit]
Description=Tez Web Server
After=network.target

[Service]
Type=simple
User=www-data
WorkingDirectory=/opt/tez
ExecStart=/opt/tez/build/Tez
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable tez
sudo systemctl start tez
sudo systemctl status tez
```

---

## Roadmap

### Phase 1 (Complete ✅)
- [x] HTTP/1.1 protocol support
- [x] Persistent connections (keep-alive)
- [x] Multi-threaded request handling
- [x] LRU caching system
- [x] Security hardening (path traversal, size limits)

### Phase 2 (In Progress 🚧)
- [ ] TLS/SSL support (HTTPS)
- [ ] Response compression (gzip/brotli)
- [ ] YAML configuration format
- [ ] Command-line argument parsing
- [ ] Structured logging with levels

### Phase 3 (Planned 📋)
- [ ] HTTP/2 support
- [ ] WebSocket support
- [ ] Reverse proxy capabilities
- [ ] Rate limiting per IP
- [ ] Metrics export (Prometheus)

### Phase 4 (Future 🔮)
- [ ] Plugin system
- [ ] Hot config reload
- [ ] Admin dashboard
- [ ] Load balancing
- [ ] Header-only library option

---

## Contributing

Contributions are welcome! Here's how you can help:

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/amazing-feature`
3. **Make your changes** and add tests
4. **Run the test suite**: `make TezTests && ./TezTests`
5. **Commit your changes**: `git commit -m 'Add amazing feature'`
6. **Push to the branch**: `git push origin feature/amazing-feature`
7. **Open a Pull Request**

### Code Style

- Follow modern C++17 conventions
- Use RAII for resource management
- Include comments for complex logic
- Add unit tests for new features
- Keep functions under 50 lines when possible

### Development Setup

```bash
# Install development dependencies
brew install clang-format cmake-format

# Format code
clang-format -i src/*.cpp include/*.hpp

# Run linter
cppcheck src/ include/
```

---

## FAQ

**Q: How do I change the port?**
A: Edit `main.cpp:196` and change `8080` to your desired port. Rebuild with `make`.

**Q: Can I use Tez in production?**
A: Tez is production-ready for embedded use cases, but consider adding TLS/SSL for public-facing deployments (coming in Phase 2).

**Q: How do I increase cache size?**
A: Edit `middleware.cpp:85-86` to change cache limits (default: 100 response, 50 file entries).

**Q: Does Tez support HTTPS?**
A: Not yet. TLS/SSL support is planned for Phase 2. For now, use a reverse proxy (nginx, Caddy) for HTTPS.

**Q: How do I enable debug logging?**
A: Logging is currently minimal. Enhanced structured logging is planned for Phase 2.

**Q: Can I use Tez as a library?**
A: Not currently, but a header-only option is planned for Phase 4.

---

## License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## Acknowledgments

- Built with [Boost.Asio](https://www.boost.org/doc/libs/release/libs/asio/) for async I/O
- JSON parsing by [nlohmann/json](https://github.com/nlohmann/json)
- Testing with [Google Test](https://github.com/google/googletest)

---

## Contact

- **Issues**: [GitHub Issues](https://github.com/Amogh-2404/tez/issues)
- **Security**: [ramogh2404@gmail.com](mailto:ramogh2404@gmail.com)

---

<div align="center">

**Made by developers, for developers** 🚀

If you find Tez useful, please consider giving it a ⭐ on GitHub!

</div>
