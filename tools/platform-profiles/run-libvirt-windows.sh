#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
profile_path=$1
PROFILE_VM_URI=${PROFILE_VM_URI:-qemu:///session}
PROFILE_VM_NAME=${PROFILE_VM_NAME:-}
. "$profile_path"
output=${PLATFORM_OUTPUT:-$root/build/platform-profiles}
profile_output="$output/$PROFILE_ID"
result="$profile_output/result.env"
primary_result="$profile_output/primary-result.env"
lifecycle_log="$profile_output/vm-lifecycle.log"
virsh_command=${PLATFORM_VIRSH:-virsh}
timeout_command=${PLATFORM_TIMEOUT:-timeout}
python_command=${PLATFORM_PYTHON:-python3}
command_timeout=${PLATFORM_VM_COMMAND_TIMEOUT:-15}
guest_runner=${PLATFORM_WINDOWS_GUEST_RUNNER:-$root/tools/platform-profiles/windows_guest.py}
readiness_attempts=${PLATFORM_VM_READINESS_ATTEMPTS:-60}
poll_seconds=${PLATFORM_VM_POLL_SECONDS:-5}
initial_state=unknown
started_by_harness=false
mkdir -p "$profile_output"
rm -f "$result" "$primary_result" "$lifecycle_log"

log() {
    echo "$*" >>"$lifecycle_log"
}

virsh_call() {
    "$timeout_command" "${command_timeout}s" "$virsh_command" "$@"
}

domain_state() {
    virsh_call -c "$PROFILE_VM_URI" domstate "$PROFILE_VM_NAME" 2>/dev/null |
      sed -n '1{s/[[:space:]]*$//;p;}'
}

write_primary() {
    {
        echo "PRIMARY_OUTCOME=$1"
        echo "PRIMARY_PHASE=$2"
        echo "PRIMARY_REASON=$3"
        echo "PRIMARY_STATUS=$4"
    } >"$primary_result"
}

load_primary() {
    if test -r "$primary_result"; then
        PRIMARY_OUTCOME=
        PRIMARY_PHASE=
        PRIMARY_REASON=
        PRIMARY_STATUS=
        . "$primary_result"
        case "$PRIMARY_OUTCOME:$PRIMARY_STATUS" in
            PASS:0|FAIL-PRODUCT:1|FAIL-MISSING-TOOL:2|FAIL-TOOL:3|FAIL-TIMEOUT:4) ;;
            *) write_primary FAIL-TOOL guest-command guest-result-invalid 3; . "$primary_result" ;;
        esac
    else
        write_primary FAIL-TOOL guest-command guest-result-missing 3
        . "$primary_result"
    fi
}

write_result() {
    load_primary
    final_state=$(domain_state) || final_state=unknown
    test -n "$final_state" || final_state=unknown
    {
        echo "PROFILE_ID=$PROFILE_ID"
        echo "PROFILE_ROLE=$PROFILE_ROLE"
        echo "PRIMARY_OUTCOME=$PRIMARY_OUTCOME"
        echo "PRIMARY_PHASE=$PRIMARY_PHASE"
        echo "PRIMARY_REASON=$PRIMARY_REASON"
        echo "PRIMARY_STATUS=$PRIMARY_STATUS"
        echo "VM_INITIAL_STATE=$initial_state"
        echo "VM_STARTED_BY_HARNESS=$started_by_harness"
        echo "VM_FINAL_STATE=$final_state"
        echo "PHASE=$PRIMARY_PHASE"
        echo "OUTCOME=$PRIMARY_OUTCOME"
        echo "REASON=$PRIMARY_REASON"
        echo "STATUS=$PRIMARY_STATUS"
    } >"$result"
    echo "$PRIMARY_OUTCOME: profile=$PROFILE_ID phase=$PRIMARY_PHASE reason=$PRIMARY_REASON status=$PRIMARY_STATUS vm=$final_state"
    return "$PRIMARY_STATUS"
}

handle_signal() {
    write_primary FAIL-TOOL vm-lifecycle interrupted 3
    trap - HUP INT TERM
    write_result
    exit 3
}
trap handle_signal HUP INT TERM

case "$readiness_attempts" in
    ''|*[!0-9]*|0)
        write_primary FAIL-TOOL host-preflight invalid-lifecycle-timeout 3
        write_result
        exit $?
        ;;
esac
case "$poll_seconds" in
    ''|*[!0-9]*)
        write_primary FAIL-TOOL host-preflight invalid-lifecycle-timeout 3
        write_result
        exit $?
        ;;
esac
case "$command_timeout" in
    ''|*[!0-9]*|0)
        write_primary FAIL-TOOL host-preflight invalid-lifecycle-timeout 3
        write_result
        exit $?
        ;;
esac
if test -z "$PROFILE_VM_NAME"; then
    write_primary FAIL-MISSING-TOOL vm-locate windows-vm-not-configured 2
    write_result
    exit $?
fi

initial_state=$(domain_state) || initial_state=unknown
log "initial_state=$initial_state"
case "$initial_state" in
    running) ;;
    'shut off')
        if virsh_call -c "$PROFILE_VM_URI" start "$PROFILE_VM_NAME" >/dev/null 2>&1; then
            started_by_harness=true
            log 'started_by_harness=true'
        else
            current=$(domain_state) || current=unknown
            if test "$current" != running; then
                write_primary FAIL-TOOL vm-start-or-connect windows-vm-start-failure 3
                write_result
                exit $?
            fi
        fi
        ;;
    *)
        write_primary FAIL-TOOL vm-state-check unsupported-vm-state 3
        write_result
        exit $?
        ;;
esac

ready=false
attempt=0
while test "$attempt" -lt "$readiness_attempts"; do
    current=$(domain_state) || current=unknown
    if test "$current" != running; then
        write_primary FAIL-TOOL guest-readiness windows-vm-stopped-externally 3
        write_result
        exit $?
    fi
    if virsh_call -c "$PROFILE_VM_URI" qemu-agent-command \
      "$PROFILE_VM_NAME" '{"execute":"guest-ping"}' >/dev/null 2>&1; then
        ready=true
        break
    fi
    attempt=$((attempt + 1))
    test "$poll_seconds" = 0 || sleep "$poll_seconds"
done
if test "$ready" != true; then
    write_primary FAIL-TIMEOUT guest-readiness windows-guest-readiness-timeout 4
    write_result
    exit $?
fi

PLATFORM_PRIMARY_RESULT="$primary_result" PLATFORM_LIFECYCLE_PID=$$ \
  PLATFORM_OUTPUT="$output" PROFILE_ID="$PROFILE_ID" PROFILE_ROLE="$PROFILE_ROLE" \
  PROFILE_VM_URI="$PROFILE_VM_URI" PROFILE_VM_NAME="$PROFILE_VM_NAME" \
  "$python_command" "$guest_runner" "$profile_path" || true

current=$(domain_state) || current=unknown
if test "$current" != running; then
    write_primary FAIL-TOOL guest-command windows-vm-stopped-externally 3
fi
write_result
exit $?