#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
profile_path=$1
PROFILE_PROVIDER=docker
. "$profile_path"
output=${PLATFORM_OUTPUT:-$root/build/platform-profiles}
result="$output/$PROFILE_ID/prepare-result.env"
mkdir -p "$output/$PROFILE_ID"
rm -f "$result"

case "$PROFILE_PROVIDER" in
    docker)
        prepare=${PLATFORM_DOCKER_PREPARE:-$root/tools/platform-profiles/build-image.sh}
        provider_result="$output/$PROFILE_ID/image-result.env"
        ;;
    *)
        printf 'PREPARE_PHASE=provider-prepare\nOUTCOME=FAIL-TOOL\nREASON=unsupported-profile-provider\nSTATUS=3\n' \
          >"$result"
        exit 3
        ;;
esac

if test "$provider_result" != "$result"; then
    rm -f "$provider_result"
fi
status=0
"$prepare" "$profile_path" || status=$?
if test -r "$provider_result" && test "$provider_result" != "$result"; then
    cp "$provider_result" "$result"
    printf 'PREPARE_PHASE=image-build\n' >>"$result"
fi
if test "$status" -eq 0 && test ! -r "$result"; then
    printf 'PREPARE_PHASE=provider-prepare\nOUTCOME=PASS\nREASON=provider-prepared\nSTATUS=0\n' \
      >"$result"
fi
exit "$status"