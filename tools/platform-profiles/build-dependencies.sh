#!/bin/sh
set -eu

prefix=/opt/platform-deps
work=/tmp/platform-deps
mkdir -p "$prefix/source" "$work"

fetch_extract() {
    name=$1
    url=$2
    checksum=$3
    archive="$work/$name.tar.gz"
    destination="$work/$name"
    curl --fail --location --proto '=https' --tlsv1.2 "$url" -o "$archive"
    echo "$checksum  $archive" | sha256sum -c -
    mkdir "$destination"
    tar -xzf "$archive" --strip-components=1 -C "$destination"
}

fetch_extract SDL \
  "https://github.com/libsdl-org/SDL/archive/${SDL_COMMIT}.tar.gz" "$SDL_SHA256"
cmake -S "$work/SDL" -B "$work/SDL-build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DCMAKE_INSTALL_LIBDIR=lib64 -DSDL_UNIX_CONSOLE_BUILD=ON \
  -DSDL3_MAINPROJECT=ON -DSDL_INSTALL=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
cmake --build "$work/SDL-build" --parallel 2
cmake --install "$work/SDL-build"
test -f "$prefix/lib64/cmake/SDL3/SDL3Config.cmake"

fetch_extract SDL_mixer \
  "https://github.com/libsdl-org/SDL_mixer/archive/${SDL_MIXER_COMMIT}.tar.gz" \
  "$SDL_MIXER_SHA256"
cmake -S "$work/SDL_mixer" -B "$work/SDL_mixer-build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DCMAKE_INSTALL_LIBDIR=lib64 -DSDL3_DIR="$prefix/lib64/cmake/SDL3" \
  -DSDLMIXER_VENDORED=OFF -DSDLMIXER_EXAMPLES=OFF -DSDLMIXER_TESTS=OFF \
  -DSDLMIXER_FLAC=OFF -DSDLMIXER_GME=OFF -DSDLMIXER_MOD=OFF \
  -DSDLMIXER_MP3=OFF -DSDLMIXER_MIDI=OFF -DSDLMIXER_OPUS=OFF \
  -DSDLMIXER_WAVPACK=OFF
cmake --build "$work/SDL_mixer-build" --parallel 2
cmake --install "$work/SDL_mixer-build"

fetch_extract enet \
  "https://github.com/lsalzman/enet/archive/${ENET_COMMIT}.tar.gz" "$ENET_SHA256"
cmake -S "$work/enet" -B "$work/enet-build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DCMAKE_INSTALL_LIBDIR=lib64 -DENET_BUILD_TESTS=OFF
cmake --build "$work/enet-build" --parallel 2
cmake --install "$work/enet-build"

fetch_extract cmocka \
  "https://gitlab.com/cmocka/cmocka/-/archive/${CMOCKA_COMMIT}/cmocka-${CMOCKA_COMMIT}.tar.gz" \
  "$CMOCKA_SHA256"
cmake -S "$work/cmocka" -B "$work/cmocka-build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DCMAKE_INSTALL_LIBDIR=lib64 -DWITH_EXAMPLES=OFF -DUNIT_TESTING=OFF
cmake --build "$work/cmocka-build" --parallel 2
cmake --install "$work/cmocka-build"

fetch_extract smc \
  "https://github.com/coco-is-magik/self-modifying-calculator/archive/${SMC_COMMIT}.tar.gz" \
  "$SMC_SHA256"
mv "$work/SDL" "$prefix/source/SDL"
mv "$work/smc" "$prefix/source/smc"
rm -rf "$work"