#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
. "$root/tools/valgrind-container/classify.sh"
cd "$root" || exit 3

if ! command -v "${VALGRIND:-valgrind}" >/dev/null 2>&1; then
    classify_result missing 127 /dev/null
    exit $?
fi
if ! make build/test-decal-io build/test-core; then
    echo "FAIL-PRODUCT: reason=leak-runner-build-failure"
    exit 1
fi

for runner in build/test-decal-io build/test-core; do
    name=${runner##*/}
    direct_log="build/native-direct-${name}.log"
    memcheck_log="build/native-valgrind-${name}.log"
    status=0
    "./$runner" >"$direct_log" 2>&1 || status=$?
    cat "$direct_log"
    classify_result direct "$status" "$direct_log" || exit $?
    status=0
    timeout 900s "${VALGRIND:-valgrind}" --error-exitcode=100 --leak-check=full \
      --show-leak-kinds=all --errors-for-leak-kinds=definite,indirect,possible \
      "./$runner" >"$memcheck_log" 2>&1 || status=$?
    cat "$memcheck_log"
    classify_result memcheck "$status" "$memcheck_log" || exit $?
done
echo "PASS: native Valgrind diagnostic passed"