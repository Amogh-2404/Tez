# Architecture

Tez is a standalone HTTP/1.x origin server. The network path uses Boost.Asio and Boost.Beast; routing and file handling return a small application `Response` that the session serializes to HTTP.

<img src="../diagrams/01-system-overview.svg" alt="Asynchronous listener and HTTP sessions dispatch to immutable configured routes, built-in handlers, and a confined static file reader with a validated file cache." width="100%">

## Session ownership

Each accepted connection has a session containing the TCP stream, HTTP parser, persistent input buffer, response object, request count, and connection accounting. Shared ownership keeps the session alive while asynchronous callbacks are outstanding. The response object remains alive until its write completes.

The input buffer belongs to the connection, not to one request. A read can receive headers, body bytes, and part of the next pipelined request together. Only bytes consumed by the parser are removed; remaining bytes are used by the next request parser.

<img src="../diagrams/02-request-flow.svg" alt="Read headers, validate framing, optionally send 100 Continue, read the body, route the request, write the response, then either read the next request or close." width="100%">

One response write finishes before the next request is dispatched on that connection. This maintains order without a separate unbounded response queue. A session's strand prevents its handlers from running concurrently across I/O workers.

## Execution model

The listener accepts asynchronously. A configured number of threads call `io_context::run()`. Different sessions may execute on different workers; a session is not assigned a permanent operating-system thread. Socket waits are asynchronous, so a quiet keep-alive client does not occupy a worker in a blocking read.

<img src="../diagrams/03-threading-model.svg" alt="Several I/O worker threads run one shared io_context. Each session has a serial strand. Synchronous file and log operations execute within handlers and can delay a worker." width="100%">

Route dispatch, file opens and reads, cache access, JSON serialization, and request logging are synchronous handler work. The file cache uses locking; logging serializes complete lines. The design does not claim asynchronous disk I/O, zero-copy responses, lock-free execution, or absence of contention.

The connection limit is admission control. It bounds active sessions, not the operating system's listen backlog or total clients trying to connect. Excess accepted connections are closed without dispatching application work.

## Routing and files

Configured routes are parsed and validated once at startup, then published as an immutable snapshot. Built-in routes provide a health response and method-dispatch examples. The configured response map is already in memory; looking it up does not require an additional LRU response-cache layer.

Static file requests use a separate path:

1. Strip the query component, decode the path once, and reject invalid or unsafe components.
2. Walk from the configured root directory using descriptor-relative opens that reject symlinks.
3. Check that the opened target is a regular file within the size limit.
4. Compare file identity and metadata with any cache entry.
5. On a miss, read a bounded body and recheck metadata before caching it.

<img src="../diagrams/05-security-defense.svg" alt="Static access crosses path decoding, component validation, descriptor-relative open with symlink rejection, regular-file and size checks, and file metadata revalidation." width="100%">

The cache maintains recency, insertion age, logical bytes, and entry count. TTL expiration and changed metadata cause a miss. The cache is an optimization after access validation, not a shortcut around the document-root boundary.

<img src="../diagrams/04-lru-cache.svg" alt="A metadata-validated file cache promotes hits to most recently used, inserts bounded misses, and evicts least recently used entries until both entry and byte limits fit." width="100%">

## Deadlines and shutdown

Each asynchronous header read, body read, and response write has a deadline. A completed operation does not grant the next phase an unlimited lifetime. The request-count limit also terminates long-lived keep-alive sessions with a correctly advertised final close.

SIGINT and SIGTERM stop network processing. Pending socket work is released during teardown and worker threads are joined. This avoids a worker pool stuck in synchronous socket reads, but it is not graceful response draining: an in-flight response may be interrupted. Synchronous filesystem or log operations are not preempted by the socket deadline.

<img src="../diagrams/06-tcp-lifecycle.svg" alt="TCP listen maps to an Asio acceptor, accepted connections become Beast sessions, I/O completion returns to the session strand, and completion, timeout, or shutdown releases the socket." width="100%">

## Source map

| File | Responsibility |
| --- | --- |
| [`src/main.cpp`](../src/main.cpp) | CLI, listener, session lifecycle, HTTP I/O, deadlines, worker startup and shutdown |
| [`src/request.cpp`](../src/request.cpp) | Request parsing and validation helpers |
| [`src/router.cpp`](../src/router.cpp) | Validated route configuration and method dispatch |
| [`src/file_server.cpp`](../src/file_server.cpp) | Confined static file access and MIME selection |
| [`src/middleware.cpp`](../src/middleware.cpp) | Cache support and request logging |
| [`include/response.hpp`](../include/response.hpp) | Application response representation |
| [`tests/`](../tests/) | Isolated component tests and socket integration coverage |

For design rationale, alternatives, and remaining gaps, see [engineering notes](engineering.md). For exact operational defaults, use [configuration](configuration.md).
