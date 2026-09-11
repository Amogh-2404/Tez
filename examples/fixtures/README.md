# Fixed HTTP fixtures

Serve repeatable responses from a version-controlled JSON file. These routes support `GET` and `HEAD`; they do not store data, select responses by query string, or simulate stateful application logic.

Build Tez using the [main instructions](../../README.md#build-and-run). Run these commands from the repository root:

```sh
./build/Tez --check-config \
  --config examples/fixtures/routes.json --static-dir examples/fixtures/static
./build/Tez \
  --config examples/fixtures/routes.json --static-dir examples/fixtures/static
```

The check command validates the config and static root without opening a listener. It returns a nonzero exit status on failure. The second command runs Tez in the foreground; stop it with `Ctrl-C`.

Open [http://127.0.0.1:8080/static/index.html](http://127.0.0.1:8080/static/index.html) to send real requests from a small static page. Both the page and fixtures use the same origin; no CORS setup is needed for this example. Opening the HTML directly from disk will not connect it to Tez.

## Fetch a response

In another terminal:

```sh
curl --fail http://127.0.0.1:8080/fixtures/profile
```

Expected body:

```json
{"name":"Ada","plan":"starter"}
```

Change `starter` to `pro` in the profile route's `body` string in [routes.json](routes.json). Stop Tez, rerun the validation and start commands, and fetch the route again:

```json
{"name":"Ada","plan":"pro"}
```

Changes take effect only when Tez starts again. The JSON `body` field is literal response text, so JSON response bodies use escaped quotes inside that string. Tez validates the route structure, not the syntax of a JSON document embedded in `body`.

## Check a failure path

```sh
curl --include http://127.0.0.1:8080/fixtures/unavailable
```

The response status is `503 Service Unavailable`, with this body:

```json
{"error":"temporarily_unavailable","message":"Try again later."}
```

This fixed error response lets a client exercise its error-handling path. The example intentionally omits curl's `--fail` so the response body is visible on error statuses.

## Inspect headers

```sh
curl --head http://127.0.0.1:8080/fixtures/profile
```

`HEAD` returns the configured status and headers without a response body. `Content-Length` describes the body a `GET` would return. Sending `POST` to a configured fixture returns `405` with `Allow: GET, HEAD`.

## Serve a file

[static/index.html](static/index.html), [static/style.css](static/style.css), and [static/demo.js](static/demo.js) are served through `/static/`. To add a plain text file:

```sh
printf 'a local file\n' > examples/fixtures/static/hello.txt
curl --fail http://127.0.0.1:8080/static/hello.txt
```

The response is `a local file` followed by a newline. Static file changes do not require a route-config restart. Request the filename explicitly; Tez does not select directory index files automatically.

## Explore a request body

The built-in `/echo` endpoint is separate from the fixed fixtures:

```sh
curl --fail http://127.0.0.1:8080/echo \
  -H 'Content-Type: text/plain' --data-binary 'hello'
```

Its JSON response includes `method: "POST"`, `received_body: "hello"`, and `body_length: 5`. The byte count describes the received body. Nothing is persisted.

See the [configuration reference](../../docs/configuration.md) for the full schema, reserved paths, validation rules, and limits.
