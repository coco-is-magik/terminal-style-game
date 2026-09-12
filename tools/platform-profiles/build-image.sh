#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
profile_path=$1
. "$profile_path"
. "$root/tools/platform-profiles/dependencies.env"
. "$root/tools/platform-profiles/classify.sh"
output="$root/build/platform-profiles/$PROFILE_ID"
result="$output/image-result.env"
mkdir -p "$output"

if ! command -v docker >/dev/null 2>&1; then
    echo "FAIL-MISSING-TOOL: profile=$PROFILE_ID reason=docker-not-found"
    printf 'OUTCOME=FAIL-MISSING-TOOL\nREASON=docker-not-found\nSTATUS=127\n' >"$result"
    exit 2
fi

status=0
timeout 1800s docker build --pull=false \
  -f "$root/tools/platform-profiles/$PROFILE_DOCKERFILE" \
  -t "$PROFILE_IMAGE" \
  --build-arg "PROFILE_ID=$PROFILE_ID" \
  --build-arg "PROFILE_ROLE=$PROFILE_ROLE" \
  --build-arg "PROFILE_COMPILER=$PROFILE_COMPILER" \
  --build-arg "PROFILE_COMPILER_PACKAGE=$PROFILE_COMPILER_PACKAGE" \
  --build-arg "SDL_COMMIT=$SDL_COMMIT" --build-arg "SDL_SHA256=$SDL_SHA256" \
  --build-arg "SDL_MIXER_COMMIT=$SDL_MIXER_COMMIT" \
  --build-arg "SDL_MIXER_SHA256=$SDL_MIXER_SHA256" \
  --build-arg "ENET_COMMIT=$ENET_COMMIT" --build-arg "ENET_SHA256=$ENET_SHA256" \
  --build-arg "CMOCKA_COMMIT=$CMOCKA_COMMIT" \
  --build-arg "CMOCKA_SHA256=$CMOCKA_SHA256" \
  --build-arg "SMC_COMMIT=$SMC_COMMIT" --build-arg "SMC_SHA256=$SMC_SHA256" \
  "$root/tools/platform-profiles" >"$output/image-build.log" 2>&1 || status=$?

classification=$(classify_platform_result image-build "$status" "$output/image-build.log")
classified_status=$?
outcome=${classification%%|*}
rest=${classification#*|}
reason=${rest%%|*}
classified_code=${rest##*|}
{
    echo "OUTCOME=$outcome"
    echo "REASON=$reason"
    echo "STATUS=$classified_code"
} >"$result"

if test "$classified_status" -eq 0; then
    docker image inspect "$PROFILE_IMAGE" \
      --format 'image_id={{.Id}} created={{.Created}} os={{.Os}} architecture={{.Architecture}}' \
      >"$output/image-identity.txt"
    echo "PASS: profile=$PROFILE_ID phase=image-build"
    exit 0
fi
echo "$outcome: profile=$PROFILE_ID phase=image-build reason=$reason status=$classified_code"
exit "$classified_status"