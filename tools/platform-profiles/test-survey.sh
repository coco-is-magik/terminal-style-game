#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
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
PROFILE_PROVIDER=docker
EOF
cat >"$tmp/profiles/third.env" <<'EOF'
PROFILE_ID=third
PROFILE_ROLE=informational
PROFILE_PROVIDER=libvirt-windows
EOF

cat >"$tmp/prepare-profile.sh" <<'EOF'
#!/bin/sh
. "$1"
printf '%s:%s\n' "$PROFILE_ID" "${PROFILE_PROVIDER:-docker}" \
  >>"$PLATFORM_CALLS"
mkdir -p "$PLATFORM_OUTPUT/$PROFILE_ID"
if test "$PROFILE_ID" = third; then
    printf 'PREPARE_PHASE=host-preflight\nOUTCOME=FAIL-MISSING-TOOL\nREASON=kvm-unavailable\nSTATUS=2\n' \
      >"$PLATFORM_OUTPUT/$PROFILE_ID/prepare-result.env"
    exit 2
fi
printf 'OUTCOME=PASS\nREASON=fixture-prepared\nSTATUS=0\n' \
  >"$PLATFORM_OUTPUT/$PROFILE_ID/prepare-result.env"
EOF
cat >"$tmp/run-profile.sh" <<'EOF'
#!/bin/sh
. "$1"
mkdir -p "$PLATFORM_OUTPUT/$PROFILE_ID"
printf 'run:%s\n' "$PROFILE_ID" >>"$PLATFORM_CALLS"
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
chmod 0755 "$tmp/prepare-profile.sh" "$tmp/run-profile.sh"

PLATFORM_PROFILES_DIR="$tmp/profiles" \
PLATFORM_OUTPUT="$tmp/output" \
PLATFORM_CALLS="$tmp/calls" \
PLATFORM_PREPARE_PROFILE="$tmp/prepare-profile.sh" \
PLATFORM_RUN_PROVIDER="$tmp/run-profile.sh" \
PLATFORM_PROFILE_NAMES='first second third' \
  "$script_dir/survey.sh" >"$tmp/survey.log"

tab=$(printf '\t')
grep -Fx "profile${tab}role${tab}provider_prepare${tab}phase${tab}outcome${tab}reason${tab}status" \
  "$tmp/output/survey-summary.tsv" >/dev/null
grep -Fx "first${tab}required${tab}PASS${tab}strict-app-build${tab}FAIL-PRODUCT${tab}application-compile-failure${tab}2" \
  "$tmp/output/survey-summary.tsv" >/dev/null
grep -Fx "second${tab}required${tab}PASS${tab}complete${tab}PASS${tab}profile-compatible${tab}0" \
  "$tmp/output/survey-summary.tsv" >/dev/null
grep -Fx "third${tab}informational${tab}FAIL${tab}host-preflight${tab}FAIL-MISSING-TOOL${tab}kvm-unavailable${tab}2" \
  "$tmp/output/survey-summary.tsv" >/dev/null
grep -F 'PASS: platform survey completed' "$tmp/survey.log" >/dev/null
grep -Fx 'first:docker' "$tmp/calls" >/dev/null
grep -Fx 'second:docker' "$tmp/calls" >/dev/null
grep -Fx 'third:libvirt-windows' "$tmp/calls" >/dev/null
grep -Fx 'run:first' "$tmp/calls" >/dev/null
grep -Fx 'run:second' "$tmp/calls" >/dev/null
if grep -Fx 'run:third' "$tmp/calls" >/dev/null; then
    echo "FAIL: profile ran after provider preparation failure"
    exit 1
fi

windows_profile="$root/tools/platform-profiles/profiles/windows-10-x64-gcc.env"
grep -Fx 'PROFILE_ID=windows-10-x64-gcc' "$windows_profile" >/dev/null
grep -Fx 'PROFILE_ROLE=informational' "$windows_profile" >/dev/null
grep -Fx 'PROFILE_PROVIDER=libvirt-windows' "$windows_profile" >/dev/null
grep -Fx 'PROFILE_ARCHITECTURE=x86_64' "$windows_profile" >/dev/null
grep -Fx 'PROFILE_CRT=ucrt' "$windows_profile" >/dev/null
if grep -q '^PROFILE_VM_NAME=' "$windows_profile"; then
    echo "FAIL: tracked Windows profile contains a machine-local VM name"
    exit 1
fi
if grep -F 'windows-10-x64-gcc' "$script_dir/survey.sh" >/dev/null; then
    echo "FAIL: default platform survey must not launch Windows testing"
    exit 1
fi
awk '/^platform-test-windows:/{getline; print}' "$root/Makefile" |
  grep -F 'run-provider.sh tools/platform-profiles/profiles/windows-10-x64-gcc.env' >/dev/null
echo "PASS: platform survey continuation tests passed"
