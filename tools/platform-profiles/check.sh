#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
summary=${PLATFORM_SUMMARY:-$root/build/platform-profiles/survey-summary.tsv}
if test ! -r "$summary"; then
    echo "FAIL-TOOL: reason=platform-survey-results-missing"
    exit 3
fi
status=0
tab=$(printf '\t')
while IFS="$tab" read -r profile role image_build phase outcome reason code; do
    test "$profile" = profile && continue
    if test "$role" = required && test "$outcome" != PASS; then
        echo "FAIL-PRODUCT: profile=$profile phase=$phase reason=$reason status=$code"
        status=1
    fi
done <"$summary"
test "$status" -eq 0 || exit "$status"
echo "PASS: all required platform profiles passed"