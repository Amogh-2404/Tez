# Tez engineering review

Tez's strongest direction is a small, inspectable HTTP server with precise behavior and defensible evidence. The work toward 1.1.0 prioritizes message framing, bounded resource use, filesystem access, repeatable tests, and honest distribution metadata. A larger feature list or an unqualified throughput claim would not establish reliability.

The baseline for this review is commit `9ee06b8`, before the 1.1.0 development changes. Findings about that baseline come from the repository itself; external sources establish protocol requirements and design constraints. This review does not contain a new benchmark. Performance hypotheses, observed code properties, and future validation are identified separately.

## 1. Baseline findings

The earlier server accepted connections asynchronously, then passed each connection to a worker that performed synchronous reads and writes. A keep-alive client could retain a worker while waiting for another request. The worker queue had no admission bound. A `Keep-Alive: timeout=5` header described a timeout that the socket operations did not enforce. Calling worker shutdown from the signal handler could wait for a worker still blocked on its connection.

The HTTP path read until the header delimiter into a stream buffer, converted all buffered bytes to a request, and then read `Content-Length` bytes again from the socket. If the first read included body bytes, that second read could wait for bytes already consumed or read part of the next request. Parsing headers into a map also overwrote duplicate fields before framing validation. `std::stol` accepted a numeric prefix without requiring the entire `Content-Length` value to be valid.

The static file path canonicalized a pathname and opened it later. A check and a later open do not identify one indivisible filesystem operation. The cache was consulted before path validation, and its capacity counted entries without bounding body bytes. A single file could be read into memory without a file-size cap. These observations justify stronger access and allocation rules; they do not by themselves demonstrate a particular remote exploit in a deployed system.

Configuration and static paths were tied to `../config.json` and `../static`. The image placed files in `/app` and started the binary there, so the process and packaging disagreed about where content lived. The image's health check used a HEAD-style probe while `/health` accepted only GET. Logging opened `server.log` for each request, despite a Compose layout that exposed a different log directory and made the root filesystem read-only.

Several tests could pass without exercising the intended behavior. The static-file test accepted either success or absence, and MIME assertions ran only when a file happened to exist. The router fixture wrote a test configuration after initialization without arranging for the router to use it. The CI startup check did not verify a usable response and could ignore process failure. Improving these tests is a prerequisite for trusting more ambitious changes.

The project presentation amplified the problem: the README simultaneously called Tez production-ready and educational, advertised response times with no corresponding raw captures, and described all I/O as asynchronous. The security policy assigned unsupported timelines and misclassified eviction policy as protection from cache poisoning. Accurate documentation is part of the engineering boundary because it controls the conditions under which other people run the software.

## 2. HTTP framing and interoperability

RFC 9112 specifies HTTP/1.1 framing independently of application routes. It requires valid Host handling and defines precedence for transfer encoding and content length. A request with ambiguous length information needs a consistent error policy; parsing differently from an intermediary can create request-smuggling risk. Persistent connections require correct consumption of one message before interpreting the next.[^1]

**Decision:** delegate wire parsing and serialization to Boost.Beast. Keep Tez-specific validation narrow and test its interaction with the library. Maintain a connection-owned input buffer across requests. Read the header first, reject invalid framing or excess declared body length, handle expectations, then finish the body. Beast explicitly supports staged parser reads and retaining unconsumed octets in a dynamic buffer.[^2]

This changes the unit of reasoning. Instead of proving a collection of string splits is a complete HTTP parser, the project must prove that it configures and uses a maintained parser correctly. That still leaves meaningful work: allowed target forms, duplicate Host fields, unsupported transfer codings, request-count accounting, parser errors, response lifetime, and application dispatch remain Tez's responsibility. Dependency choice reduces duplicated protocol machinery; it does not eliminate validation.

