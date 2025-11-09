# Reddit Launch Posts

## r/cpp Post (Primary Target)

**Timing**: Tuesday-Thursday, 10am-2pm EST
**Flair**: Show & Tell / Project

### Title
```
Tez: An educational HTTP/1.1 server in modern C++ with production-quality documentation
```

### Post Content
```markdown
Hi r/cpp! I've been working on Tez, an HTTP/1.1 web server written in modern C++17 that demonstrates async I/O patterns with production-level documentation.

**Why I built this:**

Most C++ HTTP servers are either too simple (toy examples with 50 lines) or too complex (50k+ LOC enterprise frameworks like Drogon). Tez aims to be the middle ground - clean, readable code (~2,000 LOC) that shows how to build a real server with security and performance in mind.

**Key Features:**
- ✅ Async I/O with Boost.Asio + thread pool architecture
- ✅ LRU caching, path traversal protection, request size limits
- ✅ Comprehensive docs: 539-line README, SECURITY.md, architecture guide
- ✅ Docker deployment, GitHub Actions CI/CD
- ✅ Full REST API support (GET/POST/PUT/DELETE)

**What makes it different:**
- **Educational focus**: Every component is documented and explained
- **Security-conscious**: Explicit vulnerability history, deployment best practices
- **Modern C++**: RAII, smart pointers, std::filesystem, proper thread safety

**NOT for production web services** (no TLS, slower than Drogon/cpp-httplib), but great for:
- 📚 Learning async networking and C++ patterns
- 🔧 Embedded/IoT projects (Raspberry Pi, internal networks)
- 💼 Portfolio demonstrations
- 🎓 Understanding web server internals

**Some highlights from the codebase:**
- LRU cache implementation with O(1) operations
- Path sanitization using filesystem canonical paths
- Security limits clearly documented with line numbers
- Config loaded once at startup (not per-request!)

**Links:**
- GitHub: https://github.com/Amogh-2404/tez
- Documentation: https://github.com/Amogh-2404/tez#documentation
- Docker: `docker run -p 8080:8080 Amogh-2404/tez`

I'd love feedback on:
- Architecture decisions (thread pool vs other async patterns)
- Code clarity - is it easy to understand?
- Documentation quality
- What features would make it more useful for learning?

The project is MIT licensed and welcoming contributions. If you're interested in learning how web servers work under the hood, this might be a good place to start!

[I'll attach a screenshot of the README or architecture diagram]
```

**Post-Submission Strategy:**
- Reply to every comment within 4 hours
- Be humble about limitations
- Emphasize learning value over production use
- Share specific code examples when asked
- Thank people for feedback

---

## r/programming Post

**Timing**: Wednesday, 12pm EST
**Flair**: Project

### Title
```
Show /r/programming: Built an educational HTTP server in C++ to demonstrate async I/O patterns
```

### Post Content
```markdown
I built Tez, an educational HTTP/1.1 server in C++, to fill a gap I noticed: there aren't many well-documented, intermediate-complexity examples of async web servers.

**The Problem:**
When learning networking in C++, you either get:
- Tutorial examples: 50 lines, no error handling, no security
- Production servers: 50,000+ lines, enterprise complexity

**The Solution:**
A real HTTP server (~2,000 LOC) with:
- Production-quality security (path traversal protection, size limits)
- Modern patterns (async I/O, thread pool, LRU caching)
- Comprehensive documentation (every decision explained)
- Small enough to understand in an afternoon

**Use Cases:**
- Learning: Understand async I/O, thread pools, HTTP protocol
- Embedded: IoT devices, Raspberry Pi projects
- Portfolios: Demonstrate C++ skills with real patterns

**Tech Stack:**
- Modern C++17 (RAII, smart pointers, std::filesystem)
- Boost.Asio for async I/O
- Thread pool for concurrency
- Docker deployment ready

**NOT meant to replace nginx/Caddy** - it's optimized for learning and embedded prototypes. No TLS, no HTTP/2, slower performance.

**What I learned building it:**
- Security is hard (spent weeks on path traversal protection)
- Documentation is as important as code
- Clear architecture makes contributions easier

GitHub: https://github.com/Amogh-2404/tez

What would you add to make it more useful as a learning resource?
```

---

## r/embedded Post

**Timing**: Thursday, 11am EST
**Flair**: Project

### Title
```
Lightweight HTTP server for embedded systems (2K LOC, Boost.Asio, Docker-ready)
```

