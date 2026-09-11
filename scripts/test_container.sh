#!/usr/bin/env bash
set -euo pipefail

container_id=''
command_output=''
fixture=$(mktemp -d)
cleanup() {
  if [[ -n "$container_id" ]]; then
    docker logs "$container_id"
    docker rm -f "$container_id" >/dev/null
  fi
  rm -rf "$fixture"
}
trap cleanup EXIT

start_container() {
  container_id=$(docker run -d --read-only --cap-drop=ALL \
    --security-opt=no-new-privileges --pids-limit=64 --memory=256m \
    -p 127.0.0.1::8080 "$@")
  local port
  port=$(docker inspect --format '{{(index (index .NetworkSettings.Ports "8080/tcp") 0).HostPort}}' "$container_id")
  base_url="http://127.0.0.1:$port"
  for attempt in {1..30}; do
    if curl --fail --silent --max-time 2 "$base_url/health" > "$fixture/health.json"; then
      python3 -c 'import json,sys; assert json.load(open(sys.argv[1])) == {"status": "ok"}' "$fixture/health.json"
      return
    fi
    sleep 1
  done
  echo 'Container did not become healthy.' >&2
  return 1
}

stop_container() {
  docker stop --time 5 "$container_id" > /dev/null
  test "$(docker inspect --format '{{.State.ExitCode}}' "$container_id")" = '0'
  docker rm "$container_id" > /dev/null
  container_id=''
}

run_command() {
  local expected_exit=$1
  shift
  container_id=$(docker run -d --read-only --network=none --cap-drop=ALL \
    --security-opt=no-new-privileges --pids-limit=64 --memory=256m "$@")
  local actual_exit
  for attempt in {1..10}; do
    if [[ "$(docker inspect --format '{{.State.Running}}' "$container_id")" == 'false' ]]; then
      command_output=$(docker logs "$container_id" 2>&1)
      actual_exit=$(docker inspect --format '{{.State.ExitCode}}' "$container_id")
      if [[ "$actual_exit" != "$expected_exit" ]]; then
        printf 'Expected exit %s, got %s.\n%s\n' "$expected_exit" "$actual_exit" "$command_output" >&2
        return 1
      fi
      docker rm "$container_id" > /dev/null
      container_id=''
      return
    fi
    sleep 1
  done
  echo 'Container command did not exit within 10 seconds.' >&2
  return 1
}

assert_command_output() {
  if [[ "$command_output" != *"$1"* ]]; then
    printf 'Expected output containing %s.\n%s\n' "$1" "$command_output" >&2
    return 1
  fi
}

start_container tez:test
image_version=$(docker image inspect --format '{{index .Config.Labels "org.opencontainers.image.version"}}' tez:test)
test "$(docker exec "$container_id" /usr/local/bin/Tez --version)" = "Tez $image_version"
curl --fail --silent --max-time 2 "$base_url/" > /dev/null
curl --fail --silent --max-time 2 "$base_url/static/style.css" > /dev/null
curl --fail --silent --head --max-time 2 "$base_url/health" > /dev/null
test "$(docker inspect --format '{{.Config.User}}' "$container_id")" = '10001:10001'
docker exec "$container_id" wget -q -T 2 -O /dev/null http://127.0.0.1:8080/health
stop_container

# User flags retain the image's address and absolute paths, even from another cwd.
start_container --workdir / tez:test --threads 2
[[ "$(curl --fail --silent --max-time 2 "$base_url/test")" == 'Test Page!' ]]
curl --fail --silent --max-time 2 "$base_url/static/style.css" > /dev/null
[[ "$(docker logs "$container_id" 2>&1)" == *'with 2 I/O worker(s)'* ]]
stop_container

run_command 0 tez:test --threads 2 --help
assert_command_output 'Usage: Tez [options]'
assert_command_output '--check-config'
run_command 0 tez:test -h
assert_command_output 'Usage: Tez [options]'
run_command 0 tez:test --version
[[ "$command_output" == "Tez $image_version" ]]
run_command 0 --workdir / tez:test --check-config
assert_command_output 'Configuration valid.'
assert_command_output '/app/config.json'
assert_command_output '/app/static'
run_command 1 tez:test --config /missing/tez/routes.json --check-config
assert_command_output '/missing/tez/routes.json'
run_command 1 tez:test --address 999.999.999.999 --check-config
assert_command_output 'Invalid value for --address'

mkdir "$fixture/static"
cat > "$fixture/routes.json" <<'JSON'
{"/custom":{"status":"200 OK","content_type":"text/plain","body":"mounted route"}}
JSON
printf '%s' 'mounted file' > "$fixture/static/check.txt"
chmod 755 "$fixture" "$fixture/static"
chmod 644 "$fixture/routes.json" "$fixture/static/check.txt"
run_command 0 \
  --mount "type=bind,src=$fixture,dst=/content,readonly" \
  tez:test --config /content/routes.json --static-dir /content/static --check-config
assert_command_output 'Configuration valid.'
assert_command_output '/content/routes.json'
assert_command_output '/content/static'
start_container \
  --mount "type=bind,src=$fixture,dst=/content,readonly" \
  tez:test --config /content/routes.json --static-dir /content/static --threads 1
[[ "$(curl --fail --silent --max-time 2 "$base_url/custom")" == 'mounted route' ]]
[[ "$(curl --fail --silent --max-time 2 "$base_url/static/check.txt")" == 'mounted file' ]]
stop_container
