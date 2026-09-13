#!/bin/sh
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
profile_path=$1
PROFILE_PROVIDER=docker
. "$profile_path"

case "$PROFILE_PROVIDER" in
    docker)
        run=${PLATFORM_DOCKER_RUN:-$root/tools/platform-profiles/run-one.sh}
        ;;
    libvirt-windows)
        run=${PLATFORM_LIBVIRT_WINDOWS_RUN:-$root/tools/platform-profiles/run-libvirt-windows.sh}
        ;;
    *)
        echo "FAIL-TOOL: profile=$PROFILE_ID reason=unsupported-profile-provider"
        exit 3
        ;;
esac

exec "$run" "$profile_path"