# Hacker News Submission

## Show HN Guidelines Compliance

✅ **Make something interesting** - Educational HTTP server with production docs
✅ **New and substantive** - Fresh approach to learning web servers
✅ **Not a hosted service** - Open source project, self-hosted
✅ **Not a sign-up page** - Direct GitHub link
✅ **Interesting to hackers** - Technical, educational value

## Submission Details

**Type:** Show HN
**Timing:** Tuesday or Wednesday, 9-11am EST (US morning, Europe afternoon)
**URL:** https://github.com/Amogh-2404/tez

### Title Options (Choose One)

**Option 1 (Educational Focus):**
```
Show HN: Tez – Educational HTTP server in C++ with production-quality docs
```

**Option 2 (Technical Focus):**
```
Show HN: Tez – Modern C++ HTTP server demonstrating async I/O patterns (2K LOC)
```

**Option 3 (Problem/Solution):**
```
Show HN: Bridging the gap between toy HTTP examples and production servers (C++)
```

**Recommended:** Option 1 - Clear value proposition, educational angle resonates on HN

---

## First Comment (Post Immediately After Submission)

Post this as the first comment to provide context:

```markdown
Author here. I built Tez to fill a gap I noticed in the C++ ecosystem: there aren't many well-documented, intermediate-complexity examples of async web servers.

**The Problem:**
Most resources are either:
- Tutorial examples: Too simple, no real-world concerns (security, performance, error handling)
- Production frameworks: Too complex, hard to learn from (Drogon has 100k+ LOC)

**What Tez Offers:**
- Clean 2,000 LOC demonstrating production patterns
- Security-first (path traversal protection, request limits, LRU caching)
- Comprehensive documentation (every architectural decision explained)
- Modern C++17 (filesystem, RAII, smart pointers, thread-safe designs)

**Key Design Decisions:**

1. **Thread Pool Architecture**: Fixed-size pool that scales to hardware cores. Each connection gets enqueued to a worker thread that handles the full request/response cycle including keep-alive. This is simpler than epoll-style event loops but still performs well (~10k req/sec).

2. **LRU Caching**: Proper least-recently-used eviction instead of naive clear-all. Prevents cache poisoning attacks while maintaining O(1) operations using list + unordered_map.

3. **Security Limits**: All configurable as constants at the top of main.cpp:
   - MAX_CONTENT_LENGTH = 10 MB (prevents memory exhaustion)
   - MAX_HEADER_SIZE = 8 KB (prevents header flooding)
   - MAX_KEEPALIVE_REQUESTS = 1000 (prevents infinite abuse)

4. **Config Loading**: Loads once at startup, not per-request. The initial version loaded config.json on every request (100x slower) - now it's cached in memory with mutex protection.

**What It's NOT:**
- Not for production web services (no TLS, no HTTP/2, slower than nginx)
- Not trying to replace Drogon or cpp-httplib
- Not the fastest or most feature-rich

**What It IS:**
- A learning resource with production-quality patterns
- Good for embedded/IoT projects (internal networks)
- Useful for portfolio demonstrations
- A starting point you can actually understand and modify

**Some Interesting Implementation Details:**

The path traversal protection uses `std::filesystem::weakly_canonical` to resolve .. and . components, then validates the canonical path still starts with our static directory. This prevents attacks like `/static/../../etc/passwd`.

The MIME type detection switched from linear search (50+ if statements) to hash map lookup after I realized it was a bottleneck in benchmarks.

Happy to answer questions about any architectural decisions or trade-offs!

GitHub: https://github.com/Amogh-2404/tez
Docs: https://github.com/Amogh-2404/tez#documentation
```

---

## Response Strategy

### Immediate (0-2 hours)
- Monitor comments every 15 minutes
- Reply to every question
- Upvote constructive criticism
- Thank people for feedback

### Common HN Questions (Pre-prepared Answers)

**Q: "Why not just use nginx/cpp-httplib/Drogon?"**
```
Excellent question. Those are all great for production. Tez is optimized for learning - every component is documented with explanations of *why* decisions were made. Think of it as a tutorial that happens to be functional code.

For production: Use nginx, Caddy, Drogon, or cpp-httplib.
For learning: Tez's 2K LOC is readable in an afternoon.
For embedded: Either works, Tez has good docs for deployment.
```

**Q: "No TLS? This is insecure!"**
```
You're absolutely right. Tez is for:
1. Learning (TLS adds complexity that obscures core concepts)
2. Internal/embedded networks (behind firewall)
3. With reverse proxy (nginx handles TLS)

The README has a prominent disclaimer about this. TLS is planned for v1.1 (Phase 2) but currently prioritizing educational clarity over production features.
```

**Q: "Only 10k req/sec?"**
```
Correct. For context:
- Drogon: 100k+ req/sec
- cpp-httplib: 50k+ req/sec
- Tez: 10k req/sec

The trade-off: Tez prioritizes code clarity over performance. Thread pool + sync file I/O is slower than epoll + async everything, but much easier to understand.

For most learning/embedded use cases, 10k req/sec is plenty. For production, use the faster options.
```

