#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
profile_path=$1
PROFILE_VM_URI=${PROFILE_VM_URI:-qemu:///session}
PROFILE_VM_NAME=${PROFILE_VM_NAME:-}
. "$profile_path"
output=${PLATFORM_OUTPUT:-$root/build/platform-profiles}
result="$output/$PROFILE_ID/prepare-result.env"
virsh_command=${PLATFORM_VIRSH:-virsh}
timeout_command=${PLATFORM_TIMEOUT:-timeout}
python_command=${PLATFORM_PYTHON:-python3}
command_timeout=${PLATFORM_VM_COMMAND_TIMEOUT:-15}
kvm_device=${PLATFORM_KVM_DEVICE:-/dev/kvm}
mkdir -p "$output/$PROFILE_ID"

write_result() {
    outcome=$1
    reason=$2
    status=$3
    {
        echo "PREPARE_PHASE=host-preflight"
        echo "OUTCOME=$outcome"
        echo "REASON=$reason"
        echo "STATUS=$status"
    } >"$result"
    echo "$outcome: profile=$PROFILE_ID phase=host-preflight reason=$reason status=$status"
}

virsh_call() {
    "$timeout_command" "${command_timeout}s" "$virsh_command" "$@"
}

if ! command -v "$virsh_command" >/dev/null 2>&1; then
    write_result FAIL-MISSING-TOOL virsh-not-found 2
    exit 2
fi
if ! command -v "$timeout_command" >/dev/null 2>&1; then
    write_result FAIL-MISSING-TOOL timeout-not-found 2
    exit 2
fi
if ! command -v "$python_command" >/dev/null 2>&1; then
    write_result FAIL-MISSING-TOOL python3-not-found 2
    exit 2
fi
case "$command_timeout" in
    ''|*[!0-9]*|0)
        write_result FAIL-TOOL invalid-lifecycle-timeout 3
        exit 3
        ;;
esac
if test ! -c "$kvm_device"; then
    write_result FAIL-MISSING-TOOL kvm-unavailable 2
    exit 2
fi
if test -z "$PROFILE_VM_NAME"; then
    write_result FAIL-MISSING-TOOL windows-vm-not-configured 2
    exit 2
fi
if ! virsh_call -c "$PROFILE_VM_URI" uri >/dev/null 2>&1; then
    write_result FAIL-TOOL libvirt-unavailable 3
    exit 3
fi
if ! virsh_call -c "$PROFILE_VM_URI" dominfo "$PROFILE_VM_NAME" \
  >/dev/null 2>&1; then
    write_result FAIL-MISSING-TOOL windows-vm-not-configured 2
    exit 2
fi
if ! virsh_call -c "$PROFILE_VM_URI" domcapabilities --virttype kvm \
  >/dev/null 2>&1; then
    write_result FAIL-MISSING-TOOL kvm-unavailable 2
    exit 2
fi
domain_xml=$(virsh_call -c "$PROFILE_VM_URI" dumpxml --inactive \
  "$PROFILE_VM_NAME" 2>/dev/null) || {
    write_result FAIL-TOOL windows-vm-inspection-failure 3
    exit 3
}
case "$domain_xml" in
    *"<domain type='kvm'"*|*'<domain type="kvm"'*) ;;
    *)
        write_result FAIL-TOOL windows-vm-not-kvm 3
        exit 3
        ;;
esac
case "$domain_xml" in
    *'org.qemu.guest_agent.0'*) ;;
    *)
        write_result FAIL-MISSING-TOOL qemu-guest-agent-channel-not-configured 2
        exit 2
        ;;
esac

write_result PASS provider-prepared 0