A decisive regression sends headers and body in the same TCP write. Another sends the body in several fragments. A third sends a complete request followed immediately by another on the same connection. All three must produce the same application requests and exact body bytes. Pipeline tests must check both response order and absence of extra bytes, because a response that looks correct in isolation can still corrupt the next message boundary.

HEAD response content must be suppressed while metadata describes the GET representation. Method rejection for a known resource requires an appropriate Allow field. `Expect: 100-continue` allows a client to wait for an interim response before sending a body, so ignoring it can stall both peers. These are observable semantics, not optional presentation details.[^3]

**Acceptance criteria:** tests cover fixed-length and chunked bodies, fragmented and coalesced input, invalid lengths, duplicate/absent Host, unsupported expectations, HTTP/1.0 persistence, HEAD, request-count closure, and truncated messages. Never recover from uncertain framing by scanning for a string that resembles the next request line.

**Remaining scope:** using an HTTP/1 parser is not a certificate of full HTTP conformance. Unsupported target forms, upgrades, transfer codings, ranges, and conditional semantics must be stated explicitly. New protocol features should arrive with byte-level tests and a clear intermediary compatibility story.

## 3. Concurrency, ownership, and deadlines

**Decision:** use asynchronous socket sessions sharing an `io_context`, with a strand per session. A worker handles callbacks from multiple connections; a connection does not retain a worker while waiting on network input. Asio defines a strand as sequential handler invocation and explains why composed operations must preserve the same associated executor.[^4]

The ownership rules matter more than the thread count. The session, parser, retained input buffer, and outgoing response must outlive every operation referring to them. A response cannot be a temporary local object whose memory disappears before the asynchronous write completes. Cancellation and normal completion must release admission accounting exactly once. An error path is part of the lifetime model, not an exception to it.

Only one response is written at a time per connection. This naturally preserves pipeline order and avoids introducing a separate unbounded queue of generated responses. Serializing one connection still permits independent sessions to make progress on other workers. This is a bounded complexity choice; it does not establish that the chosen scheduling policy is fastest for every request mix.

Beast's TCP stream provides deadlines for logical asynchronous operations. Its documentation distinguishes those operations from synchronous stream I/O and notes that expiry applies across the calls composing an operation until reset.[^5] Tez therefore needs an explicit phase policy: header read, body read, and write each receive a deadline. A timeout value in a response header alone cannot enforce anything.

**Tradeoff:** routing, file access, JSON serialization, and log writes remain synchronous handler work. A stalled filesystem or blocked stderr sink can delay an I/O worker and timer processing. The design fixes network-wait occupation; it does not make every operation nonblocking. Adding an independent bounded blocking-work executor may become appropriate, but it adds cancellation, queue, ownership, and shutdown states that need their own tests.

**Shutdown contract:** signals terminate network processing and release sessions; requests are not promised a drain window. True graceful draining would need to stop admission, prevent another keep-alive cycle, await current responses for a bounded period, and then cancel stragglers. That should be implemented and tested as a distinct feature rather than implied by the presence of a signal handler.

## 4. Resource budgets and overload

A body limit, a cache limit, and a connection limit regulate different allocations. None alone is a process-memory limit. Beast exposes separate header and body limits, and they must be configured before the relevant parsing begins.[^6][^7]

**Decision:** expose admission and body limits as runtime settings, use conservative defaults, and keep explicit fixed limits for headers, configuration source, files, and cache bytes. Close an excess connection without allocating another full application session. Close on framing/size errors to avoid carrying ambiguous input into persistent requests.

The default payload arithmetic is easy to inspect: 128 admitted sessions retaining 1 MiB each represent 128 MiB before response bodies, parser storage, allocator overhead, stacks, file cache, and socket buffers. Echo JSON can expand the representation through escaping. Static responses can be much larger than request bodies, and copying a cached body can multiply live memory by the number of active writers. This explains why a 32 MiB cache does not mean a 32 MiB server.

