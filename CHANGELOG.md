# Changelog

Changes are recorded against the source tree. An unreleased entry does not imply that a GitHub release or Docker image has been published.

## Unreleased — 1.1.0-dev

### Developer experience

- Add `--check-config` to validate runtime settings, routes, and the static root without opening a listener.
- Report selected configuration and static paths with the configured route count; identify files, routes, and fields in configuration errors.
- Support `-h`, help and version alongside runtime options, and actionable unknown-option, missing-value, and numeric-range errors.
- Validate IPv6 zone suffixes explicitly and print canonical listener addresses in startup diagnostics.
- Retain container listener and content defaults when adding runtime flags; allow explicit overrides without repeating unrelated settings.
- Add an editable HTTP fixture example with fixed success/error responses, a same-origin static page, and a validate/run/edit/restart guide.

### HTTP and runtime

- Replace blocking per-connection workers and manual framing with Boost.Beast asynchronous HTTP sessions.
- Preserve buffered bytes across request bodies and persistent requests; support chunked request bodies and ordered pipelining.
- Add header, body, connection, and operation limits, strict framing checks, and command-line runtime settings.
- Add `HEAD` response handling and `Expect: 100-continue`; include accurate `Allow` headers for supported resources.
- Default the native listener to loopback. Handle shutdown without waiting on blocking socket reads.

### Files and routing

- Resolve explicit configuration and static paths independently of the build directory.
- Validate configuration at startup and publish an immutable route snapshot.
- Confine static opens using directory file descriptors; reject symlinks, traversal, and non-regular files.
- Bound static file size and cache bytes; revalidate cached files against descriptor metadata.
- Write serialized, escaped request logs to stderr and omit query strings.

### Build and project

- Require test dependencies when testing is enabled; add isolated component and real-socket regression tests.
- Add sanitizer build options and explicit Linux/macOS build requirements.
- Align container paths, non-root permissions, health checks, and read-only operation with the runtime.
- Replace unsupported performance and readiness claims with a capability reference, architecture, deployment guidance, and cited engineering research.
- Preserve the original measurement files and document their limitations.

## Historical source

The repository previously labeled its initial feature summary `1.0.0` and dated it `2025-01-09`. That prose also included unverified speedups and release guarantees; it is not used as evidence of release chronology. Consult [GitHub releases](https://github.com/Amogh-2404/Tez/releases) and Git history for actual published artifacts and commits.

The earlier implementation introduced Boost.Asio acceptance, synchronous connection workers, JSON routes, static files, LRU caches, and GoogleTest component tests. Raw ApacheBench captures from October 2025 remain in [metrics](metrics/), with context in the [performance notes](docs/performance.md).
