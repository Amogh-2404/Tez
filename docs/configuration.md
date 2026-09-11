# Configuration

This reference describes the development source. Run `Tez --version` and `Tez --help` when checking a binary from another checkout or container tag.

## Command line

```sh
./build/Tez --address 127.0.0.1 --port 8080 --threads 2 \
  --config config.json --static-dir static \
  --timeout 30 --max-connections 128 --body-limit 1048576
```

| Option | Default | Meaning |
| --- | --- | --- |
| `--address` | `127.0.0.1` | IPv4 or IPv6 address to bind; use `0.0.0.0` deliberately for all IPv4 interfaces; hostnames are not accepted |
| `--port` | `8080` | TCP port; `0` requests an available port from the OS |
| `--threads` | Hardware concurrency capped at 8; fallback 1 | Number of I/O workers, 1–256 |
| `--config` | Discovery, described below | JSON file containing fixed response routes |
| `--static-dir` | Discovery, described below | Root of the public static file tree |
| `--timeout` | `30` | Seconds allowed for each header-read, body-read, or response-write phase, 1–3600 |
| `--max-connections` | `128` | Maximum admitted sessions, 1–65536 |
| `--body-limit` | `1048576` | Maximum decoded request body in bytes, 1–10485760 |
| `--check-config` | — | Validate configuration and exit without opening a listener |
| `--help`, `-h` | — | Print usage and exit |
| `--version` | — | Print version and exit |

Pass options and their values as separate arguments. A repeated valued option uses its last value; all supplied numeric values must still pass validation. `--help`, `-h`, and `--version` can appear alongside other valid options and exit before loading files. Unknown options, missing values, and invalid numeric values produce an error and exit status `1`; numeric errors include the accepted range. Prefix flag-like relative filenames with `./` so they are treated as values.

An IPv6 address may include a `%zone` suffix, using an existing interface name or a decimal scope ID from `0` through `4294967295`. Empty, malformed, and unknown named zones are rejected. Startup displays IPv6 addresses in brackets and uses a numeric scope ID when present. A successful configuration check does not establish that an address or numeric scope can be bound on the current machine.

Explicit relative paths are resolved from the working directory. With no path flags, discovery checks `config.json` and `static` there, then falls back to `../config.json` and `../static` for the older build-directory invocation. Missing implicit config permits built-in routes; missing implicit static content produces file misses. An invalid explicit path fails startup. Prefer explicit paths in services and containers. Command-line settings are not read from the route JSON, and no environment-variable configuration layer is provided.

Startup reports the selected absolute paths, the number of configured routes, and the `/static/` URL prefix. The route count excludes built-in endpoints. If discovery finds no config or static directory, the summary explicitly reports built-in routes only or disabled static files. Route edits require a restart.

## Check before running

Validate your files without starting the server:

```sh
./build/Tez --check-config --config config.json --static-dir static
```

Success prints `Configuration valid.`, followed by the same path and route-count summary as startup, and exits with status `0`. Failure prints the diagnostic to stderr and exits with status `1`. Configuration errors include the file path; route validation errors identify the route and offending field where applicable. For example, a numeric `body` value produces `route "/hello": body must be a string`. Paths and keys are escaped in diagnostics.

The check validates command-line values and the route file, and opens the static root directory. It does not bind a port, check port availability, inspect every static file, or issue HTTP requests. The same discovery rules apply: missing implicit files are permitted, while missing explicitly selected paths fail. Use explicit paths when a preflight check must require particular files.

## Routes

The configuration is a JSON object keyed by request path. Each route has three string fields:

```json
{
  "/": {
    "status": "200 OK",
    "content_type": "text/html; charset=utf-8",
    "body": "<h1>Tez</h1>"
  },
  "/ready": {
    "status": "200 OK",
    "content_type": "application/json",
    "body": "{\"ready\":true}\n"
  }
}
```

The configuration source must be a regular file no larger than 1 MiB. The parser validates it at startup before accepting requests. It rejects duplicate keys, excess nesting, unknown fields, invalid route paths, and invalid response fields. Invalid configuration is a startup error. Responses are held in an immutable map; editing the JSON does not change a running process. Restart to load new routes.

Configured routes support `GET` and `HEAD`. Query strings do not create distinct routes: `/ready?probe=1` selects `/ready`. Configured path matching is case-sensitive and retains percent escapes; unlike static filenames, route keys are not percent-decoded. The JSON `body` is returned as literal text; there is no templating, script execution, or automatic JSON interpretation. A fixed `application/json` body must itself contain valid JSON if clients expect to parse it.

Built-in paths are reserved: configuration cannot redefine `/health`, `/echo`, `/api/data`, or `/static/*`. Status values must be printable HTTP status strings from 200 through 599; `204`, `205`, and `304` require an empty body. Header values cannot contain injected control bytes.

Available resources:

| Path | Methods | Result |
| --- | --- | --- |
| `/health` | `GET`, `HEAD` | `200`, with `{"status":"ok"}` for GET |
| `/echo` | `POST`, `PUT` | JSON containing `method`, `received_body`, and `body_length` |
| `/api/data` | `GET`, `HEAD`, `POST`, `PUT`, `DELETE` | Fixed demonstration responses; no stored state |
| `/static/<path>` | `GET`, `HEAD` | File from the configured static root |
| Configured path | `GET`, `HEAD` | Configured status, content type, and body |

