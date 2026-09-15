#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output=${DISPLAY_ACCEPTANCE_OUTPUT:-$root/build/display-acceptance/linux-x11}
duration=${DISPLAY_ACCEPTANCE_DURATION:-12}
log="$output/application.log"
host_log="$output/host.log"
screenshot="$output/presentation.png"

mkdir -p "$output"
rm -f "$log" "$host_log" "$screenshot"
test -n "${DISPLAY:-}" || { echo 'FAIL-MISSING-TOOL: DISPLAY is unset'; exit 2; }
command -v xdpyinfo >/dev/null 2>&1 || { echo 'FAIL-MISSING-TOOL: xdpyinfo'; exit 2; }
command -v import >/dev/null 2>&1 || { echo 'FAIL-MISSING-TOOL: ImageMagick import'; exit 2; }

{
    printf 'session_type=x11\n'
    printf 'session_context=local-non-remote\n'
    printf 'display=%s\n' "$DISPLAY"
    printf 'keyboard_device=Virtual core keyboard\n'
    printf 'pointer_device=Virtual core pointer\n'
    printf 'input_method=synthetic-host-XTest-not-physical\n'
    printf 'capture_method=ImageMagick-import-native-X11-window\n'
    printf 'resize_method=standard-EWMH-fullscreen-request\n'
    printf 'command=make display-acceptance-linux\n'
    printf 'duration_limit_seconds=%s\n' "$duration"
    xdpyinfo | sed -n '1,6p'
} >"$host_log"

cd "$root"
./build/ascii-fps --display-acceptance "$duration" >"$log" 2>&1 &
app_pid=$!
cleanup() {
    if kill -0 "$app_pid" 2>/dev/null; then
        kill "$app_pid" 2>/dev/null || true
        wait "$app_pid" 2>/dev/null || true
    fi
}
trap cleanup HUP INT TERM EXIT

if ! PYTHONDONTWRITEBYTECODE=1 tools/display-acceptance/x11_drive.py "$screenshot" >>"$host_log" 2>&1; then
    echo "FAIL-TOOL: native X11 driver failed; details=$host_log"
    exit 3
fi
status=0
wait "$app_pid" || status=$?
trap - HUP INT TERM EXIT
test "$status" -eq 0 || {
    cat "$log"
    echo "FAIL-PRODUCT: native X11 display acceptance status=$status"
    exit "$status"
}
test -s "$screenshot" || { echo 'FAIL-TOOL: presentation capture is empty'; exit 3; }
grep -F '"display_acceptance":"pass"' "$log" >/dev/null || {
    cat "$log"
    echo 'FAIL-PRODUCT: passing acceptance record missing'
    exit 1
}
cat "$log"
echo "PASS: native X11 display/input acceptance; evidence=$output"