#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/classify.sh"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

check() {
    expected_status=$1
    expected_text=$2
    shift 2
    status=0
    output=$(classify_platform_result "$@") || status=$?
    test "$status" -eq "$expected_status"
    test "$output" = "$expected_text"
}

: >"$tmp/empty.log"
printf '%s\n' 'internal compiler error: controlled fixture' >"$tmp/ice.log"
printf '%s\n' 'CMake Error at dependency/CMakeLists.txt:1' >"$tmp/cmake.log"
check 0 'PASS|image-built|0' image-build 0 "$tmp/empty.log"
check 1 'FAIL-PRODUCT|dependency-build-failure|1' image-build 1 "$tmp/cmake.log"
check 3 'FAIL-TOOL|image-build-failure|1' image-build 1 "$tmp/empty.log"
check 4 'FAIL-TIMEOUT|image-build-timeout|124' image-build 124 "$tmp/empty.log"
check 0 'PASS|container-completed|0' docker 0 "$tmp/empty.log"
check 3 'FAIL-TOOL|docker-daemon-or-runtime|125' docker 125 "$tmp/empty.log"
check 3 'FAIL-TOOL|container-command-not-executable|126' docker 126 "$tmp/empty.log"
check 3 'FAIL-TOOL|container-command-not-found|127' docker 127 "$tmp/empty.log"
check 4 'FAIL-TIMEOUT|container-timeout|124' docker 124 "$tmp/empty.log"
check 1 'FAIL-PRODUCT|contained-product-failure|1' docker 1 "$tmp/empty.log"
check 2 'FAIL-MISSING-TOOL|contained-missing-tool|2' docker 2 "$tmp/empty.log"
check 3 'FAIL-TOOL|contained-tool-failure|3' docker 3 "$tmp/empty.log"
check 4 'FAIL-TIMEOUT|contained-timeout|4' docker 4 "$tmp/empty.log"
check 0 'PASS|phase-completed|0' dependency-check 0 "$tmp/empty.log"
check 3 'FAIL-TOOL|profile-image-invalid|9' dependency-check 9 "$tmp/empty.log"
check 1 'FAIL-PRODUCT|application-compile-failure|2' strict-app-build 2 "$tmp/empty.log"
check 1 'FAIL-PRODUCT|test-compile-failure|2' strict-test-build 2 "$tmp/empty.log"
check 3 'FAIL-TOOL|compiler-crash|2' strict-app-build 2 "$tmp/ice.log"
check 4 'FAIL-TIMEOUT|build-timeout|143' strict-test-build 143 "$tmp/empty.log"
check 1 'FAIL-PRODUCT|test-runner-failure|7' complete-test-run 7 "$tmp/empty.log"
check 1 'FAIL-PRODUCT|standards-core-failure|1' standards-core 1 "$tmp/empty.log"
check 4 'FAIL-TIMEOUT|execution-timeout|124' complete-test-run 124 "$tmp/empty.log"
check 2 'FAIL-MISSING-TOOL|compiler-not-found|127' missing-compiler 127 "$tmp/empty.log"
check 2 'FAIL-MISSING-TOOL|make-not-found|127' missing-make 127 "$tmp/empty.log"
check 3 'FAIL-TOOL|classifier-invalid-phase|9' invalid 9 "$tmp/empty.log"
echo "PASS: platform profile classifier tests passed"