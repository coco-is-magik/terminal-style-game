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
    output=$(classify_result "$@") || status=$?
    test "$status" -eq "$expected_status"
    test "$output" = "$expected_text"
}

: >"$tmp/empty.log"
printf '%s\n' 'valgrind: Unrecognised instruction at address 0x1' >"$tmp/sigill.log"
check 0 'PASS: reason=container-completed' docker 0 "$tmp/empty.log"
check 4 'FAIL-TIMEOUT: reason=container-timeout status=124' docker 124 "$tmp/empty.log"
check 3 'FAIL-TOOL: reason=docker-daemon-or-runtime status=125' docker 125 "$tmp/empty.log"
check 3 'FAIL-TOOL: reason=container-command-not-executable status=126' docker 126 "$tmp/empty.log"
check 3 'FAIL-TOOL: reason=container-command-not-found status=127' docker 127 "$tmp/empty.log"
check 1 'FAIL-PRODUCT: reason=contained-product-failure status=1' docker 1 "$tmp/empty.log"
check 2 'FAIL-MISSING-TOOL: reason=contained-missing-tool status=2' docker 2 "$tmp/empty.log"
check 3 'FAIL-TOOL: reason=contained-tool-failure status=3' docker 3 "$tmp/empty.log"
check 4 'FAIL-TIMEOUT: reason=contained-timeout status=4' docker 4 "$tmp/empty.log"
check 1 'FAIL-PRODUCT: reason=contained-runner-status status=9' docker 9 "$tmp/empty.log"
check 0 'PASS: reason=direct-runner-passed' direct 0 "$tmp/empty.log"
check 1 'FAIL-PRODUCT: reason=direct-runner-failure status=9' direct 9 "$tmp/empty.log"
check 0 'PASS: reason=memcheck-clean' memcheck 0 "$tmp/empty.log"
check 1 'FAIL-PRODUCT: reason=memcheck-findings status=100' memcheck 100 "$tmp/empty.log"
check 4 'FAIL-TIMEOUT: reason=memcheck-timeout status=143' memcheck 143 "$tmp/empty.log"
check 3 'FAIL-TOOL: reason=unsupported-client-instruction status=132' memcheck 132 "$tmp/sigill.log"
check 3 'FAIL-TOOL: reason=valgrind-crash status=9' memcheck 9 "$tmp/empty.log"
check 2 'FAIL-MISSING-TOOL: reason=valgrind-not-found' missing 127 "$tmp/empty.log"
check 3 'FAIL-TOOL: reason=classifier-invalid-phase phase=invalid' invalid 0 "$tmp/empty.log"
echo "PASS: Valgrind diagnostic classifier tests passed"