A configured maximum should be treated as a ceiling, not as a recommended operating point. Increasing workers, connections, or body size without a workload and memory budget can worsen contention or memory pressure. A container-level memory limit is the final enforcement mechanism, but reaching it can terminate the process. Application limits should keep normal operation below that boundary with room for bursts and overhead.

**Acceptance criteria:** exercise a limit with small, deterministic fixtures; verify the intended rejection; release the offending connection; then confirm an ordinary request still succeeds. The recovery check catches counters or ownership paths that leak capacity after failure. Capacity behavior should be reproducible with a deliberately tiny configured cap, so validation does not require a stress benchmark.

## 5. Filesystem confinement and freshness

`openat` resolves a relative path against an already-open directory. `O_NOFOLLOW` rejects a symlink in the final component of an individual open, so checking each component requires walking them separately. `O_NONBLOCK` does not turn regular-file reads into asynchronous disk I/O.[^8]

**Decision:** open the static root as a trusted directory descriptor, decode request paths once, reject traversal/separator ambiguity, and walk components using descriptor-relative opens. Check the final descriptor for a regular file and acceptable size before reading. Keep the root and content deployment under trusted administrative control. Do not advertise this as isolation from an attacker who can create hard links, mounts, or arbitrary filesystem mutations within the root.

This addresses a different question from string normalization. A normalized path says what a string means at one instant; an opened descriptor identifies the object actually being used. File identity, size, and timestamps used for the cache should come from that descriptor, not from a fresh lookup of a pathname that may now resolve elsewhere.

Linux offers additional resolution controls through `openat2`, including policies for staying beneath a directory and disallowing symlinks or mount crossings.[^9] **Alternative considered:** use that interface exclusively. **Decision:** retain a component walk for the current Linux/macOS scope and document its boundaries. A Linux-specific backend can be evaluated later if it supplies a needed guarantee that is tested and clearly differentiated from the portable path.

The cache checks file metadata before a hit and checks again after reading on a miss. This improves ordinary file-replacement freshness and avoids caching a body when a detectable mutation occurred during its read. It is not content hashing or an atomic snapshot of an actively modified file. Deploy files by atomic replacement and avoid untrusted writers. TTL is a secondary age bound, not permission to skip filesystem validation for a minute.

nginx's documentation treats symlink restrictions, file metadata caching, and asynchronous file I/O as separate controls. Its file-I/O discussion also includes platform and alignment constraints.[^10] The useful lesson is separation of concerns: path policy, freshness, memory use, and disk scheduling cannot all be solved by naming a component a cache.

## 6. Cache and route design

**Decision:** configured responses are an immutable startup snapshot. They are already resident in memory, so a second response LRU adds locking, recency bookkeeping, and duplicate state without avoiding disk or JSON parsing. Removing that route-path cache simplifies the proof of behavior. Any performance benefit is unmeasured.

File caching has a different purpose: a valid hit avoids reading the body again. Its policy therefore needs an explicit key, identity check, insertion timestamp, entry bound, byte bound, and eviction order. A cache entry must not be returned before validating access to the requested file. An invalid or deleted target must not remain accessible merely because an older response was cached.

Byte accounting should describe what it counts. Logical response/key bytes exclude allocator capacity, hash-table buckets, list nodes, metadata objects, and live copies held outside the cache. Naming a cap as a logical-byte budget is more accurate than implying an RSS guarantee. Expiry tests should use controllable time, while eviction tests should distinguish entry pressure from byte pressure and test updates as well as inserts.

**Open optimization questions:** immutable shared bodies could reduce response copies, streaming could reduce peak allocation, and a bounded blocking-work executor could isolate disk stalls. All three alter ownership and failure behavior. They should be evaluated against representative payloads after correctness and memory accounting are stable, rather than included simply because they sound faster.

## 7. Packaging, logging, and supply chain

A container is another deployment of the same runtime contract. The executable path, working directory, config root, document root, UID, port, health probe, and log destination must agree. CLI path options eliminate the earlier assumption that the process always starts from a `build/` directory.

