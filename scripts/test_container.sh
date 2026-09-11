#!/usr/bin/env bash
set -euo pipefail

container_id=''
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

start_container tez:test
image_version=$(docker image inspect --format '{{index .Config.Labels "org.opencontainers.image.version"}}' tez:test)
test "$(docker exec "$container_id" /usr/local/bin/Tez --version)" = "Tez $image_version"
curl --fail --silent --max-time 2 "$base_url/" > /dev/null
curl --fail --silent --max-time 2 "$base_url/static/style.css" > /dev/null
curl --fail --silent --head --max-time 2 "$base_url/health" > /dev/null
test "$(docker inspect --format '{{.Config.User}}' "$container_id")" = '10001:10001'
docker exec "$container_id" wget -q -T 2 -O /dev/null http://127.0.0.1:8080/health
stop_container

mkdir "$fixture/static"
cat > "$fixture/routes.json" <<'JSON'
{"/custom":{"status":"200 OK","content_type":"text/plain","body":"mounted route"}}
JSON
printf '%s' 'mounted file' > "$fixture/static/check.txt"
chmod 755 "$fixture" "$fixture/static"
chmod 644 "$fixture/routes.json" "$fixture/static/check.txt"
start_container \
  --mount "type=bind,src=$fixture/routes.json,dst=/app/config.json,readonly" \
  --mount "type=bind,src=$fixture/static,dst=/app/static,readonly" \
  tez:test --address 0.0.0.0 --config /app/config.json --static-dir /app/static --threads 1
[[ "$(curl --fail --silent --max-time 2 "$base_url/custom")" == 'mounted route' ]]
[[ "$(curl --fail --silent --max-time 2 "$base_url/static/check.txt")" == 'mounted file' ]]
stop_container
