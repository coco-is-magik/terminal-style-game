#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
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
if test "$1" = -c; then
    shift 2
fi
command=$1
shift
echo "$command $*" >>"$FAKE_VIRSH_LOG"
case "$command" in
    uri)
        if test "$FAKE_SCENARIO" = libvirt-unavailable; then
            exit 1
        fi
        echo qemu:///fixture
        ;;
    dominfo)
        test "$FAKE_SCENARIO" != missing-domain
        ;;
    domcapabilities)
        test "$FAKE_SCENARIO" != missing-kvm-capability
        ;;
    dumpxml)
        if test "$FAKE_SCENARIO" = inspect-failure; then
            exit 1
        fi
        domain_type=kvm
        test "$FAKE_SCENARIO" = wrong-domain-type && domain_type=qemu
        echo "<domain type='$domain_type'><devices>"
        test "$FAKE_SCENARIO" = missing-qga-channel || \
          echo "<channel><target name='org.qemu.guest_agent.0'/></channel>"
        echo '</devices></domain>'
        ;;
    domstate)
        cat "$FAKE_VM_STATE"
        ;;
    start)
        if test "$FAKE_SCENARIO" = start-race; then
            echo running >"$FAKE_VM_STATE"
            exit 1
        fi
        test "$FAKE_SCENARIO" != start-failure || exit 1
        echo running >"$FAKE_VM_STATE"
        ;;
    qemu-agent-command)
        payload=$2
        case "$payload" in
            *guest-ping*)
                test "$FAKE_SCENARIO" != readiness-timeout
                ;;
            *guest-shutdown*)
                case "$FAKE_SCENARIO" in
                    cleanup-acpi|cleanup-timeout) exit 1 ;;
                    *) echo 'shut off' >"$FAKE_VM_STATE"; exit 1 ;;
                esac
                ;;
        esac
        ;;
    shutdown)
        test "$FAKE_SCENARIO" = cleanup-timeout || \
          echo 'shut off' >"$FAKE_VM_STATE"
        ;;
    *)
        echo "unexpected fake virsh command: $command" >&2
        exit 99
        ;;
esac
EOF

cat >"$tmp/guest-pass.sh" <<'EOF'
#!/bin/sh
cat >"$PLATFORM_PRIMARY_RESULT" <<'RESULT'
PRIMARY_OUTCOME=PASS
PRIMARY_PHASE=complete
PRIMARY_REASON=profile-compatible
PRIMARY_STATUS=0
RESULT
EOF
cat >"$tmp/guest-product-fail.sh" <<'EOF'
#!/bin/sh
cat >"$PLATFORM_PRIMARY_RESULT" <<'RESULT'
PRIMARY_OUTCOME=FAIL-PRODUCT
PRIMARY_PHASE=strict-app-build
PRIMARY_REASON=application-compile-failure
PRIMARY_STATUS=1
RESULT
exit 1
EOF
cat >"$tmp/guest-malformed.sh" <<'EOF'
#!/bin/sh
cat >"$PLATFORM_PRIMARY_RESULT" <<'RESULT'
PRIMARY_OUTCOME=PASS
PRIMARY_PHASE=complete
PRIMARY_REASON=invalid-status-fixture
PRIMARY_STATUS=not-a-number
RESULT
EOF
cat >"$tmp/guest-stop.sh" <<'EOF'
#!/bin/sh
echo 'shut off' >"$FAKE_VM_STATE"
EOF
cat >"$tmp/guest-signal.sh" <<'EOF'
#!/bin/sh
kill -TERM "$PLATFORM_LIFECYCLE_PID"
EOF
chmod 0755 "$tmp/virsh" "$tmp/guest-pass.sh" \
  "$tmp/guest-product-fail.sh" "$tmp/guest-malformed.sh" \
  "$tmp/guest-stop.sh" "$tmp/guest-signal.sh"

export PLATFORM_OUTPUT="$tmp/output"
export PLATFORM_VIRSH="$tmp/virsh"
export PLATFORM_KVM_DEVICE=/dev/null
export PLATFORM_TIMEOUT=timeout
export PLATFORM_VM_COMMAND_TIMEOUT=2
export PLATFORM_VM_READINESS_ATTEMPTS=2
export PLATFORM_VM_SHUTDOWN_ATTEMPTS=2
export PLATFORM_VM_POLL_SECONDS=0
export FAKE_VIRSH_LOG="$tmp/virsh.log"
export FAKE_VM_STATE="$tmp/state"