**Decision:** use a multistage image, an explicit non-root identity, read-only content, stderr logs, and a health check that exercises a supported method. Docker documents multistage separation, minimal runtime dependencies, immutable image digests, and the tradeoff that pinned bases need deliberate updates to receive fixes.[^11] A smaller image is useful, but size alone says nothing about runtime protocol correctness.

Request logs omit query strings and escape control bytes so a request target cannot inject a new logical log line. The peer address comes from the socket. Behind a proxy that identifies the proxy, not necessarily the original client. Trusting forwarded headers requires a separately defined trusted-proxy policy. Caddy documents its handling of forwarded headers and trusted proxies; copying a header into a log without that trust boundary would be misleading.[^12]

Build verification and publication should have different privileges. Pull requests need enough access to compile and test, not registry credentials. Release publication should use the tested revision, verified artifacts, correct architecture labels, and explicit promotion policy. GitHub recommends full-length action commit pins and warns against executing untrusted code in privileged workflow contexts.[^13]

**Acceptance criteria:** required test dependencies cannot silently disappear; a startup check validates HTTP status/body; image smoke tests run the image's actual command under its actual UID; read-only operation works; custom config/static mounts are exercised; and shutdown leaves no test child process running. CI should not manufacture performance rankings from shared-runner timing.

## 8. Evidence and performance claims

The four archived ApacheBench captures each contain 100 requests over roughly 9–11 ms. They lack a recorded source revision, exact command, build flags, and complete machine configuration. Response sizes differ. They cannot support a comparative claim about the new architecture, nor a ranking against established servers.

ApacheBench distinguishes two time-per-request calculations and warns that the tool itself can be the bottleneck.[^14] This is especially relevant when interpreting very short runs. The project retains raw historical files but removes endpoint/percentile claims for which those files provide no evidence.

**Decision:** separate three kinds of statement. A code property is inspectable: an immutable config is loaded once. A hypothesis names an expected effect: removing redundant work may lower CPU cost for configured routes. A measurement quantifies an observed effect on a named workload and machine. Only the third supports a numerical speedup.

A later evaluation should hold body bytes, request mix, connection policy, logging, resource limits, and build settings constant. It needs repetitions, error counts, achieved throughput, tail latency, CPU, RSS, and raw artifacts. The arrival model matters: wrk2 explains how closed-loop waiting can omit latency during stalls and how intended request times alter its measurement.[^15] The project's [performance plan](performance.md) defines a workload matrix and publication requirements without running a benchmark locally.

## 9. Project presentation and release integrity

The repository landing page should answer four questions quickly: what the server does, how to run this version, where its boundaries are, and how its claims can be checked. Architecture art should reflect the actual code path. A diagram showing an obsolete worker pool can mislead a contributor more effectively than a stale paragraph because it is easier to remember.

**Decision:** use a restrained visual identity, an accessible SVG banner, a raster social preview, a compact capability table, and a single source of documentation for runtime settings. Preserve standard community policy text and the existing maintainer identity. Avoid badges for nonexistent guarantees, fabricated audit status, release dates inferred from old prose, and inflated adoption claims.

GitHub accepts a separate repository social preview image and documents a 1280×640 recommendation.[^16] Docker Hub supports a Markdown repository overview with a preview step.[^17] The maintained assets and overview are part of the source tree so they can be reviewed and updated with behavior changes. A page update must not imply that historical registry tags suddenly contain the development version.

## 10. Next decisions

