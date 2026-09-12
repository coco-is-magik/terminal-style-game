#!/bin/sh
set -u

gate_dir=/usr/local/lib/platform-profile
. "$gate_dir/classify.sh"
result=/output/result.env

write_result() {
    phase=$1
    classification=$2
    outcome=${classification%%|*}
    rest=${classification#*|}
    reason=${rest%%|*}
    status=${rest##*|}
    {
        echo "PROFILE_ID=$PROFILE_ID"
        echo "PROFILE_ROLE=$PROFILE_ROLE"
        echo "PHASE=$phase"
        echo "OUTCOME=$outcome"
        echo "REASON=$reason"
        echo "STATUS=$status"
    } >"$result"
    echo "$outcome: profile=$PROFILE_ID phase=$phase reason=$reason status=$status"
}

finish_failure() {
    phase=$1
    status=$2
    log=$3
    classified=$(classify_platform_result "$phase" "$status" "$log")
    classifier_status=$?
    write_result "$phase" "$classified"
    exit "$classifier_status"
}

if test ! -r /source/Makefile || test ! -d /output || test ! -w /output; then
    write_result environment 'FAIL-TOOL|container-mount-contract|3'
    exit 3
fi
if ! command -v make >/dev/null 2>&1; then
    finish_failure missing-make 127 /dev/null
fi
if ! command -v "$PROFILE_COMPILER" >/dev/null 2>&1; then
    finish_failure missing-compiler 127 /dev/null
fi

rm -rf /work/source
mkdir -p /work/source
if ! tar -C /source --exclude='./.git' --exclude='./.cache' --exclude='./build' \
  --exclude='./vendor' -cf /work/source.tar .; then
    write_result environment 'FAIL-TOOL|source-copy-failure|3'
    exit 3
fi
if ! tar -C /work/source -xf /work/source.tar; then
    write_result environment 'FAIL-TOOL|workspace-extract-failure|3'
    exit 3
fi
rm -f /work/source.tar
cd /work/source || exit 3
mkdir -p build vendor/src
ln -s /opt/platform-deps vendor/dist
ln -s /opt/platform-deps/source/SDL vendor/src/SDL
ln -s /opt/platform-deps/source/smc vendor/src/smc

{
    echo "profile_id=$PROFILE_ID"
    echo "profile_role=$PROFILE_ROLE"
    echo "architecture=$(uname -m)"
    echo "kernel=$(uname -sr)"
    echo "compiler=$($PROFILE_COMPILER --version | head -n 1)"
    echo "linker=$($PROFILE_COMPILER -Wl,--version 2>&1 | head -n 1)"
    echo "libc=$(ldd --version 2>&1 | head -n 1)"
    echo "make=$(make --version | head -n 1)"
    echo "cflags=-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror"
    echo "sdl_commit=$SDL_COMMIT"
    echo "sdl_mixer_commit=$SDL_MIXER_COMMIT"
    echo "enet_commit=$ENET_COMMIT"
    echo "cmocka_commit=$CMOCKA_COMMIT"
    echo "smc_commit=$SMC_COMMIT"
} > /output/environment.log
cp /opt/platform-deps/packages.txt /output/packages.txt

dependency_log=/output/dependency-check.log
status=0
{
    test -f vendor/dist/include/SDL3/SDL.h
    test -f vendor/dist/include/cmocka.h
    test -f vendor/dist/include/enet/enet.h
    test -f vendor/dist/lib64/libSDL3.so
    test -f vendor/dist/lib64/libSDL3_mixer.so
    test -f vendor/dist/lib64/libcmocka.so
    test -f vendor/dist/lib64/libenet.so || test -f vendor/dist/lib64/libenet.a
    test -f vendor/src/smc/Makefile
} >"$dependency_log" 2>&1 || status=$?
test "$status" -eq 0 || finish_failure dependency-check "$status" "$dependency_log"

app_log=/output/strict-app-build.log
status=0
timeout 900s make CC="$PROFILE_COMPILER" all >"$app_log" 2>&1 || status=$?
test "$status" -eq 0 || finish_failure strict-app-build "$status" "$app_log"

test_build_log=/output/strict-test-build.log
status=0
timeout 1800s make CC="$PROFILE_COMPILER" test-build >"$test_build_log" 2>&1 || status=$?
test "$status" -eq 0 || finish_failure strict-test-build "$status" "$test_build_log"

test_run_log=/output/complete-test-run.log
status=0
timeout 1800s make CC="$PROFILE_COMPILER" test >"$test_run_log" 2>&1 || status=$?
test "$status" -eq 0 || finish_failure complete-test-run "$status" "$test_run_log"

standards_log=/output/standards-core.log
status=0
timeout 120s make CC="$PROFILE_COMPILER" standards-core >"$standards_log" 2>&1 || status=$?
test "$status" -eq 0 || finish_failure standards-core "$status" "$standards_log"

write_result complete 'PASS|profile-compatible|0'