result="$tmp/output/windows-fixture/result.env"
prepare_result="$tmp/output/windows-fixture/prepare-result.env"

assert_field() {
    file=$1
    field=$2
    expected=$3
    grep -Fx "$field=$expected" "$file" >/dev/null
}

assert_no_mutation() {
    if grep -E '^(start|shutdown) |guest-shutdown' "$FAKE_VIRSH_LOG" >/dev/null; then
        echo "FAIL: unexpected VM mutation"
        cat "$FAKE_VIRSH_LOG"
        exit 1
    fi
}

assert_no_forced_operation() {
    if grep -E '^(destroy|reset|reboot|resume|suspend|snapshot)' \
      "$FAKE_VIRSH_LOG" >/dev/null; then
        echo "FAIL: forbidden VM operation"
        cat "$FAKE_VIRSH_LOG"
        exit 1
    fi
}

reset_case() {
    scenario=$1
    state=$2
    FAKE_SCENARIO=$scenario
    export FAKE_SCENARIO
    echo "$state" >"$FAKE_VM_STATE"
    : >"$FAKE_VIRSH_LOG"
    rm -f "$result" "$prepare_result"
}

run_lifecycle() {
    expected_status=$1
    runner=$2
    status=0
    PLATFORM_WINDOWS_GUEST_RUNNER="$runner" \
      "$script_dir/run-libvirt-windows.sh" "$tmp/profile.env" >/dev/null || status=$?
    test "$status" -eq "$expected_status"
    assert_no_forced_operation
}

reset_case normal 'shut off'
status=0
PLATFORM_VM_READINESS_ATTEMPTS=0 PLATFORM_WINDOWS_GUEST_RUNNER="$tmp/guest-pass.sh" \
  "$script_dir/run-libvirt-windows.sh" "$tmp/profile.env" >/dev/null || status=$?
test "$status" -eq 3
assert_field "$result" PRIMARY_REASON invalid-lifecycle-timeout
assert_no_mutation

reset_case normal 'shut off'
"$script_dir/prepare-libvirt-windows.sh" "$tmp/profile.env" >/dev/null
assert_field "$prepare_result" OUTCOME PASS

reset_case normal 'shut off'
status=0
PLATFORM_KVM_DEVICE="$tmp/missing-kvm" \
  "$script_dir/prepare-libvirt-windows.sh" "$tmp/profile.env" >/dev/null || status=$?
test "$status" -eq 2
assert_field "$prepare_result" REASON kvm-unavailable
test ! -s "$FAKE_VIRSH_LOG"

reset_case normal 'shut off'
status=0
PLATFORM_PYTHON=missing-python \
  "$script_dir/prepare-libvirt-windows.sh" "$tmp/profile.env" \
  >/dev/null || status=$?
test "$status" -eq 2
assert_field "$prepare_result" REASON python3-not-found
test ! -s "$FAKE_VIRSH_LOG"

reset_case libvirt-unavailable 'shut off'
status=0
"$script_dir/prepare-libvirt-windows.sh" "$tmp/profile.env" >/dev/null || status=$?
test "$status" -eq 3
assert_field "$prepare_result" REASON libvirt-unavailable
assert_no_mutation

reset_case missing-domain 'shut off'
status=0
"$script_dir/prepare-libvirt-windows.sh" "$tmp/profile.env" >/dev/null || status=$?
test "$status" -eq 2
assert_field "$prepare_result" REASON windows-vm-not-configured
assert_no_mutation

reset_case missing-kvm-capability 'shut off'
status=0
"$script_dir/prepare-libvirt-windows.sh" "$tmp/profile.env" >/dev/null || status=$?
test "$status" -eq 2
assert_field "$prepare_result" REASON kvm-unavailable
assert_no_mutation

reset_case wrong-domain-type 'shut off'
status=0
"$script_dir/prepare-libvirt-windows.sh" "$tmp/profile.env" >/dev/null || status=$?
test "$status" -eq 3
assert_field "$prepare_result" REASON windows-vm-not-kvm
assert_no_mutation

reset_case inspect-failure 'shut off'
status=0
"$script_dir/prepare-libvirt-windows.sh" "$tmp/profile.env" >/dev/null || status=$?
test "$status" -eq 3
assert_field "$prepare_result" REASON windows-vm-inspection-failure
assert_no_mutation

