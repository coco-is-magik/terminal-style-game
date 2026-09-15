#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

mkdir -p "$temporary/display-acceptance/linux-x11" "$temporary/ordinary-directory"
printf 'capture\n' >"$temporary/display-acceptance/linux-x11/presentation.png"
printf 'binary\n' >"$temporary/ordinary-binary"
printf 'object\n' >"$temporary/ordinary-directory/object.o"

make -s -C "$root" BUILD_DIR="$temporary" clean

test -f "$temporary/display-acceptance/linux-x11/presentation.png"
test ! -e "$temporary/ordinary-binary"
test ! -e "$temporary/ordinary-directory"

echo "PASS: clean preserves acceptance evidence and removes ordinary build products"