# Deployment

Use a build of the current source when following these instructions. Historical Docker tags may predate the command-line interface and filesystem fixes.

## Container quick start

From the repository root:

```sh
docker build -t tez:local .
docker run --rm --name tez \
  --read-only --cap-drop=ALL --security-opt=no-new-privileges \
  -p 127.0.0.1:8080:8080 tez:local
```

The command stays in the foreground. Stop it with `Ctrl-C`, or run `docker stop tez` from another terminal. Check it with `curl --fail http://127.0.0.1:8080/health`.

| Image setting | Value |
| --- | --- |
| Executable | `/usr/local/bin/Tez` |
| Working directory | `/app` |
| User and group | `10001:10001` |
| Container listener | `0.0.0.0:8080` |
| Route configuration | `/app/config.json` |
| Static root | `/app/static` |
| Request logs | stderr; inspect with `docker logs tez` |
| Writable application data | None required |

The container's all-interface listener is needed for port publishing. The host-side `127.0.0.1:8080:8080` mapping limits the example to loopback. Omitting the host address exposes the port more broadly; choose that intentionally.

## Development registry image

The publication workflow promotes verified amd64/arm64 development builds to `ramogh2404/tez:main`. Check its revision on the [registry Tags page](https://hub.docker.com/r/ramogh2404/tez/tags) before using it; `main` may lag the current source.

```sh
docker pull ramogh2404/tez:main
docker run --rm --name tez \
  --read-only --cap-drop=ALL --security-opt=no-new-privileges \
  -p 127.0.0.1:8080:8080 ramogh2404/tez:main
```

This is a mutable development tag. Pin the verified `ramogh2404/tez@sha256:…` digest for a deployment. Historical `latest` and `1.0.0` images may have different behavior; a tag's name is not a source revision.

## Mount your content

```sh
docker run --rm --name tez \
  --read-only --cap-drop=ALL --security-opt=no-new-privileges \
  -p 127.0.0.1:8080:8080 \
  --mount type=bind,src="$(pwd)/config.json",dst=/app/config.json,readonly \
  --mount type=bind,src="$(pwd)/static",dst=/app/static,readonly \
  tez:local
```

The files must be readable and their directories traversable by UID `10001`. Keep mounted content under trusted control. Route changes require a restart; static requests revalidate file metadata before using cache entries.

Arguments after the image name extend the image's entrypoint defaults. Add only the settings you want to change; when a valued option appears more than once, the last value wins:

```sh
docker run --rm --name tez \
  --read-only --cap-drop=ALL --security-opt=no-new-privileges \
  --memory=512m --cpus=2 --pids-limit=128 \
  -p 127.0.0.1:8080:8080 \
  tez:local \
  --threads 2 --max-connections 32 --body-limit 1048576 --timeout 15
```

The resource values are an example allocation, not a measured capacity guarantee. A large response is buffered for each active request; align file sizes, concurrency, and memory limits with the actual workload. The image health check targets port `8080`; override it if you change the internal port. Changing only the published host port does not require a different health check.

Inspect CLI help or validate the bundled configuration without starting a listener:

```sh
docker run --rm tez:local --help
docker run --rm tez:local --version
docker run --rm --read-only tez:local --check-config
```

For content mounted elsewhere, override just the relevant paths. This checks the route configuration and static root, then exits:

```sh
docker run --rm --read-only \
  --mount type=bind,src="$(pwd)/config.json",dst=/content/config.json,readonly \
  --mount type=bind,src="$(pwd)/static",dst=/content/static,readonly \
  tez:local --config /content/config.json --static-dir /content/static --check-config
```

Successful validation prints the resolved paths and configured route count. It does not test HTTP responses or every file below the static root.

## Compose

The repository's [Compose file](../docker-compose.yml) builds the current source, mounts the route configuration and static tree read-only, and publishes on loopback.

```sh
docker compose up --build
```

Stop the foreground process with `Ctrl-C`, then remove its container and network when finished:

```sh
docker compose down
```

Request logs go to the container's stderr stream. No `logs/` bind mount is required.

## Image provenance

The registry repository is [ramogh2404/tez](https://hub.docker.com/r/ramogh2404/tez). A tag's name alone does not prove that it contains the current checkout. Inspect its creation time, supported architecture, source revision label, and digest before using it.

At the start of the 1.1.0 development work, the published `latest`, `1.0.0`, and `main` tags referred to historical amd64 images. New capabilities in this source tree are not retroactively present in those images. See the registry's [Tags page](https://hub.docker.com/r/ramogh2404/tez/tags) for published artifacts; use a verified digest when reproducibility matters.

The maintained Docker Hub overview is [dockerhub.md](dockerhub.md). It distinguishes development-image usage from a source build and historical release tags.

## Native service

Use absolute config/static paths and an unprivileged service account. Keep the listener on loopback behind a separately managed TLS reverse proxy. Tez logs the socket peer; forwarded headers do not establish a trusted client identity.

The application does not implement TLS, authentication, authorization, rate limiting, or graceful request draining. SIGINT and SIGTERM stop the network event loop; a request in progress can be interrupted. Remove an instance from incoming traffic before stopping it when that matters to callers.

Filesystem reads and logging are synchronous. Avoid network filesystems or blocked log sinks if predictable responsiveness is important. A health response confirms the HTTP process can answer that request; it is not proof that every configured route or storage operation is healthy.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Native startup cannot load configuration | Use explicit paths; check working directory, syntax, permissions, and size |
| Container runs but cannot be reached | Check port mapping and any explicit `--address` or `--port` override |
| Static file returns `403` | Remove symlinks, traversal components, or invalid encodings; check permissions |
| Static file returns `404` | Verify the mounted path and case-sensitive filename |
| Container health fails on a custom port | Override the health check's URL to match the internal port |
| Client connection closes during overload | Lower client concurrency or adjust the connection cap with a memory budget |
| No `server.log` file appears | Request logs now go to stderr |
