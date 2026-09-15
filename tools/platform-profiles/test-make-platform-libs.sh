#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)

linux_libs=$(make -s -C "$root" -pn OS= | sed -n 's/^LIBS := //p' | head -n 1)
linux_test_libs=$(make -s -C "$root" -pn OS= | sed -n 's/^TEST_LIBS := //p' | head -n 1)
windows_libs=$(make -s -C "$root" -pn OS=Windows_NT | sed -n 's/^LIBS := //p' | head -n 1)
windows_test_libs=$(make -s -C "$root" -pn OS=Windows_NT | sed -n 's/^TEST_LIBS := //p' | head -n 1)

case "$linux_libs $linux_test_libs" in
    *-lwinmm*|*-lws2_32*)
        echo "FAIL: Windows system libraries leaked into Linux link policy"
        exit 1
        ;;
esac

case "$windows_libs" in
    *'-lenet -lwinmm -lws2_32'*) ;;
    *) echo "FAIL: application link lacks ordered Windows ENet dependencies"; exit 1 ;;
esac

case "$windows_test_libs" in
    *'-lenet -lwinmm -lws2_32'*) ;;
    *) echo "FAIL: test link lacks ordered Windows ENet dependencies"; exit 1 ;;
esac

echo "PASS: platform-specific Make link policy tests passed"
