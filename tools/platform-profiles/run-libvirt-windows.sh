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
default_guest_runner=$root/tools/platform-profiles/windows_guest.py
guest_runner=${PLATFORM_WINDOWS_GUEST_RUNNER:-$default_guest_runner}
readiness_attempts=${PLATFORM_VM_READINESS_ATTEMPTS:-60}
shutdown_attempts=${PLATFORM_VM_SHUTDOWN_ATTEMPTS:-60}
poll_seconds=${PLATFORM_VM_POLL_SECONDS:-5}
started_by_harness=false
initial_state=unknown
cleanup_done=false
not_owned_reason=vm-not-owned-left-unchanged
mkdir -p "$profile_output"
rm -f "$result" "$primary_result" "$lifecycle_log"

log() {
    echo "$*" >>"$lifecycle_log"
}

run_guest_operation() {
    if test "${PLATFORM_WINDOWS_DEPENDENCY_MODE:-}" = bootstrap; then
        "$python_command" "$guest_runner" bootstrap "$PLATFORM_WINDOWS_DEPENDENCY_STEP"
    else
        "$guest_runner" "$profile_path"
    fi
}

virsh_call() {
    "$timeout_command" "${command_timeout}s" "$virsh_command" "$@"
}

domain_state() {
    virsh_call -c "$PROFILE_VM_URI" domstate "$PROFILE_VM_NAME" 2>/dev/null |
      sed -n '1{s/[[:space:]]*$//;p;}'
}

sleep_poll() {
    test "$poll_seconds" = 0 || sleep "$poll_seconds"
}

write_primary() {
    PRIMARY_OUTCOME=$1
    PRIMARY_PHASE=$2
    PRIMARY_REASON=$3
    PRIMARY_STATUS=$4
    {
        echo "PRIMARY_OUTCOME=$PRIMARY_OUTCOME"
        echo "PRIMARY_PHASE=$PRIMARY_PHASE"
        echo "PRIMARY_REASON=$PRIMARY_REASON"
        echo "PRIMARY_STATUS=$PRIMARY_STATUS"
    } >"$primary_result"
}

load_primary() {
    if test -r "$primary_result"; then
        PRIMARY_OUTCOME=
        PRIMARY_PHASE=
        PRIMARY_REASON=
        PRIMARY_STATUS=
        . "$primary_result"
        if test -z "$PRIMARY_OUTCOME" || test -z "$PRIMARY_PHASE" ||
          test -z "$PRIMARY_REASON" || test -z "$PRIMARY_STATUS"; then
            write_primary FAIL-TOOL guest-command guest-result-invalid 3
            return
        fi
        case "$PRIMARY_OUTCOME:$PRIMARY_STATUS" in
            PASS:0|FAIL-PRODUCT:1|FAIL-MISSING-TOOL:2|FAIL-TOOL:3|FAIL-TIMEOUT:4) ;;
            *) write_primary FAIL-TOOL guest-command guest-result-invalid 3 ;;
        esac
    else
        write_primary FAIL-TOOL guest-command guest-result-missing 3
    fi
}

wait_for_state() {
    expected=$1
    attempts=$2
    count=0
    while test "$count" -lt "$attempts"; do
        current=$(domain_state) || current=unknown
        log "state=$current"
        test "$current" = "$expected" && return 0
        count=$((count + 1))
        sleep_poll
    done
    return 1
}

cleanup_vm() {
    test "$cleanup_done" = false || return
    cleanup_done=true
    CLEANUP_OUTCOME=PASS
    CLEANUP_REASON=$not_owned_reason
    CLEANUP_STATUS=0
    if test "$started_by_harness" = true; then
        current=$(domain_state) || current=unknown
        if test "$current" = 'shut off'; then
            CLEANUP_REASON=vm-already-shut-off
        else
            log 'request=qga-powerdown'
            virsh_call -c "$PROFILE_VM_URI" qemu-agent-command \
              "$PROFILE_VM_NAME" \
              '{"execute":"guest-shutdown","arguments":{"mode":"powerdown"}}' \
              >/dev/null 2>&1 || true
            if wait_for_state 'shut off' "$shutdown_attempts"; then
                CLEANUP_REASON=vm-shut-down-by-qga
            else
                log 'request=acpi-shutdown'
                virsh_call -c "$PROFILE_VM_URI" shutdown "$PROFILE_VM_NAME" \
                  >/dev/null 2>&1 || true
                if wait_for_state 'shut off' "$shutdown_attempts"; then
                    CLEANUP_REASON=vm-shut-down-by-acpi
                else
                    CLEANUP_OUTCOME=FAIL-TOOL
                    CLEANUP_REASON=vm-shutdown-timeout
                    CLEANUP_STATUS=3
                fi
            fi
        fi
    fi
    VM_FINAL_STATE=$(domain_state) || VM_FINAL_STATE=unknown
}

