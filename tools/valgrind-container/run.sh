#!/bin/sh
set -u

gate_dir=/usr/local/lib/valgrind-gate
. "$gate_dir/classify.sh"

if test "${1:-}" = --self-test; then
    exec "$gate_dir/self-test.sh"
fi

if test ! -r /source/Makefile || test ! -d /output; then
    echo "FAIL-TOOL: reason=container-mount-contract"
    exit 3
fi

rm -rf /work/source
mkdir -p /work/source /output
if ! tar -C /source \
  --exclude='./.git' --exclude='./.cache' --exclude='./build' \
  --exclude='./vendor/dist' --exclude='./vendor/src/SDL' \
  -cf /work/source.tar .; then
    echo "FAIL-TOOL: reason=source-copy-failure"
    exit 3
fi
if ! tar -C /work/source -xf /work/source.tar; then
    echo "FAIL-TOOL: reason=workspace-extract-failure"
    exit 3
fi
rm -f /work/source.tar
cd /work/source || exit 3
mkdir -p build vendor/src
ln -s /opt/valgrind-deps vendor/dist
ln -s /opt/valgrind-deps/source/SDL vendor/src/SDL
cp /opt/valgrind-deps/manifest.txt /output/image-manifest.txt

build_log=/output/build.log
if ! make build/test-decal-io build/test-core >"$build_log" 2>&1; then
    cat "$build_log"
    echo "FAIL-PRODUCT: reason=leak-runner-build-failure log=build/valgrind-container/build.log"
    exit 1
fi
cat "$build_log"

for runner in build/test-decal-io build/test-core; do
    name=${runner##*/}
    direct_log="/output/direct-${name}.log"
    memcheck_log="/output/valgrind-${name}.log"
    status=0
    "./$runner" >"$direct_log" 2>&1 || status=$?
    cat "$direct_log"
    classify_result direct "$status" "$direct_log" || exit $?

    status=0
    valgrind --error-exitcode=100 --leak-check=full --show-leak-kinds=all \
      --errors-for-leak-kinds=definite,indirect,possible \
      "./$runner" >"$memcheck_log" 2>&1 || status=$?
    cat "$memcheck_log"
    classify_result memcheck "$status" "$memcheck_log" || exit $?
done

echo "PASS: canonical containerized Valgrind leak gate passed"