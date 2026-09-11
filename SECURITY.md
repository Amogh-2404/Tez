# Security policy

Tez is a personal C++ HTTP server project under active development. The development branch contains changes that may not exist in published releases or container tags. Use the exact commit or image digest when reporting a problem. There is no security support SLA or claim of comprehensive security certification.

## Report privately

Email **[ramogh2404@gmail.com](mailto:ramogh2404@gmail.com)** with the affected version, a minimal reproducer, expected and observed behavior, and the likely impact. Use `Tez security report` in the subject. Do not include real credentials, customer data, or an exploit against a system you do not control.

Please allow time to investigate before publishing details. Disclosure timing and attribution can be coordinated with the reporter. If you have not received an acknowledgment, send a follow-up; a fixed response deadline cannot be guaranteed for this independently maintained project.

## Security boundaries

The current implementation uses HTTP parser limits and validation, socket operation deadlines, a connection cap, descriptor-relative static file access, and a byte- and entry-bounded file cache. Exact defaults and rejection behavior are documented in [configuration](docs/configuration.md#limits).

These controls have specific scopes:

- Limits apply to individual requests, connections, files, or caches. They are not a bound on process RSS, CPU usage, or aggregate traffic.
- Static path handling rejects symlinks and traversal components. The document root and its contents must remain under trusted administrative control; hard links and mount points are not a tenant isolation mechanism.
- Cache entries are revalidated against file metadata, not cryptographically verified. LRU eviction is a resource policy, not protection against every form of cache abuse.
- Request logs omit query strings and escape control bytes. Paths and peer addresses can still contain sensitive operational information; access and retention belong to the operator.
- Socket deadlines do not interrupt synchronous filesystem operations or a blocked stderr sink.
- Shutdown cancels network work; it does not guarantee that in-flight responses are drained.

## Deployment scope

Tez does not implement TLS, authentication, authorization, per-client rate limits, or persistent application storage. Keep the default loopback bind for local use. Public deployment requires an independently maintained TLS gateway and an explicit access policy, plus process/container memory and CPU limits. A reverse proxy does not make an unaudited backend automatically production-ready.

Only place public, trusted files in the static root. Keep config, private keys, credentials, and writable application data outside it. Run under an unprivileged account with read-only access to the binary, route configuration, and static tree. See [deployment](docs/deployment.md) for the container contract.

## Maintenance

Security fixes target the current development line. Older tags are not promised backports. Release notes describe fixes when an affected version and behavior are established; this document does not assign invented CVE identifiers, CVSS scores, or historical vulnerability dates.
