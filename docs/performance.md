# Performance evidence and measurement plan

There is no benchmark of the current implementation. No throughput improvement, latency reduction, or ranking against another server is established by this repository's historical captures. Architectural changes can remove known costs while introducing new ones; measurement is required to determine the net effect.

## Historical captures

These numbers are transcribed from the original files in [metrics](../metrics/), not rerun. Dates come from filenames. Each capture used ApacheBench, concurrency 10, and 100 completed requests with zero reported failures.

| Capture | Path | Body | Duration | Reported requests/s |
| --- | --- | ---: | ---: | ---: |
| [2025-10-23, first async test](../metrics/2025-10-23_22-22-12_first_async_test.txt) | `/` | 32 B | 0.009 s | 10,896.81 |
| [2025-10-23, logging middleware](../metrics/2025-10-23_22-34-14_logging_middleware.txt) | `/` | 32 B | 0.010 s | 10,107.14 |
| [2025-10-25, cached response](../metrics/Sat%20Oct%2025%2015%3A56%3A45%20IST%202025_cached_response.txt) | `/` | 170 B | 0.011 s | 8,706.25 |
| [2025-10-25, static cache](../metrics/static_file_caching_Sat%20Oct%2025%2016%3A18%3A58%20IST%202025.txt) | `/static/style.css` | 40 B | 0.011 s | 8,971.83 |

The output rounds duration, so dividing 100 by the displayed duration need not reproduce the reported requests/s. ApacheBench reports both concurrency-adjusted and aggregate time-per-request values; they are different calculations, not two independent latency measurements. Its default disables keep-alive unless `-k` is passed. The original commands and connection policy are not recorded here. [ApacheBench documentation](https://httpd.apache.org/docs/2.4/programs/ab.html)

The captures do not identify the source revision, compiler flags, CPU/OS configuration, thermal state, load generator command, or repetitions. The body sizes differ across scenarios. With only 100 requests and roughly a hundredth of a second of runtime, they do not establish steady-state capacity or a useful tail-latency distribution. Older README tables attributing these results to `/health`, JSON echo, or unrelated p99 values were unsupported and have been removed.

## Reasoning that can be checked without a benchmark

| Change | Directly inspectable property | Performance result still unknown |
| --- | --- | --- |
| Asynchronous socket sessions | Waiting on a socket does not occupy a worker in a blocking read | Throughput and latency under realistic concurrency |
| Immutable configured routes | No per-request JSON file read or redundant route-cache insertion | Size of any latency/CPU change |
| Bounded file cache | Entry and logical-byte budgets prevent unbounded cache growth | Useful hit rate and total RSS under the workload |
| File metadata revalidation | A cache hit still checks the opened file's identity and metadata | Extra syscall cost versus stale-serving alternatives |
| Body and connection limits | Accepted payload and session counts have explicit bounds | Sustainable capacity and overload recovery |

These are code properties and engineering hypotheses, not substitute benchmark results. File opens, reads, JSON serialization, response copies, and stderr writes can still dominate requests.

## Future benchmark protocol

Run performance experiments on dedicated, suitable hardware, separately from ordinary correctness tests. Record the baseline and candidate source hashes before changing anything. Use a separate load-generator machine where possible; if both processes share a host, disclose that limitation.

Capture:

- CPU model, memory, OS/kernel, architecture, compiler, optimization flags, and Boost version.
- Container image/base-image digests, CPU/memory limits, thread counts, and Tez runtime flags.
- Load-generator version and exact command or script, payload files, endpoint definitions, and response validation.
- Connection count, keep-alive policy, arrival-rate model, warmup, duration, repetitions, and run order.
- Successful and failed requests, status distribution, timeouts, p50/p95/p99 latency, achieved throughput, CPU use, and RSS.

First check the response bytes and status with an ordinary client. A high request rate for error responses is not useful application throughput. Keep the endpoint, body size, headers, connection policy, and logging equivalent across candidates.

### Workload matrix

| Workload | Purpose | Required distinction |
| --- | --- | --- |
| Fixed in-memory response | HTTP/session and serialization costs | New connections versus persistent connections |
| Small static file | File-open, metadata, and cache overhead | Application-cache warm versus cold |
| Larger static file below the limit | Buffering and write pressure | Response size, hit rate, and active connections |
| Echo POST | Body framing, allocation, and JSON escaping | Fixed-length versus chunked; text versus escape-heavy input |
| Mixed routes | Shared-resource contention | Distribution and relative payload sizes |
| Idle/slow clients | Deadline and admission behavior | Functional assertions first, then dedicated resilience experiment |
| Capacity exceeded | Failure policy and recovery | Count rejected connections and recovery time, not just completed requests |

Do not flush system caches or tune kernel settings silently. Distinguish application cache state from the OS page cache. If comparing to nginx or another server, match the observable behavior and describe remaining differences; the same URL does not imply the same work.

### Latency interpretation

A closed-loop generator issues more work only as previous work completes. During stalls it can reduce the offered load, understating queueing a constant-rate client would experience. `wrk2` documents this coordinated-omission problem and measures latency against intended request start times. This is a reason to record the arrival model, not a guarantee that one tool matches every application. [wrk2 methodology](https://github.com/giltene/wrk2#readme)

`wrk` supports multithreaded load generation and scripted requests. Verify that the generator itself has headroom, and disclose the use of script work or response parsing that can affect its ceiling. [wrk documentation](https://github.com/wg/wrk#readme)

Use repeated, adequately long runs and alternate baseline/candidate order to reduce drift. Report the distribution of results and error rate. Select duration, repetitions, and offered rates before interpreting a result; there is no universal duration that makes an experiment valid. Avoid claiming a change from differences smaller than run-to-run noise.

## Publication rule

A performance claim should link to raw output, full configuration, source hashes, and a script or exact invocation. Include regressions and failed runs that affect the conclusion. If an experiment could not be run, publish the mechanism and uncertainty only. New benchmark results belong in a separate dated record; do not overwrite the historical captures.
