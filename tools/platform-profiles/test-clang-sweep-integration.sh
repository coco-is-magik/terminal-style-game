#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
runner="$script_dir/run-profile.sh"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

extract_branch() {
    sed -n '/^app_log=\/output\/strict-app-build.log$/,/^fi$/p' "$runner" |
      sed 's|/output/|$OUTPUT/|g'
}

run_fixture() {
    compiler=$1
    OUTPUT="$tmp/$compiler"
    export OUTPUT PROFILE_COMPILER=$compiler
    mkdir -p "$OUTPUT"
    calls="$OUTPUT/calls"
    branch="$OUTPUT/branch.sh"
    extract_branch >"$branch"
    cat >"$OUTPUT/functions.sh" <<'EOF'
timeout() {
    shift
    printf '%s\n' "$*" >>"$calls"
    return 2
}
run_clang_diagnostic_sweep() {
    echo sweep >>"$calls"
}
finish_failure() {
    printf 'finish:%s:%s:%s\n' "$1" "$2" "$3" >>"$calls"
}
EOF
    # shellcheck disable=SC1090
    . "$OUTPUT/functions.sh"
    # shellcheck disable=SC1090
    . "$branch"
}

run_fixture clang
grep -Fx 'sweep' "$tmp/clang/calls" >/dev/null
grep -F 'finish:strict-app-build:2:' "$tmp/clang/calls" >/dev/null

run_fixture gcc
if grep -Fx 'sweep' "$tmp/gcc/calls" >/dev/null; then
    echo 'FAIL: GCC failure invoked the Clang diagnostic sweep'
    exit 1
fi
grep -F 'finish:strict-app-build:2:' "$tmp/gcc/calls" >/dev/null

echo 'PASS: failed Clang builds sweep diagnostics without replacing the strict result'