reset_case missing-qga-channel 'shut off'
status=0
"$script_dir/prepare-libvirt-windows.sh" "$tmp/profile.env" >/dev/null || status=$?
test "$status" -eq 2
assert_field "$prepare_result" REASON qemu-guest-agent-channel-not-configured
assert_no_mutation

reset_case normal running
run_lifecycle 0 "$tmp/guest-pass.sh"
assert_field "$result" VM_INITIAL_STATE running
assert_field "$result" VM_STARTED_BY_HARNESS false
assert_field "$result" VM_FINAL_STATE running
assert_field "$result" CLEANUP_REASON vm-left-running-as-found
assert_no_mutation

reset_case normal 'shut off'
run_lifecycle 0 "$tmp/guest-pass.sh"
assert_field "$result" VM_STARTED_BY_HARNESS true
assert_field "$result" VM_FINAL_STATE 'shut off'
assert_field "$result" CLEANUP_REASON vm-shut-down-by-qga

reset_case normal 'shut off'
run_lifecycle 1 "$tmp/guest-product-fail.sh"
assert_field "$result" PRIMARY_OUTCOME FAIL-PRODUCT
assert_field "$result" OUTCOME FAIL-PRODUCT
assert_field "$result" CLEANUP_OUTCOME PASS

reset_case readiness-timeout 'shut off'
run_lifecycle 4 "$tmp/guest-pass.sh"
assert_field "$result" PRIMARY_REASON windows-guest-readiness-timeout
assert_field "$result" VM_FINAL_STATE 'shut off'

for unsupported_state in paused pmsuspended blocked crashed unknown; do
    reset_case normal "$unsupported_state"
    run_lifecycle 3 "$tmp/guest-pass.sh"
    assert_field "$result" PRIMARY_REASON unsupported-vm-state
    assert_no_mutation
done

reset_case start-failure 'shut off'
run_lifecycle 3 "$tmp/guest-pass.sh"
assert_field "$result" VM_STARTED_BY_HARNESS false
assert_field "$result" PRIMARY_REASON windows-vm-start-failure
if grep -F 'guest-shutdown' "$FAKE_VIRSH_LOG" >/dev/null; then
    echo "FAIL: start failure established cleanup ownership"
    exit 1
fi

reset_case start-race 'shut off'
run_lifecycle 0 "$tmp/guest-pass.sh"
assert_field "$result" VM_INITIAL_STATE 'shut off'
assert_field "$result" VM_STARTED_BY_HARNESS false
assert_field "$result" VM_FINAL_STATE running
assert_field "$result" CLEANUP_REASON vm-left-running-external-start

reset_case cleanup-acpi 'shut off'
run_lifecycle 0 "$tmp/guest-pass.sh"
assert_field "$result" CLEANUP_REASON vm-shut-down-by-acpi
grep -E '^shutdown fixture-vm$' "$FAKE_VIRSH_LOG" >/dev/null

reset_case cleanup-timeout 'shut off'
run_lifecycle 3 "$tmp/guest-product-fail.sh"
assert_field "$result" PRIMARY_OUTCOME FAIL-PRODUCT
assert_field "$result" CLEANUP_OUTCOME FAIL-TOOL
assert_field "$result" CLEANUP_REASON vm-shutdown-timeout
assert_field "$result" OUTCOME FAIL-TOOL
assert_field "$result" VM_FINAL_STATE running

reset_case normal running
run_lifecycle 3 "$tmp/guest-stop.sh"
assert_field "$result" PRIMARY_REASON windows-vm-stopped-externally
if grep -E '^start ' "$FAKE_VIRSH_LOG" >/dev/null; then
    echo "FAIL: externally stopped VM was restarted"
    exit 1
fi

reset_case normal 'shut off'
run_lifecycle 3 "$tmp/guest-malformed.sh"
assert_field "$result" PRIMARY_REASON guest-result-invalid
assert_field "$result" VM_FINAL_STATE 'shut off'

reset_case normal 'shut off'
run_lifecycle 3 "$tmp/guest-signal.sh"
assert_field "$result" PRIMARY_REASON interrupted
assert_field "$result" VM_FINAL_STATE 'shut off'

echo "PASS: libvirt lifecycle tests passed"