#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/output"

cat >"$tmp/default.env" <<'EOF'
PROFILE_ID=default-docker
PROFILE_ROLE=required
EOF
cat >"$tmp/docker.env" <<'EOF'
PROFILE_ID=explicit-docker
PROFILE_ROLE=required
PROFILE_PROVIDER=docker
EOF
cat >"$tmp/windows.env" <<'EOF'
PROFILE_ID=windows
PROFILE_ROLE=informational
PROFILE_PROVIDER=libvirt-windows
EOF
cat >"$tmp/invalid.env" <<'EOF'
PROFILE_ID=invalid
PROFILE_ROLE=informational
PROFILE_PROVIDER=invalid-provider
EOF

cat >"$tmp/prepare.sh" <<'EOF'
#!/bin/sh
. "$1"
mkdir -p "$PLATFORM_OUTPUT/$PROFILE_ID"
printf '%s\n' "$PROFILE_ID" >>"$PLATFORM_CALLS"
printf 'OUTCOME=PASS\nREASON=fixture-prepared\nSTATUS=0\n' \
  >"$PLATFORM_OUTPUT/$PROFILE_ID/image-result.env"
EOF
cat >"$tmp/run.sh" <<'EOF'
#!/bin/sh
. "$1"
printf '%s\n' "$PROFILE_ID" >>"$PLATFORM_CALLS"
EOF
cat >"$tmp/fail-without-result.sh" <<'EOF'
#!/bin/sh
exit 1
EOF
cat >"$tmp/virsh" <<'EOF'
#!/bin/sh
exit 0
EOF
chmod 0755 "$tmp/prepare.sh" "$tmp/run.sh" \
  "$tmp/fail-without-result.sh" "$tmp/virsh"

export PLATFORM_OUTPUT="$tmp/output"
export PLATFORM_CALLS="$tmp/calls"
export PLATFORM_DOCKER_PREPARE="$tmp/prepare.sh"
export PLATFORM_DOCKER_RUN="$tmp/run.sh"

"$script_dir/prepare-profile.sh" "$tmp/default.env"
grep -Fx 'default-docker' "$tmp/calls" >/dev/null
grep -Fx 'REASON=fixture-prepared' \
  "$tmp/output/default-docker/prepare-result.env" >/dev/null
grep -Fx 'PREPARE_PHASE=image-build' \
  "$tmp/output/default-docker/prepare-result.env" >/dev/null

printf 'OUTCOME=PASS\nREASON=stale-result\nSTATUS=0\n' \
  >"$tmp/output/default-docker/image-result.env"
status=0
PLATFORM_DOCKER_PREPARE="$tmp/fail-without-result.sh" \
  "$script_dir/prepare-profile.sh" "$tmp/default.env" || status=$?
test "$status" -eq 1
test ! -e "$tmp/output/default-docker/image-result.env"
test ! -e "$tmp/output/default-docker/prepare-result.env"

"$script_dir/prepare-profile.sh" "$tmp/docker.env"
"$script_dir/run-provider.sh" "$tmp/docker.env"
test "$(grep -Fxc 'explicit-docker' "$tmp/calls")" -eq 2

PLATFORM_LIBVIRT_WINDOWS_RUN="$tmp/run.sh" \
  "$script_dir/run-provider.sh" "$tmp/windows.env"
grep -Fx 'windows' "$tmp/calls" >/dev/null

status=0
"$script_dir/prepare-profile.sh" "$tmp/windows.env" >/dev/null || status=$?
test "$status" -eq 3
grep -Fx 'REASON=unsupported-profile-provider' \
  "$tmp/output/windows/prepare-result.env" >/dev/null

status=0
output=$(PROFILE_VM_NAME= "$script_dir/run-provider.sh" "$tmp/windows.env") || status=$?
test "$status" -eq 2
test "$output" = 'FAIL-MISSING-TOOL: profile=windows phase=vm-locate reason=windows-vm-not-configured status=2 vm=unknown'

status=0
"$script_dir/prepare-profile.sh" "$tmp/invalid.env" || status=$?
test "$status" -eq 3
grep -Fx 'REASON=unsupported-profile-provider' \
  "$tmp/output/invalid/prepare-result.env" >/dev/null

status=0
output=$("$script_dir/run-provider.sh" "$tmp/invalid.env") || status=$?
test "$status" -eq 3
test "$output" = \
  'FAIL-TOOL: profile=invalid reason=unsupported-profile-provider'

echo "PASS: platform provider dispatch tests passed"