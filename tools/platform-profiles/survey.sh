#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
profiles=${PLATFORM_PROFILES_DIR:-$root/tools/platform-profiles/profiles}
output=${PLATFORM_OUTPUT:-$root/build/platform-profiles}
build_image=${PLATFORM_BUILD_IMAGE:-$root/tools/platform-profiles/build-image.sh}
run_one=${PLATFORM_RUN_ONE:-$root/tools/platform-profiles/run-one.sh}
profile_names=${PLATFORM_PROFILE_NAMES:-ubuntu-24.04-gcc ubuntu-24.04-clang fedora-gcc alpine-musl-gcc}
summary="$output/survey-summary.tsv"
mkdir -p "$output"
printf 'profile\trole\timage_build\tphase\toutcome\treason\tstatus\n' >"$summary"
classified_all=true

for name in $profile_names; do
    profile="$profiles/$name.env"
    . "$profile"
    image_outcome=PASS
    image_status=0
    "$build_image" "$profile" || image_status=$?
    if test "$image_status" -eq 0; then
        :
    else
        image_outcome=FAIL
        image_result="$output/$PROFILE_ID/image-result.env"
        if test -r "$image_result"; then
            . "$image_result"
        else
            OUTCOME=FAIL-TOOL
            REASON=image-build-result-missing
            STATUS=3
            classified_all=false
        fi
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
          "$PROFILE_ID" "$PROFILE_ROLE" "$image_outcome" image-build \
          "$OUTCOME" "$REASON" "$STATUS" >>"$summary"
        continue
    fi
    "$run_one" "$profile" || true
    result="$output/$PROFILE_ID/result.env"
    if test ! -r "$result"; then
        classified_all=false
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
          "$PROFILE_ID" "$PROFILE_ROLE" "$image_outcome" unknown \
          FAIL-TOOL result-missing 3 >>"$summary"
        continue
    fi
    . "$result"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "$PROFILE_ID" "$PROFILE_ROLE" "$image_outcome" "$PHASE" \
      "$OUTCOME" "$REASON" "$STATUS" >>"$summary"
done

cat "$summary"
if test "$classified_all" != true; then
    echo "FAIL-TOOL: reason=survey-incomplete"
    exit 3
fi
echo "PASS: platform survey completed; compatibility outcomes are recorded above"