| Priority | Question | Evidence required before claiming completion |
| --- | --- | --- |
| Immediate | Does the new parser/session path preserve bytes, ordering, and failure boundaries? | Component and socket regressions on Linux/macOS; sanitizer results where supported |
| Immediate | Do paths, cache checks, and runtime packaging agree? | Temporary-root fixtures, symlink cases, replacement/deletion cases, container smoke checks |
| Before release | Are the source revision, image contents, architecture, and documentation consistent? | Tested artifact/digest traceability and release review |
| Before performance claims | What workloads improve or regress, and by how much? | Dedicated reproducible experiments with errors, latency, throughput, CPU, and RSS |
| Future | Should bodies be shared/streamed or blocking work offloaded? | Allocation profiles, workload evidence, and cancellation/backpressure design |
| Future | Is graceful draining required? | Explicit shutdown semantics and bounded drain/cancellation tests |
| Future | Which protocol features are worth the added surface? | A concrete consumer need, protocol specification, and regression coverage |

A credible release is a checked set of behaviors and artifacts, not an assertion that all possible work is finished. Tez can become a stronger systems project by making each guarantee smaller, testable, and easy to inspect.

## Sources

External documentation was reviewed on 2026-09-11. RFCs are cited by published revision; library and vendor documentation may evolve. Where behavior depends on the installed Boost version, the supported-version build and socket tests remain the implementation check.

[^1]: IETF. [RFC 9112: HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112.html), June 2022, §§3.2, 6.3, 9.3, 11.2. Host, framing, persistence, and request smuggling.
[^2]: Boost.Beast. [Parser Stream Operations](https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/using_http/parser_stream_operations.html). Staged reads and retained input buffers.
[^3]: IETF. [RFC 9110: HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html), June 2022, §§8.6, 9.3.2, 10.1.1, 15.5.6. HEAD, content length, expectations, and method rejection.
[^4]: Boost.Asio. [Strands: Use Threads Without Explicit Locking](https://www.boost.org/doc/libs/latest/doc/html/boost_asio/overview/core/strands.html). Sequential handlers and associated executors.
[^5]: Boost.Beast. [Timeouts](https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/using_io/timeouts.html). Logical operation deadlines and asynchronous stream behavior.
[^6]: Boost.Beast. [basic_parser::header_limit](https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/ref/boost__beast__http__basic_parser/header_limit.html). Serialized header limits and configuration timing.
[^7]: Boost.Beast. [basic_parser::body_limit](https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/ref/boost__beast__http__basic_parser/body_limit.html). Payload limits and chunked decoding.
[^8]: Linux man-pages project. [open(2)](https://man7.org/linux/man-pages/man2/open.2.html). Descriptor-relative opens, O_NOFOLLOW, and O_NONBLOCK limitations.
[^9]: Linux man-pages project. [openat2(2)](https://man7.org/linux/man-pages/man2/openat2.2.html). Linux-specific path-resolution policies.
[^10]: nginx. [ngx_http_core_module](https://nginx.org/en/docs/http/ngx_http_core_module.html), `aio`, `disable_symlinks`, and `open_file_cache`. File access, caching, and platform-specific I/O controls.
[^11]: Docker. [Building best practices](https://docs.docker.com/build/building/best-practices/). Runtime stages, dependencies, base-image pins, and rebuilds.
[^12]: Caddy. [reverse_proxy directive](https://caddyserver.com/docs/caddyfile/directives/reverse_proxy), headers and trusted proxies. Forwarded identity and proxy trust.
[^13]: GitHub. [Secure use reference](https://docs.github.com/en/actions/reference/security/secure-use). Action immutability and privileged workflow boundaries.
[^14]: Apache Software Foundation. [ab — Apache HTTP server benchmarking tool](https://httpd.apache.org/docs/2.4/programs/ab.html). Output calculations and measurement caveats.
[^15]: Gil Tene. [wrk2 methodology](https://github.com/giltene/wrk2#readme). Intended-arrival timing and coordinated omission.
[^16]: GitHub. [Customizing your repository's social media preview](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/customizing-your-repositorys-social-media-preview). Accepted image formats and dimensions.
[^17]: Docker. [Repository information](https://docs.docker.com/docker-hub/repos/manage/information/). Docker Hub Markdown overview and preview.
