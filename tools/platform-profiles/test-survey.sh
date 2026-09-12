#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/profiles" "$tmp/output"

cat >"$tmp/profiles/first.env" <<'EOF'
PROFILE_ID=first
PROFILE_ROLE=required
EOF
cat >"$tmp/profiles/second.env" <<'EOF'
PROFILE_ID=second
PROFILE_ROLE=required
EOF
cat >"$tmp/profiles/third.env" <<'EOF'
PROFILE_ID=third
PROFILE_ROLE=informational
EOF

cat >"$tmp/build-image.sh" <<'EOF'
#!/bin/sh
. "$1"
if test "$PROFILE_ID" = third; then
    mkdir -p "$PLATFORM_OUTPUT/$PROFILE_ID"
    printf 'OUTCOME=FAIL-PRODUCT\nREASON=dependency-build-failure\nSTATUS=1\n' \
      >"$PLATFORM_OUTPUT/$PROFILE_ID/image-result.env"
    exit 1
fi
EOF
cat >"$tmp/run-one.sh" <<'EOF'
#!/bin/sh
. "$1"
mkdir -p "$PLATFORM_OUTPUT/$PROFILE_ID"
if test "$PROFILE_ID" = first; then
    outcome=FAIL-PRODUCT
    phase=strict-app-build
    reason=application-compile-failure
    status=2
else
    outcome=PASS
    phase=complete
    reason=profile-compatible
    status=0
fi
cat >"$PLATFORM_OUTPUT/$PROFILE_ID/result.env" <<RESULT
PROFILE_ID=$PROFILE_ID
PROFILE_ROLE=$PROFILE_ROLE
PHASE=$phase
OUTCOME=$outcome
REASON=$reason
STATUS=$status
RESULT
test "$status" -eq 0
EOF
chmod 0755 "$tmp/build-image.sh" "$tmp/run-one.sh"

PLATFORM_PROFILES_DIR="$tmp/profiles" \
PLATFORM_OUTPUT="$tmp/output" \
PLATFORM_BUILD_IMAGE="$tmp/build-image.sh" \
PLATFORM_RUN_ONE="$tmp/run-one.sh" \
PLATFORM_PROFILE_NAMES='first second third' \
  "$script_dir/survey.sh" >"$tmp/survey.log"

grep -F 'first' "$tmp/output/survey-summary.tsv" >/dev/null
grep -F 'second' "$tmp/output/survey-summary.tsv" >/dev/null
grep -F 'third' "$tmp/output/survey-summary.tsv" >/dev/null
grep -F 'PASS: platform survey completed' "$tmp/survey.log" >/dev/null
grep -F 'first' "$tmp/output/survey-summary.tsv" | grep -F 'FAIL-PRODUCT' >/dev/null
grep -F 'third' "$tmp/output/survey-summary.tsv" | grep -F 'dependency-build-failure' >/dev/null
echo "PASS: platform survey continuation tests passed"