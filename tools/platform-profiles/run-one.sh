#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
profile_path=$1
. "$profile_path"
. "$root/tools/platform-profiles/classify.sh"
output="$root/build/platform-profiles/$PROFILE_ID"
mkdir -p "$output"
rm -f "$output/result.env"

if ! command -v docker >/dev/null 2>&1; then
    echo "FAIL-MISSING-TOOL: profile=$PROFILE_ID reason=docker-not-found"
    exit 2
fi

status=0
timeout 3600s docker run --rm \
  --network none --cap-drop ALL --security-opt no-new-privileges \
  --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$root")" \
  --read-only \
  --tmpfs /tmp:rw,noexec,nosuid,nodev,mode=1777 \
  --tmpfs /work:rw,exec,nosuid,nodev,mode=1777 \
  --mount "type=bind,src=$root,dst=/source,readonly" \
  --mount "type=bind,src=$output,dst=/output" \
  "$PROFILE_IMAGE" >"$output/docker.log" 2>&1 || status=$?

if test -r "$output/result.env"; then
    cat "$output/result.env"
    exit "$status"
fi

classification=$(classify_platform_result docker "$status" "$output/docker.log")
classified_status=$?
outcome=${classification%%|*}
rest=${classification#*|}
reason=${rest%%|*}
classified_code=${rest##*|}
{
    echo "PROFILE_ID=$PROFILE_ID"
    echo "PROFILE_ROLE=$PROFILE_ROLE"
    echo "PHASE=container-start"
    echo "OUTCOME=$outcome"
    echo "REASON=$reason"
    echo "STATUS=$classified_code"
} >"$output/result.env"
cat "$output/result.env"
exit "$classified_status"