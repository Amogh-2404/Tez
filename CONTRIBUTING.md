# Contributing to Tez

Small, reviewable changes are easiest to get right. Describe the concrete problem, the behavior your patch changes, and how you checked it. For larger features, open an issue first to agree on the scope.

## Build and test

Install the dependencies in the [README](README.md#build-and-run), then run from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DTEZ_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

`BUILD_TESTING=ON` requires GoogleTest and Python 3.8+. A missing dependency must fail configuration rather than produce a build that silently skips tests. Use `BUILD_TESTING=OFF` only for an intentional server-only build.

The suite includes component tests and a Python integration test that starts Tez on an ephemeral loopback port, sends HTTP over real sockets, and stops its child processes. Fixtures use temporary directories. Tests must not overwrite the repository's config or static files.

For memory and undefined-behavior checks with a supported GCC or Clang toolchain:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DTEZ_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel 2
ctest --test-dir build-asan --output-on-failure
```

For concurrency checks, use a separate build with `-DTEZ_ENABLE_TSAN=ON`. Do not combine ThreadSanitizer with AddressSanitizer. Runtime availability depends on the compiler and host; record a skipped or unsupported sanitizer honestly. If a macOS sanitizer runtime reports unsupported leak detection, use `ASAN_OPTIONS=detect_leaks=0` for that toolchain. This does not disable AddressSanitizer or UndefinedBehaviorSanitizer. Linux CI keeps leak detection enabled.

## Review expectations

- Keep C++17 compatibility and use RAII for sockets, file descriptors, and other resources.
- Follow the repository's formatting configuration. Avoid unrelated reformatting in a functional patch.
- Make ownership, buffer lifetime, and executor assumptions visible near asynchronous operations.
- Test protocol changes through a socket as well as through helper functions. Splitting or coalescing TCP writes must not change the parsed requests.
- Give caches explicit identity, freshness, size, and synchronization rules.
- Document changes to defaults, command-line flags, response semantics, resource limits, and supported platforms.
- Use conventional commit messages such as `fix(http): preserve buffered request bodies` or `docs: clarify image configuration`.

A useful regression test fails before the fix and checks a meaningful outcome. Avoid conditional assertions that pass when the feature under test is absent, and avoid sleeps for TTL tests when an injected clock can express the condition directly.

## Performance changes

Do not infer speedups from fewer lines of code or a short localhost run. Explain the cost being removed, the workload expected to benefit, and possible regressions. Without a controlled measurement, label the result as a hypothesis.

The [performance plan](docs/performance.md) defines the evidence expected for quantitative claims. Benchmarks are separate, opt-in work on suitable hardware; routine builds and tests must not trigger them.

## Issues and pull requests

Bug reports should include the commit or image digest, operating system, compiler and Boost versions, build options, command line, and a minimal reproducer. For HTTP bugs, a small raw request is often more useful than a screenshot. Remove credentials and personal data from logs.

Start a feature branch from the current default branch. Keep the pull request description focused on the final change and include the commands you ran. Mark checks you could not run and explain why. Never present an unexecuted check as passing.

Security reports belong in the private channel described in [SECURITY.md](SECURITY.md), not in public issues. Participation follows the [Code of Conduct](CODE_OF_CONDUCT.md). Contributions are accepted under the repository's [MIT license](LICENSE).
