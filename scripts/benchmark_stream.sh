#!/bin/bash
# Focused benchmark: compare renderer state-tracking modes.
# Runs N passes per mode, extracts key metrics, and prints a summary table.

set -e

PASSES="${1:-5}"
DURATION="${2:-5}"

MODES="baseline dirty smc_generic smc_indexed smc_batch smc_stream"

declare -A MODE_FLAG
MODE_FLAG[baseline]=""
MODE_FLAG[dirty]="USE_DIRTY_CELLS=1"
MODE_FLAG[smc_generic]="USE_SMC_STATE_TRACKER=1"
MODE_FLAG[smc_indexed]="USE_SMC_INDEXED_STATE_TRACKER=1"
MODE_FLAG[smc_batch]="USE_SMC_BATCH_STATE_TRACKER=1"
MODE_FLAG[smc_stream]="USE_SMC_STREAM_STATE_TRACKER=1"

TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

run_mode() {
    local mode=$1
    local pass=$2
    local flag=${MODE_FLAG[$mode]}
    make clean >/dev/null 2>&1
    if [ -n "$flag" ]; then
        make "$flag" PROFILE_FRAME=1 >/dev/null 2>&1
    else
        make PROFILE_FRAME=1 >/dev/null 2>&1
    fi
    ./build/ascii-fps --benchmark-raycast "$DURATION" 2>&1 | tee "$TMPDIR/${mode}_${pass}.log"
}

extract() {
    local file=$1
    local key=$2
    grep -E "$key" "$file" | head -1 | sed -E 's/.*[^0-9]([0-9]+(\.[0-9]+)?).*/\1/'
}

for mode in $MODES; do
    echo "=== $mode ==="
    for pass in $(seq 1 $PASSES); do
        echo "  pass $pass/$PASSES"
        run_mode "$mode" "$pass" >/dev/null
    done
done

echo ""
echo "=== Summary (medians over $PASSES passes, ${DURATION}s each) ==="
printf "%-14s %8s %8s %10s %10s %10s %10s %10s %10s %10s\n" \
    "mode" "avg_ms" "worst_ms" "frames" "cells_rast" "cells_skip" "checks" "bytes_cmp" "pack_ms" "diff_ms"

for mode in $MODES; do
    avg_ms=$(for p in $(seq 1 $PASSES); do extract "$TMPDIR/${mode}_${p}.log" '"avg_render_ms"'; done | sort -n | awk '{a[NR]=$1} END {if(NR%2==1) print a[int(NR/2)+1]; else print (a[NR/2]+a[NR/2+1])/2}')
    worst_ms=$(for p in $(seq 1 $PASSES); do extract "$TMPDIR/${mode}_${p}.log" '"worst_render_ms"'; done | sort -n | awk '{a[NR]=$1} END {if(NR%2==1) print a[int(NR/2)+1]; else print (a[NR/2]+a[NR/2+1])/2}')
    frames=$(for p in $(seq 1 $PASSES); do extract "$TMPDIR/${mode}_${p}.log" '"frames"'; done | sort -n | awk '{a[NR]=$1} END {if(NR%2==1) print a[int(NR/2)+1]; else print (a[NR/2]+a[NR/2+1])/2}')
    cells_rast=$(for p in $(seq 1 $PASSES); do extract "$TMPDIR/${mode}_${p}.log" 'cells_rasterized='; done | sort -n | awk '{a[NR]=$1} END {if(NR%2==1) print a[int(NR/2)+1]; else print (a[NR/2]+a[NR/2+1])/2}')
    cells_skip=$(for p in $(seq 1 $PASSES); do extract "$TMPDIR/${mode}_${p}.log" 'cells_skipped='; done | sort -n | awk '{a[NR]=$1} END {if(NR%2==1) print a[int(NR/2)+1]; else print (a[NR/2]+a[NR/2+1])/2}')
    checks=$(for p in $(seq 1 $PASSES); do extract "$TMPDIR/${mode}_${p}.log" 'checks='; done | sort -n | awk '{a[NR]=$1} END {if(NR%2==1) print a[int(NR/2)+1]; else print (a[NR/2]+a[NR/2+1])/2}')
    bytes_cmp=$(for p in $(seq 1 $PASSES); do extract "$TMPDIR/${mode}_${p}.log" 'bytes_compared='; done | sort -n | awk '{a[NR]=$1} END {if(NR%2==1) print a[int(NR/2)+1]; else print (a[NR/2]+a[NR/2+1])/2}')
    pack_ms=$(for p in $(seq 1 $PASSES); do grep 'state packing:' "$TMPDIR/${mode}_${p}.log" | head -1 | sed -E 's/.*state packing: *([0-9]+(\.[0-9]+)?).*/\1/'; done | sort -n | awk '{a[NR]=$1} END {if(NR%2==1) print a[int(NR/2)+1]; else print (a[NR/2]+a[NR/2+1])/2}')
    diff_ms=$(for p in $(seq 1 $PASSES); do grep 'smc diff:' "$TMPDIR/${mode}_${p}.log" | head -1 | sed -E 's/.*smc diff: *([0-9]+(\.[0-9]+)?).*/\1/'; done | sort -n | awk '{a[NR]=$1} END {if(NR%2==1) print a[int(NR/2)+1]; else print (a[NR/2]+a[NR/2+1])/2}')
    printf "%-14s %8.2f %8.2f %10.0f %10.0f %10.0f %10.0f %10.0f %10.3f %10.3f\n" \
        "$mode" "$avg_ms" "$worst_ms" "$frames" "$cells_rast" "$cells_skip" "$checks" "$bytes_cmp" "$pack_ms" "$diff_ms"
done
