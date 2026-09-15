#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
classifier="$script_dir/classify-clang-diagnostics.sh"
runner="$script_dir/run-profile.sh"
profile="$script_dir/profiles/ubuntu-24.04-clang.env"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

known="$tmp/known.log"
for path in \
  vendor/src/smc/include/smc.h \
  vendor/src/smc/src/c/smc_artifact.c \
  vendor/src/smc/src/c/smc_artifact.h \
  vendor/src/smc/src/c/smc_runtime_stub.c; do
    printf '%s:1:1: error: no newline at end of file [-Werror,-Wnewline-eof]\n' "$path"
done >"$known"

"$classifier" "$known" "$tmp/result.env" 2
grep -Fx 'DIAGNOSTIC_SCOPE=known-deferred-smc-newline-only' "$tmp/result.env" >/dev/null
grep -Fx 'ADDITIONAL_DIAGNOSTICS=false' "$tmp/result.env" >/dev/null

cp "$known" "$tmp/additional.log"
printf 'src/app.c:1:1: error: unused variable [-Werror,-Wunused-variable]\n' \
  >>"$tmp/additional.log"
status=0
"$classifier" "$tmp/additional.log" "$tmp/result.env" 2 || status=$?
test "$status" -ne 0
grep -Fx 'ADDITIONAL_DIAGNOSTICS=true' "$tmp/result.env" >/dev/null

cp "$known" "$tmp/smc-other-warning.log"
printf 'vendor/src/smc/include/smc.h:2:1: error: unused variable [-Werror,-Wunused-variable]\n' \
  >>"$tmp/smc-other-warning.log"
status=0
"$classifier" "$tmp/smc-other-warning.log" "$tmp/result.env" 2 || status=$?
test "$status" -ne 0
grep -Fx 'ADDITIONAL_DIAGNOSTICS=true' "$tmp/result.env" >/dev/null

cp "$known" "$tmp/malformed.log"
printf 'src/app.c:1:1: error: diagnostic without a Clang category\n' >>"$tmp/malformed.log"
status=0
"$classifier" "$tmp/malformed.log" "$tmp/result.env" 2 || status=$?
test "$status" -ne 0
grep -Fx 'ADDITIONAL_DIAGNOSTICS=true' "$tmp/result.env" >/dev/null

sed '$d' "$known" >"$tmp/incomplete.log"
status=0
"$classifier" "$tmp/incomplete.log" "$tmp/result.env" 2 || status=$?
test "$status" -ne 0
grep -Fx 'DIAGNOSTIC_SCOPE=additional-or-inconclusive-clang-diagnostics' \
  "$tmp/result.env" >/dev/null

status=0
"$classifier" "$known" "$tmp/result.env" 124 || status=$?
test "$status" -ne 0
grep -Fx 'DIAGNOSTIC_SCOPE=additional-or-inconclusive-clang-diagnostics' \
  "$tmp/result.env" >/dev/null
grep -Fx 'SWEEP_STATUS=124' "$tmp/result.env" >/dev/null

status=0
"$classifier" /dev/null "$tmp/result.env" 0 || status=$?
test "$status" -ne 0
grep -Fx 'ADDITIONAL_DIAGNOSTICS=true' "$tmp/result.env" >/dev/null

grep -Fx 'PROFILE_ROLE=informational' "$profile" >/dev/null
grep -F 'make -k CC="$PROFILE_COMPILER"' "$runner" >/dev/null
grep -F -- '-Wall -Wextra -Wpedantic -Werror -ferror-limit=0' "$runner" >/dev/null
if grep -F -- '-Wno-' "$runner" >/dev/null; then
    echo 'FAIL: Clang diagnostic sweep suppresses warnings'
    exit 1
fi
grep -F 'all test-build' "$runner" >/dev/null

echo 'PASS: Clang diagnostic sweep preserves errors and detects additional findings'