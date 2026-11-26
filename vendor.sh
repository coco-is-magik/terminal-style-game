#!/usr/bin/env bash
set -e

ROOT=$(pwd)

mkdir -p vendor/src vendor/build vendor/dist

cd vendor/src

[ -d SDL ] || git clone https://github.com/libsdl-org/SDL.git
[ -d SDL_mixer ] || git clone https://github.com/libsdl-org/SDL_mixer.git
[ -d enet ] || git clone https://github.com/lsalzman/enet.git
[ -d cmocka ] || git clone https://gitlab.com/cmocka/cmocka.git
[ -d smc ] || git clone https://github.com/coco-is-magik/self-modifying-calculator.git smc
# utf8proc is pinned to a tag rather than tracking a branch: D10 fixes the Unicode version to the
# vendored data release, so a floating master would silently change what "valid UTF-8" and "one
# grapheme cluster" mean. Changing this tag is a deliberate act that must update the record with it.
[ -d utf8proc ] || git clone --branch v2.12.0 --depth 1 https://github.com/JuliaStrings/utf8proc.git utf8proc

build_dep () {
    NAME=$1
    SRC=$2

    mkdir -p "$ROOT/vendor/build/$NAME"
    cd "$ROOT/vendor/build/$NAME"

    cmake "$SRC" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$ROOT/vendor/dist"

    cmake --build . -j
    cmake --install .
}

build_dep sdl "$ROOT/vendor/src/SDL"
build_dep sdl_mixer "$ROOT/vendor/src/SDL_mixer"
build_dep enet "$ROOT/vendor/src/enet"
build_dep cmocka "$ROOT/vendor/src/cmocka"