An unsupported method on a known route returns `405` with `Allow`. Missing non-static routes return `404`. The `/static/` namespace accepts only GET/HEAD and returns `405` for other methods before looking up a file. `HEAD` selects the GET representation and suppresses the response body on the wire; its `Content-Length` describes the GET body. The server may still read a file to produce that representation. Echo/API JSON replaces invalid UTF-8 sequences with U+FFFD; `body_length` reports the original input byte count.

## Static files

Create files under the static root and request them through `/static/`. Nested directories are supported. MIME types are selected from the filename extension; unknown extensions use `application/octet-stream`.

The path is decoded once. Traversal components, dotfiles, empty segments, malformed escapes, encoded separators, control bytes, colons, and backslashes are rejected. Encoded spaces are supported; a tilde or `..` within an otherwise valid filename is not itself traversal. Symbolic links below the root are not served, even if they point to another file in that root. Directories are not listed and special files such as FIFOs are not served.

The process opens files relative to a directory descriptor and checks the opened file before reading it. That root descriptor is pinned for the process lifetime; replacing the root directory itself requires a restart. This avoids a separate canonicalize-then-open path check. The static tree is still an administrative trust boundary: do not let untrusted users create files, hard links, or mounts there.

The cache uses a file identity and metadata check on each request. Changing or replacing a file invalidates its old cached representation when the metadata differs. TTL does not deliberately serve an old representation for 60 seconds without checking the filesystem. A detected modification during a read returns `503` so the client can retry. Use atomic file replacement when deploying content; modifying a file in place during a read cannot provide a transactional snapshot.

Static content is buffered in memory. Range responses, conditional requests (`ETag` / `If-Modified-Since`), compression, and directory index selection are not implemented.

## Limits

MiB and KiB below use powers of 1024.

| Resource | Default / fixed cap | Behavior |
| --- | --- | --- |
| Serialized request header | 8 KiB | Parser rejects an oversized header with `431` and closes |
| Decoded request body | 1 MiB by default, configurable up to 10 MiB | Oversized body receives `413` and closes |
| Admitted connections | 128 by default | Excess connections are closed without creating another session |
| Socket phase deadline | 30 seconds by default | Incomplete operation closes the connection |
| Requests per connection | 1,000 | Final response advertises `Connection: close` |
| Route config source | 1 MiB | Larger configuration fails startup |
| Static file | 16 MiB | Larger file receives `413` |
| File cache entries | 50 | Least-recently-used eviction |
| File cache logical bytes | 32 MiB | Eviction or skipped insertion when the budget would be exceeded |
| File cache TTL | 60 seconds | Expired entries miss; identity/metadata checks apply before a hit |

These limits are independent. They do not cap total process memory: active request bodies, response copies, JSON escaping, parser buffers, worker stacks, cache bookkeeping, and socket buffers add to the working set. For example, 128 admitted sessions each retaining a 1 MiB request body already account for 128 MiB of payload alone; this is arithmetic, not measured RSS. Set container or service limits and lower concurrency/body limits for constrained environments.

The timeout is a deadline for an asynchronous phase, not a CPU budget or guaranteed whole-request time. Header and body phases have separate deadlines. Synchronous route work, file operations, and stderr writes must return before the I/O worker can run more handlers.

## HTTP compatibility

Tez uses Boost.Beast for HTTP/1.x parsing and serialization, with additional request validation. It retains unused TCP bytes for the next parse and responds to pipelined requests in arrival order. Bodies may use a single `Content-Length` or plain chunked transfer encoding. Duplicate or comma-separated Content-Length values and combinations of Transfer-Encoding with Content-Length are rejected. Other transfer codings receive `501`. Ambiguous framing is rejected; parser errors close the connection instead of attempting to guess the next request boundary.

HTTP/1.1 requires a valid `Host`. HTTP/1.0 connections close by default unless persistence is requested. `Connection: close` takes precedence over keep-alive. Valid `Expect: 100-continue` is handled before reading the body; unsupported expectations receive `417`. Trailers that attempt to add framing, routing, or authentication semantics are rejected. Permitted extension trailer metadata is parsed and ignored; it is not merged into application request headers.

Origin-form paths and absolute-form `http://` / `https://` request targets are accepted. The latter syntax does not enable TLS: the connection is still plaintext. Path and query characters follow [RFC 3986](https://www.rfc-editor.org/rfc/rfc3986.html#section-3.3); clients must percent-encode disallowed punctuation such as angle brackets, braces, and square brackets in query keys (`items%5B%5D=1`). Brackets enclosing an IPv6 authority are permitted. Unsupported HTTP versions receive `505`, malformed syntax receives `400`, and CONNECT receives `501`. An expired socket phase closes the connection without a `408` response; admission rejection closes TCP without a `503` response.

This is an origin server with a deliberately small feature set, not a forward proxy or a full HTTP conformance certification. CONNECT tunnels, protocol upgrades, HTTP/2 and HTTP/3 are unavailable. Authentication and forwarded-client-IP trust policies are not implemented. Request logs identify the socket peer, which is the proxy when one is used.
