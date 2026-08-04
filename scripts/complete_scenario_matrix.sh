#!/bin/bash
# Complete remaining dynamic-scene scenario benchmarks after a partial run.
set -o pipefail

export DISPLAY="${DISPLAY:-:0}"
LOG_BASE="docs/smc_benchmark_logs"
OUT="${LOG_BASE}/scenario_matrix_completion.md"

mkdir -p "$LOG_BASE"
{
  echo "# Scenario Matrix Completion"
  echo ""
  echo "**Date**: $(date -Iseconds)"
  echo "**Host**: $(uname -n)"
  echo "**CPU**: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | sed 's/^ //')"
  echo ""
} > "$OUT"

run_one() {
  local scenario="$1"
  local mode="$2"
  local flags="$3"
  local log_dir="${LOG_BASE}/scenario_${scenario}_${mode}"

  mkdir -p "$log_dir"
  echo "Running ${scenario} / ${mode}" | tee -a "$OUT"

  make clean > /dev/null 2>&1 || true
  if ! make PROFILE_FRAME=1 $flags > "${log_dir}/build.log" 2>&1; then
    echo "**BUILD_FAIL: ${mode}**" >> "$OUT"
    return 1
  fi

  if ! ./build/ascii-fps --benchmark-scenario "${scenario}" --frames 600 > "${log_dir}/run.log" 2>&1; then
    sleep 1
    if ! ./build/ascii-fps --benchmark-scenario "${scenario}" --frames 600 > "${log_dir}/run.log" 2>&1; then
      echo "**RUN_FAIL: ${mode}**" >> "$OUT"
      cat "${log_dir}/run.log" >> "$OUT"
      echo "" >> "$OUT"
      return 1
    fi
  fi

  cat "${log_dir}/run.log" >> "$OUT"
  echo "" >> "$OUT"
  grep -E 'avg_render_ms|skip_rate|cells_rasterized|checks=|"result"' "${log_dir}/run.log" | head -8
  return 0
}

# camera dirty/batch already complete from prior run; redo stream + remaining scenarios
echo "### Scenario: camera" >> "$OUT"
echo "" >> "$OUT"
run_one camera smc_stream_opt "USE_SMC_STREAM_STATE_TRACKER=1"

for scenario in rotate flicker ui fullchange; do
  echo "### Scenario: ${scenario}" >> "$OUT"
  echo "" >> "$OUT"
  run_one "$scenario" dirty_cells "USE_DIRTY_CELLS=1"
  run_one "$scenario" smc_batch "USE_SMC_BATCH_STATE_TRACKER=1"
  run_one "$scenario" smc_stream_opt "USE_SMC_STREAM_STATE_TRACKER=1"
done

echo "---" >> "$OUT"
echo "DONE $(date -Iseconds)" >> "$OUT"
echo "Wrote $OUT"
