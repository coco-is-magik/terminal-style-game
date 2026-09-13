#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
profile_path=$1
step=${WINDOWS_DEPENDENCY_STEP:-}
. "$profile_path"
output=${PLATFORM_OUTPUT:-$root/build/platform-profiles}
profile_output="$output/$PROFILE_ID"
archives="$profile_output/dependency-archives"
python_command=${PLATFORM_PYTHON:-python3}
mkdir -p "$profile_output"

if test -z "$step"; then
    echo "FAIL-MISSING-TOOL: profile=$PROFILE_ID phase=dependency-build reason=windows-dependency-step-not-specified status=2"
    exit 2
fi

if ! command -v "$python_command" >/dev/null 2>&1; then
    echo "FAIL-MISSING-TOOL: profile=$PROFILE_ID phase=dependency-build reason=python3-not-found status=2"
    exit 2
fi

preflight_status=0
"$root/tools/platform-profiles/prepare-libvirt-windows.sh" "$profile_path" || \
  preflight_status=$?
test "$preflight_status" -eq 0 || exit "$preflight_status"

if ! "$python_command" "$root/tools/platform-profiles/windows_dependencies.py" \
  acquire "$archives" >"$profile_output/dependency-acquire.log" 2>&1; then
    echo "FAIL-TOOL: profile=$PROFILE_ID phase=dependency-build reason=dependency-archive-acquisition-failure status=3"
    exit 3
fi

PLATFORM_WINDOWS_DEPENDENCY_ARCHIVES="$archives" \
PLATFORM_WINDOWS_GUEST_RUNNER="$root/tools/platform-profiles/windows_dependencies.py" \
PLATFORM_WINDOWS_DEPENDENCY_MODE=bootstrap \
PLATFORM_WINDOWS_DEPENDENCY_STEP="$step" \
  "$root/tools/platform-profiles/run-libvirt-windows.sh" "$profile_path"