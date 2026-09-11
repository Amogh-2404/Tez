# Tez

**HTTP, close to the metal.**

A compact C++17 HTTP server built on Boost.Beast and Boost.Asio. JSON routes, static files, explicit resource limits, and a codebase designed to be read.

[Source](https://github.com/Amogh-2404/Tez) · [Documentation](https://github.com/Amogh-2404/Tez/tree/main/docs) · [Issues](https://github.com/Amogh-2404/Tez/issues) · [License](https://github.com/Amogh-2404/Tez/blob/main/LICENSE)

## Version note

The source is being developed toward 1.1.0. Existing registry tags can contain the earlier implementation. The commands and capabilities below describe a build of the current source; do not assume a historical `latest` or `1.0.0` image has them. Check the [published tags](https://hub.docker.com/r/ramogh2404/tez/tags), image revision, architecture, and digest before choosing a registry image.

## Run the current source

```sh
git clone https://github.com/Amogh-2404/Tez.git
cd Tez
docker build -t tez:local .
docker run --rm --name tez \
  --read-only --cap-drop=ALL --security-opt=no-new-privileges \
  -p 127.0.0.1:8080:8080 tez:local
```

Then, in another terminal:

```sh
curl --fail http://127.0.0.1:8080/health
```

Stop with `Ctrl-C` or `docker stop tez`. The health endpoint returns `{"status":"ok"}`.

## Bring your routes and files

```sh
docker run --rm --name tez \
  --read-only --cap-drop=ALL --security-opt=no-new-privileges \
  -p 127.0.0.1:8080:8080 \
  --mount type=bind,src="$(pwd)/config.json",dst=/app/config.json,readonly \
  --mount type=bind,src="$(pwd)/static",dst=/app/static,readonly \
  tez:local
```

A route configuration looks like this:

```json
{
  "/hello": {
    "status": "200 OK",
    "content_type": "text/plain; charset=utf-8",
    "body": "hello, Tez\n"
  }
}
```

A file at `static/style.css` is served at `/static/style.css`. Configured routes are loaded at startup; restart after editing them. Mounted files must be readable by UID `10001`. Symlinks below the static root are rejected.

## Image contract

| Setting | Value |
| --- | --- |
| Entrypoint | `/usr/local/bin/Tez` |
| User | `10001:10001` |
| Listener inside container | `0.0.0.0:8080` |
| Configuration | `/app/config.json` |
| Static root | `/app/static` |
| Request logs | stderr, available through `docker logs` |
| Filesystem | Read-only operation supported |

Inspect options with `docker run --rm tez:local --help`.

Arguments after the image name replace the default command arguments. To tune workers, repeat the listener and paths:

```sh
docker run --rm -p 127.0.0.1:8080:8080 tez:local \
  --address 0.0.0.0 --config /app/config.json --static-dir /app/static \
  --threads 2 --max-connections 32 --timeout 15
```

## Scope and limits

HTTP/1.x sessions support keep-alive, ordered pipelining, chunked request bodies, `HEAD`, and `Expect: 100-continue`. Defaults include an 8 KiB header limit, 1 MiB request-body limit, 128 admitted connections, and 30-second socket phase deadlines. Static files are limited to 16 MiB; the file cache is limited to 50 entries and 32 MiB of logical data.

These are individual limits, not a process-memory guarantee. Use an explicit container memory and CPU budget. TLS, HTTP/2, HTTP/3, WebSocket, authentication, rate limiting, and persistent application storage are not implemented. Filesystem reads and logging remain synchronous within request handlers.

Tez is an independently maintained systems project. No current throughput ranking or production-readiness guarantee is claimed. Use a maintained TLS gateway and restrict backend access for any exposed deployment.

[Full deployment reference](https://github.com/Amogh-2404/Tez/blob/main/docs/deployment.md) · [Security reports](https://github.com/Amogh-2404/Tez/blob/main/SECURITY.md)

Maintained by R. Amogh. MIT licensed.