**Q: "Boost is a heavy dependency"**
```
Fair criticism. I chose Boost.Asio because:
1. Industry-standard for async I/O in C++
2. Well-documented (easier for learners)
3. Handles cross-platform edge cases

Alternative: A future version could offer no-dependency option using raw epoll/kqueue, but that's less portable and harder to learn from.

cpp-httplib (zero dependencies) is a great alternative if you don't want Boost.
```

**Q: "The code could be more idiomatic C++"**
```
I'd love specific suggestions! Please open an issue or PR. The goal is to demonstrate good modern C++ patterns while remaining readable for intermediate developers.

What specific areas could be improved?
```

**Q: "This seems like reinventing the wheel"**
```
For production use: Absolutely, wheel already exists.

For learning: Sometimes building the wheel teaches you more than using it. How many people learn React by building a framework? It's about the journey.

Also useful for:
- Embedded projects needing minimal dependencies
- Understanding how existing wheels work
- Portfolio/interview demonstrations
```

---

## Engagement Tactics

### DO:
✅ Reply to every comment within 2 hours
✅ Be humble about limitations
✅ Share technical details when asked
✅ Thank people for suggestions
✅ Link to specific code when discussing features
✅ Acknowledge valid criticisms
✅ Offer to help people try it

### DON'T:
❌ Get defensive about criticism
❌ Over-promote or be salesy
❌ Claim it's production-ready
❌ Compare negatively to other projects
❌ Ignore "negative" comments
❌ Spam links in replies

---

## Technical Deep Dives (If Asked)

### Thread Pool Implementation
```
The thread pool uses a condition variable + task queue pattern:

1. Fixed-size pool created at startup (hardware_concurrency threads)
2. Tasks enqueued with std::function<void()> wrapper
3. Workers block on condition variable until task available
4. Each worker handles full request/response cycle
5. Graceful shutdown: poison pill pattern + thread joining

This is simpler than event loop patterns (epoll) but still handles ~10k concurrent connections effectively.

Code: src/thread_pool.cpp
```

### LRU Cache Design
```
O(1) get/put using list + unordered_map:

- List maintains access order (most recent at front)
- Map stores {key -> {value, list_iterator}}
- Get: Move accessed item to front, return value
- Put: If full, evict back of list (LRU), add new item to front

This prevents the naive approach of cache.clear() which allows cache poisoning attacks.

Code: middleware.cpp:10-82
```

### Security Hardening
```
Three layers:

1. Path Sanitization (file_server.cpp:57-93):
   - Blocks ../, ~, absolute paths
   - Uses filesystem::weakly_canonical for validation
   - Ensures resolved path stays in static directory

2. Request Limits (main.cpp:20-24):
   - Validates before allocating memory
   - Returns 413/431 for oversized requests
   - Prevents DoS via large payloads

3. Thread Safety (all components):
   - Mutex guards on all shared state
   - LRU cache, config, logging all protected
   - Safe for concurrent access

Full details: SECURITY.md
```

---

## Success Metrics

### Immediate (Day 1):
- 50+ points
- 30+ comments
- Front page (top 30)
- 20+ GitHub stars

### Week 1:
- 100+ points
- Stays on front page for 12+ hours
- 50+ GitHub stars
- 5+ thoughtful discussions

### Month 1:
- 200+ total points
- 10+ blog posts mentioning it
- 100+ GitHub stars
- Educational citations

---

## Contingency Plans

### If Submission Flops (<20 points):
- Wait 7 days
- Resubmit with different title/timing
- Try "Ask HN: Feedback on my C++ HTTP server"
- Focus on technical subreddits instead

### If Heavily Criticized:
- Don't panic - HN loves critical discussion
- Engage constructively with every point
- Update docs based on feedback
- Consider it valuable user research

### If Flagged/Killed:
- Don't resubmit immediately
- Email mods if seems unfair
- Focus on Reddit instead
- Review HN guidelines

---

## Follow-Up Actions

### Same Day:
- Thank top commenters
- Answer all questions
- Fix any critical bugs mentioned
- Update docs based on feedback

### Next Day:
- Post summary of feedback received
- Announce any quick fixes made
- Thank community in GitHub README

### Week Later:
- Write blog post about launch experience
- Share technical deep dives requested
- Highlight interesting discussions

---

## Example Follow-Up Comments

**If someone asks for feature:**
```
Great suggestion! I've added this to the Phase 2 roadmap: https://github.com/Amogh-2404/tez/issues/new

Would you be interested in contributing to this feature? Happy to provide guidance.
```

**If someone reports bug:**
```
Thank you for catching this! Can you open an issue with reproduction steps?
https://github.com/Amogh-2404/tez/issues/new

I'll prioritize fixing this today.
```

**If someone shares their usage:**
```
This is awesome! Would you be willing to share this as an example in the tez-examples repo?

Love seeing real-world uses!
```

---

## Timing Strategy

**Best Days:** Tuesday, Wednesday (avoid Monday/Friday)
**Best Time:** 9-11am EST (US morning peak)
**Avoid:** Weekends, holidays, major tech news days

**Check before posting:**
- No major outages (AWS, GitHub, etc.)
- No big tech news dominating HN
- No similar projects on front page

---

## Post-HN Tasks

1. Add "Discussed on HN" badge to README if successful
2. Link HN discussion from GitHub
3. Update based on feedback
4. Thank community in next release notes
