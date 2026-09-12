#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
. "$root/tools/valgrind-container/classify.sh"
image=${VALGRIND_IMAGE:-terminal-style-game-valgrind:ubuntu24.04-amd64}
output="$root/build/valgrind-container"
mkdir -p "$output"

if ! command -v docker >/dev/null 2>&1; then
    echo "FAIL-MISSING-TOOL: reason=docker-not-found"
    exit 2
fi

status=0
timeout 1800s docker run --rm \
  --network none \
  --cap-drop ALL \
  --security-opt no-new-privileges \
  --user "$(id -u):$(id -g)" \
  --group-add "$(stat -c %g "$root")" \
  --read-only \
  --tmpfs /tmp:rw,noexec,nosuid,nodev,mode=1777 \
  --tmpfs /work:rw,exec,nosuid,nodev,mode=1777 \
  --mount "type=bind,src=$root,dst=/source,readonly" \
  --mount "type=bind,src=$output,dst=/output" \
  "$image" >"$output/docker.log" 2>&1 || status=$?
cat "$output/docker.log"
classify_result docker "$status" "$output/docker.log" || exit $?