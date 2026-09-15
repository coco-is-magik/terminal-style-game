#!/bin/sh
set -eu

log=$1
result=$2
sweep_status=$3
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
errors="$temporary/errors"
known="$temporary/known"
additional="$temporary/additional"

grep -E ': (fatal )?error:' "$log" >"$errors" || true
: >"$known"
: >"$additional"

while IFS= read -r line; do
    case "$line" in
        *': error: no newline at end of file [-Werror,-Wnewline-eof]')
            diagnostic_path=${line%%:*}
            case "$diagnostic_path" in
                vendor/src/smc/include/smc.h|*/vendor/src/smc/include/smc.h|vendor/src/smc/src/c/smc_artifact.c|*/vendor/src/smc/src/c/smc_artifact.c|vendor/src/smc/src/c/smc_artifact.h|*/vendor/src/smc/src/c/smc_artifact.h|vendor/src/smc/src/c/smc_runtime_stub.c|*/vendor/src/smc/src/c/smc_runtime_stub.c)
                    printf '%s\n' "$line" >>"$known"
                    ;;
                *)
                    printf '%s\n' "$line" >>"$additional"
                    ;;
            esac
            ;;
        *)
            printf '%s\n' "$line" >>"$additional"
            ;;
    esac
done <"$errors"

known_complete=true
for path in \
  vendor/src/smc/include/smc.h \
  vendor/src/smc/src/c/smc_artifact.c \
  vendor/src/smc/src/c/smc_artifact.h \
  vendor/src/smc/src/c/smc_runtime_stub.c; do
    grep -F "$path:" "$known" >/dev/null || known_complete=false
done

scope=additional-or-inconclusive-clang-diagnostics
additional_diagnostics=true
if test "$sweep_status" -eq 2 && test -s "$errors" &&
  test ! -s "$additional" && test "$known_complete" = true; then
    scope=known-deferred-smc-newline-only
    additional_diagnostics=false
fi

{
    echo "DIAGNOSTIC_SCOPE=$scope"
    echo "ADDITIONAL_DIAGNOSTICS=$additional_diagnostics"
    echo "SWEEP_STATUS=$sweep_status"
    echo "KNOWN_DIAGNOSTIC_LINES=$(wc -l <"$known")"
    echo "ADDITIONAL_DIAGNOSTIC_LINES=$(wc -l <"$additional")"
} >"$result"

test "$additional_diagnostics" = false