write_result() {
    load_primary
    cleanup_vm
    OUTCOME=$PRIMARY_OUTCOME
    PHASE=$PRIMARY_PHASE
    REASON=$PRIMARY_REASON
    STATUS=$PRIMARY_STATUS
    if test "$CLEANUP_OUTCOME" != PASS; then
        OUTCOME=$CLEANUP_OUTCOME
        PHASE=vm-power-state-restore
        REASON=$CLEANUP_REASON
        STATUS=$CLEANUP_STATUS
    fi
    {
        echo "PROFILE_ID=$PROFILE_ID"
        echo "PROFILE_ROLE=$PROFILE_ROLE"
        echo "PRIMARY_OUTCOME=$PRIMARY_OUTCOME"
        echo "PRIMARY_PHASE=$PRIMARY_PHASE"
        echo "PRIMARY_REASON=$PRIMARY_REASON"
        echo "PRIMARY_STATUS=$PRIMARY_STATUS"
        echo "VM_INITIAL_STATE=$initial_state"
        echo "VM_STARTED_BY_HARNESS=$started_by_harness"
        echo "VM_FINAL_STATE=$VM_FINAL_STATE"
        echo "CLEANUP_OUTCOME=$CLEANUP_OUTCOME"
        echo "CLEANUP_REASON=$CLEANUP_REASON"
        echo "CLEANUP_STATUS=$CLEANUP_STATUS"
        echo "PHASE=$PHASE"
        echo "OUTCOME=$OUTCOME"
        echo "REASON=$REASON"
        echo "STATUS=$STATUS"
    } >"$result"
    echo "$OUTCOME: profile=$PROFILE_ID phase=$PHASE reason=$REASON status=$STATUS"
    return "$STATUS"
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
case "$shutdown_attempts" in
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
    running)
        not_owned_reason=vm-left-running-as-found
        ;;
    'shut off')
        if virsh_call -c "$PROFILE_VM_URI" start "$PROFILE_VM_NAME" \
          >/dev/null 2>&1; then
            started_by_harness=true
            log 'started_by_harness=true'
        else
            current=$(domain_state) || current=unknown
            if test "$current" != running; then
                write_primary FAIL-TOOL vm-start-or-connect windows-vm-start-failure 3
                write_result
                exit $?
            fi
            not_owned_reason=vm-left-running-external-start
            log 'start_race=externally-started'
        fi
        ;;
    'in shutdown')
        if ! wait_for_state 'shut off' "$shutdown_attempts"; then
            write_primary FAIL-TIMEOUT vm-state-check windows-vm-shutdown-wait-timeout 4
            write_result
            exit $?
        fi
        if virsh_call -c "$PROFILE_VM_URI" start "$PROFILE_VM_NAME" \
          >/dev/null 2>&1; then
            started_by_harness=true
        else
            current=$(domain_state) || current=unknown
            if test "$current" != running; then
                write_primary FAIL-TOOL vm-start-or-connect windows-vm-start-failure 3
                write_result
                exit $?
            fi
            not_owned_reason=vm-left-running-external-start
        fi
        ;;
    paused|pmsuspended|blocked|crashed|unknown|'')
        write_primary FAIL-TOOL vm-state-check unsupported-vm-state 3
        write_result
        exit $?
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
    sleep_poll
done
if test "$ready" != true; then
    write_primary FAIL-TIMEOUT guest-readiness windows-guest-readiness-timeout 4
    write_result
    exit $?
fi

if test -z "$guest_runner"; then
    write_primary FAIL-MISSING-TOOL guest-command windows-guest-runner-not-implemented 2
else
    if test "$guest_runner" = "$default_guest_runner"; then
        PLATFORM_PRIMARY_RESULT="$primary_result" PLATFORM_LIFECYCLE_PID=$$ \
          PLATFORM_OUTPUT="$output" PROFILE_ID="$PROFILE_ID" \
          PROFILE_ROLE="$PROFILE_ROLE" PROFILE_VM_URI="$PROFILE_VM_URI" \
          PROFILE_VM_NAME="$PROFILE_VM_NAME" \
          "$python_command" "$guest_runner" "$profile_path" || true
    else
        PLATFORM_PRIMARY_RESULT="$primary_result" PLATFORM_LIFECYCLE_PID=$$ \
          PLATFORM_OUTPUT="$output" PROFILE_ID="$PROFILE_ID" \
          PROFILE_ROLE="$PROFILE_ROLE" PROFILE_VM_URI="$PROFILE_VM_URI" \
          PROFILE_VM_NAME="$PROFILE_VM_NAME" \
          run_guest_operation || true
    fi
fi

current=$(domain_state) || current=unknown
if test "$current" != running; then
    write_primary FAIL-TOOL guest-command windows-vm-stopped-externally 3
fi

write_result
exit $?