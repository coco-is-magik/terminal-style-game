#!/bin/sh
set -eu

tmp=/tmp/valgrind-image-self-test
rm -rf "$tmp"
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat >"$tmp/probe.c" <<'EOF'
#include <stdio.h>

int main(void) {
    if (puts("valgrind-image-self-test") == EOF) {
        return 1;
    }
    return 0;
}
EOF
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror "$tmp/probe.c" -o "$tmp/probe"
"$tmp/probe"
valgrind --error-exitcode=100 --leak-check=full "$tmp/probe" >"$tmp/valgrind.log" 2>&1
cat "$tmp/valgrind.log"
grep -F 'ERROR SUMMARY: 0 errors' "$tmp/valgrind.log" >/dev/null
echo "PASS: Valgrind image self-test passed"