### Post Content
```markdown
I built Tez, a lightweight HTTP/1.1 server for embedded systems and IoT projects.

**Why another HTTP server?**

I needed something for Raspberry Pi projects that was:
- ✅ Small and understandable (<2,000 LOC)
- ✅ Secure (path traversal protection, size limits)
- ✅ Well-documented (every component explained)
- ✅ Easy to deploy (Docker + systemd)

**Key Features for Embedded:**
- Auto-scaling thread pool (adapts to available cores)
- LRU caching (60s TTL, configurable)
- Static file serving with MIME detection
- RESTful API support (GET/POST/PUT/DELETE)
- Request size limits (prevents memory exhaustion)
- Graceful shutdown handling

**Performance:**
- ~10,000 req/sec on M1 Mac (8 cores)
- ~50 MB Docker image (Alpine-based)
- Minimal dependencies (Boost + nlohmann_json)

**Example Use Cases:**
- Raspberry Pi sensor data APIs
- IoT device configuration interfaces
- Internal network services
- Prototype REST APIs

**Deployment:**
```bash
# Docker (easiest)
docker run -p 8080:8080 Amogh-2404/tez

# Or build from source
git clone https://github.com/Amogh-2404/tez
cd tez && mkdir build && cd build
cmake .. && make
./Tez
```

**Security Notes:**
- NO TLS (use reverse proxy for public internet)
- Designed for internal/trusted networks
- Request size limits prevent DoS
- Path sanitization prevents directory traversal

**Documentation:**
- Full deployment guide (Docker, systemd)
- Security best practices
- Architecture walkthrough
- Example configurations

GitHub: https://github.com/Amogh-2404/tez

What features would make this more useful for your embedded projects?
```

---

## r/learnprogramming Post (Optional)

**Timing**: Friday, 2pm EST
**Flair**: Resource

### Title
```
I built an HTTP server in C++ as a learning resource - feedback welcome
```

### Post Content
```markdown
I created Tez, an educational HTTP/1.1 server to help people learn async I/O and C++ networking.

**Goal:** Bridge the gap between toy examples and production complexity

**What it teaches:**
- Async I/O patterns (Boost.Asio)
- Thread pool architecture
- HTTP protocol implementation
- Security considerations (path traversal, size limits)
- Caching strategies (LRU)
- Modern C++ patterns (RAII, smart pointers)

**Documentation includes:**
- Commented codebase (~2,000 LOC)
- Architecture diagrams with request flow
- Security decisions explained
- Common modification patterns
- Deployment guides

**Learning Path:**
1. Read README architecture section
2. Follow request flow in main.cpp
3. Understand thread pool in thread_pool.cpp
4. Study security in file_server.cpp
5. Modify and extend with your features

GitHub: https://github.com/Amogh-2404/tez

Is this helpful for learning? What would make it better?
```

---

## Post-Launch Monitoring

**Track these metrics:**
- Upvotes/downvotes
- Comment sentiment
- Questions/concerns
- Traffic spikes on GitHub

**Response Strategy:**
- Reply to every comment (even negative ones)
- Be humble and acknowledge limitations
- Share code examples when asked
- Direct security questions to SECURITY.md
- Thank people for suggestions

**Common Questions (Pre-prepare Answers):**

**Q: "Why not just use cpp-httplib?"**
A: cpp-httplib is excellent for production! Tez is optimized for learning - every decision is documented, architecture is transparent. Think of it as a tutorial that happens to work.

**Q: "No TLS? That's insecure!"**
A: Absolutely right. Tez is for learning and embedded (internal networks). For internet-facing services, use nginx/Caddy or cpp-httplib. This is clearly documented in the README.

**Q: "Only 10k req/sec? That's slow."**
A: Correct. Production servers like Drogon hit 100k+ req/sec. Tez prioritizes code clarity over performance. It's fast enough for learning and embedded use cases.

**Q: "Why Boost? That's a heavy dependency."**
A: Fair point. I chose Boost.Asio because it's industry-standard for async I/O and well-documented. A future version might offer a no-dependency option.

**Q: "This is just a toy project."**
A: Partly true! It's a learning resource first. But the security hardening, tests, and documentation make it useful for real embedded projects too.

---

## Success Metrics

**Week 1:**
- 50+ upvotes on r/cpp
- 20+ upvotes on r/programming
- 10+ GitHub stars
- 5+ thoughtful comments

**Month 1:**
- 100+ GitHub stars
- 5+ PRs or issues
- 2+ blog posts mentioning Tez
- 1+ educational citation

---

## Contingency Plans

**If downvoted:**
- Don't panic - engage with critics
- Ask how to improve
- Emphasize educational value

**If ignored:**
- Try different time/day
- Adjust messaging
- Focus on niche subreddits (r/embedded, r/learnprogramming)

**If negative feedback:**
- Acknowledge valid concerns
- Update documentation
- Don't be defensive
- Learn and iterate
