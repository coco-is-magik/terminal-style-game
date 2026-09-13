#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
profiles=${PLATFORM_PROFILES_DIR:-$root/tools/platform-profiles/profiles}
output=${PLATFORM_OUTPUT:-$root/build/platform-profiles}
prepare_profile=${PLATFORM_PREPARE_PROFILE:-$root/tools/platform-profiles/prepare-profile.sh}
run_provider=${PLATFORM_RUN_PROVIDER:-$root/tools/platform-profiles/run-provider.sh}
profile_names=${PLATFORM_PROFILE_NAMES:-ubuntu-24.04-gcc ubuntu-24.04-clang fedora-gcc alpine-musl-gcc windows-10-x64-gcc}
summary="$output/survey-summary.tsv"
mkdir -p "$output"
printf 'profile\trole\tprovider_prepare\tphase\toutcome\treason\tstatus\n' >"$summary"
classified_all=true

for name in $profile_names; do
    profile="$profiles/$name.env"
    PROFILE_PROVIDER=docker
    . "$profile"
    prepare_outcome=PASS
    prepare_status=0
    "$prepare_profile" "$profile" || prepare_status=$?
    if test "$prepare_status" -eq 0; then
        :
    else
        prepare_outcome=FAIL
        prepare_result="$output/$PROFILE_ID/prepare-result.env"
        if test -r "$prepare_result"; then
            PREPARE_PHASE=provider-prepare
            . "$prepare_result"
        else
            PREPARE_PHASE=provider-prepare
            OUTCOME=FAIL-TOOL
            REASON=provider-prepare-result-missing
            STATUS=3
            classified_all=false
        fi
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
          "$PROFILE_ID" "$PROFILE_ROLE" "$prepare_outcome" "$PREPARE_PHASE" \
          "$OUTCOME" "$REASON" "$STATUS" >>"$summary"
        continue
    fi
    "$run_provider" "$profile" || true
    result="$output/$PROFILE_ID/result.env"
    if test ! -r "$result"; then
        classified_all=false
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
          "$PROFILE_ID" "$PROFILE_ROLE" "$prepare_outcome" unknown \
          FAIL-TOOL result-missing 3 >>"$summary"
        continue
    fi
    . "$result"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "$PROFILE_ID" "$PROFILE_ROLE" "$prepare_outcome" "$PHASE" \
      "$OUTCOME" "$REASON" "$STATUS" >>"$summary"
done

cat "$summary"
if test "$classified_all" != true; then
    echo "FAIL-TOOL: reason=survey-incomplete"
    exit 3
fi
echo "PASS: platform survey completed; compatibility outcomes are recorded above"