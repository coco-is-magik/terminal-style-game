#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

cat >"$tmp/all-pass.tsv" <<'EOF'
profile	role	image_build	phase	outcome	reason	status
ubuntu-gcc	required	PASS	complete	PASS	profile-compatible	0
alpine	informational	PASS	strict-app-build	FAIL-PRODUCT	application-compile-failure	2
EOF
PLATFORM_SUMMARY="$tmp/all-pass.tsv" "$script_dir/check.sh" >/dev/null

cat >"$tmp/required-fail.tsv" <<'EOF'
profile	role	image_build	phase	outcome	reason	status
ubuntu-gcc	required	PASS	strict-test-build	FAIL-PRODUCT	test-compile-failure	2
EOF
status=0
PLATFORM_SUMMARY="$tmp/required-fail.tsv" "$script_dir/check.sh" >/dev/null || status=$?
test "$status" -eq 1

status=0
PLATFORM_SUMMARY="$tmp/missing.tsv" "$script_dir/check.sh" >/dev/null || status=$?
test "$status" -eq 3
echo "PASS: platform profile check tests passed"