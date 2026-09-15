#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
runner="$script_dir/run-libvirt-windows.sh"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/output"

cat >"$tmp/profile.env" <<'EOF'
PROFILE_ID=windows-fixture
PROFILE_ROLE=informational
PROFILE_PROVIDER=libvirt-windows
PROFILE_VM_URI=qemu:///fixture
PROFILE_VM_NAME=fixture-vm
EOF

cat >"$tmp/virsh" <<'EOF'
#!/bin/sh
if test "$1" = -c; then shift 2; fi
command=$1
shift
echo "$command $*" >>"$FAKE_VIRSH_LOG"
case "$command" in
    domstate) cat "$FAKE_VM_STATE" ;;
    start)
        test "$FAKE_SCENARIO" != start-failure || exit 1
        echo running >"$FAKE_VM_STATE"
        ;;
    qemu-agent-command)
        test "$FAKE_SCENARIO" != readiness-timeout
        ;;
    *) exit 99 ;;
esac
EOF

cat >"$tmp/guest-pass.sh" <<'EOF'
#!/bin/sh
cat >"$PLATFORM_PRIMARY_RESULT" <<'RESULT'
PRIMARY_OUTCOME=PASS
PRIMARY_PHASE=complete-test-run
PRIMARY_REASON=profile-compatible
PRIMARY_STATUS=0
RESULT
EOF
cat >"$tmp/guest-fail.sh" <<'EOF'
#!/bin/sh
cat >"$PLATFORM_PRIMARY_RESULT" <<'RESULT'
PRIMARY_OUTCOME=FAIL-PRODUCT
PRIMARY_PHASE=complete-test-run
PRIMARY_REASON=test-runner-failure
PRIMARY_STATUS=1
RESULT
exit 1
EOF
cat >"$tmp/guest-signal.sh" <<'EOF'
#!/bin/sh
kill -TERM "$PLATFORM_LIFECYCLE_PID"
EOF
chmod 0755 "$tmp/virsh" "$tmp/guest-pass.sh" "$tmp/guest-fail.sh" \
    "$tmp/guest-signal.sh" "$runner"

export PLATFORM_OUTPUT="$tmp/output"
export PLATFORM_VIRSH="$tmp/virsh"
export PLATFORM_TIMEOUT=timeout
export PLATFORM_PYTHON=sh
export PLATFORM_VM_COMMAND_TIMEOUT=2
export PLATFORM_VM_READINESS_ATTEMPTS=2
export PLATFORM_VM_POLL_SECONDS=0
export FAKE_VIRSH_LOG="$tmp/virsh.log"
export FAKE_VM_STATE="$tmp/state"
result="$tmp/output/windows-fixture/result.env"

assert_field() {
    grep -Fx "$2=$3" "$1" >/dev/null
}

assert_no_power_off() {
    if grep -E '^(shutdown|destroy|reset|reboot|suspend|snapshot|undefine) |guest-shutdown' \
      "$FAKE_VIRSH_LOG" >/dev/null; then
        echo "FAIL: Windows test runner attempted to stop or alter the VM"
        exit 1
    fi
}

run_case() {
    scenario=$1
    state=$2
    guest=$3
    expected=$4
    export FAKE_SCENARIO=$scenario
    echo "$state" >"$FAKE_VM_STATE"
    : >"$FAKE_VIRSH_LOG"
    status=0
    PLATFORM_WINDOWS_GUEST_RUNNER="$guest" "$runner" "$tmp/profile.env" \
      >/dev/null || status=$?
    test "$status" -eq "$expected"
    assert_no_power_off
}

run_case normal running "$tmp/guest-pass.sh" 0
assert_field "$result" VM_INITIAL_STATE running
assert_field "$result" VM_STARTED_BY_HARNESS false
assert_field "$result" VM_FINAL_STATE running
if grep -E '^start ' "$FAKE_VIRSH_LOG" >/dev/null; then
    echo "FAIL: already-running VM was started again"
    exit 1
fi

run_case normal 'shut off' "$tmp/guest-pass.sh" 0
assert_field "$result" VM_STARTED_BY_HARNESS true
assert_field "$result" VM_FINAL_STATE running
test "$(grep -c '^start fixture-vm$' "$FAKE_VIRSH_LOG")" -eq 1

run_case normal running "$tmp/guest-fail.sh" 1
assert_field "$result" OUTCOME FAIL-PRODUCT
assert_field "$result" VM_FINAL_STATE running

run_case readiness-timeout 'shut off' "$tmp/guest-pass.sh" 4
assert_field "$result" REASON windows-guest-readiness-timeout
assert_field "$result" VM_FINAL_STATE running

run_case normal running "$tmp/guest-signal.sh" 3
assert_field "$result" REASON interrupted
assert_field "$result" VM_FINAL_STATE running

if grep -E 'guest-shutdown|[[:space:]](shutdown|destroy|reset|reboot|suspend|snapshot|undefine)[[:space:]]' \
  "$runner" >/dev/null; then
    echo "FAIL: lifecycle source contains a forbidden VM power operation"
    exit 1
fi

echo "PASS: Windows lifecycle starts when needed and never stops the VM"