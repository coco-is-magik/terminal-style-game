#!/bin/bash

set -o pipefail

OUTPUT_FILE="docs/SMC_BENCHMARK_RESULTS.md"
LOG_BASE="docs/smc_benchmark_logs"
GRID_WIDTH=160
GRID_HEIGHT=260
CELL_COUNT=$((GRID_WIDTH * GRID_HEIGHT))

mkdir -p "$LOG_BASE" docs

{
  echo "# SMC State Tracking Benchmark Results"
  echo ""
  echo "**Date**: $(date -Iseconds)"
  echo "**Git commit**: $(git rev-parse --short HEAD)"
  echo "**Git branch**: $(git rev-parse --abbrev-ref HEAD)"
  echo "**SMC submodule commit**: $(cd vendor/src/smc && git rev-parse --short HEAD)"
  echo "**Grid**: ${GRID_WIDTH}x${GRID_HEIGHT} (${CELL_COUNT} cells)"
  echo ""
} > "$OUTPUT_FILE"

MODE_NAMES=("baseline" "dirty_cells" "smc_indexed" "smc_batch" "smc_stream_opt" "smc_stream_base")
BUILD_FLAGS=("" "USE_DIRTY_CELLS=1" "USE_SMC_INDEXED_STATE_TRACKER=1" "USE_SMC_BATCH_STATE_TRACKER=1" "USE_SMC_STREAM_STATE_TRACKER=1" "USE_SMC_STREAM_STATE_TRACKER=1 SMC_CFLAGS+=-DSMC_DISABLE_OPTIMIZED_STREAM_KERNELS")
STREAM_BASE_MODE="smc_stream_base"

declare -A BUILD_STATUS
declare -A RUN_STATUS
declare -A PARSE_STATUS
declare -A CORRECTNESS_STATUS
declare -A DECISION_STATUS
declare -A PARSED_VALUES

# ============================================
# Helpers
# ============================================

get_value() {
  echo "${PARSED_VALUES[$1]:-missing}"
}

extract_json() {
  grep -oP "\"${2}\":\\s*\\K[0-9.]+" "$1" 2>/dev/null | head -1 || echo "missing"
}

extract_stats() {
  grep -oP "${2}=\\K[0-9]+" "$1" 2>/dev/null | head -1 || echo "missing"
}

# Profile labels have spaces and colons in output: "grid/raycast:", "state packing:", etc.
extract_profile() {
  case "$2" in
    raycast_grid_ms) grep -oP "grid/raycast:\\s*\\K[0-9.]+" "$1" 2>/dev/null | head -1 || echo "missing" ;;
    state_pack_ms) grep -oP "state packing:\\s*\\K[0-9.]+" "$1" 2>/dev/null | head -1 || echo "missing" ;;
    smc_diff_ms) grep -oP "smc diff:\\s*\\K[0-9.]+" "$1" 2>/dev/null | head -1 || echo "missing" ;;
    smc_stream_diff_ms) grep -oP "smc stream diff:\\s*\\K[0-9.]+" "$1" 2>/dev/null | head -1 || echo "missing" ;;
    dirty_check_ms) grep -oP "dirty decision:\\s*\\K[0-9.]+" "$1" 2>/dev/null | head -1 || echo "missing" ;;
    dirty_iter_ms) grep -oP "dirty iteration:\\s*\\K[0-9.]+" "$1" 2>/dev/null | head -1 || echo "missing" ;;
    raster_ms) grep -oP "rasterization:\\s*\\K[0-9.]+" "$1" 2>/dev/null | head -1 || echo "missing" ;;
    sdl_update_ms) grep -oP "SDL/update/present:\\s*\\K[0-9.]+" "$1" 2>/dev/null | head -1 || echo "missing" ;;
    *) echo "missing" ;;
  esac
}

median3_or_missing() {
  arr=($1)
  if [[ "${#arr[@]}" -ne 3 ]]; then
    echo "missing"
    return
  fi
  for v in "${arr[@]}"; do
    if [[ "$v" == "missing" ]] || ! [[ "$v" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
      echo "missing"
      return
    fi
  done
  printf "%s\n" "${arr[@]}" | sort -n | sed -n '2p'
}

field_present() {
  [[ "$(get_value "$1")" != "missing" ]]
}

median_field() {
  mode="$1"
  field="$2"
  vals=""
  for run in 1 2 3; do
    vals="$vals $(get_value "${mode}|${run}|${field}")"
  done
  median3_or_missing "$vals"
}

# ============================================
# Runner
# ============================================

run_mode_benchmarks() {
  local mode_name="$1"
  local build_flags="$2" 
  local is_stream_baseline="${3:-false}"
  local log_dir="${LOG_BASE}/${mode_name}"
  
  mkdir -p "$log_dir"
  
  echo "## Mode: ${mode_name}" >> "$OUTPUT_FILE"
  echo "" >> "$OUTPUT_FILE"
  PARSE_STATUS[$mode_name]="PARSE_FAIL"
  
  make clean > /dev/null 2>&1 || true
  
  build_log="${log_dir}/build.log"
  echo "### Build" >> "$OUTPUT_FILE"
  echo '```' >> "$OUTPUT_FILE"
  
  if [[ "$is_stream_baseline" == "true" ]]; then
    if ! make PROFILE_FRAME=1 USE_SMC_STREAM_STATE_TRACKER=1 SMC_CFLAGS+=-DSMC_DISABLE_OPTIMIZED_STREAM_KERNELS V=1 > "$build_log" 2>&1; then
      cat "$build_log" >> "$OUTPUT_FILE"
      echo '```' >> "$OUTPUT_FILE"
      echo "**BUILD_FAIL**" >> "$OUTPUT_FILE"
      BUILD_STATUS[$mode_name]="BUILD_FAIL"
      return
    fi
    cat "$build_log" >> "$OUTPUT_FILE"
    echo '```' >> "$OUTPUT_FILE"
    if ! grep -E "smc_state\\.c.*-DSMC_DISABLE_OPTIMIZED_STREAM_KERNELS|-DSMC_DISABLE_OPTIMIZED_STREAM_KERNELS.*smc_state\\.c" "$build_log" > /dev/null 2>&1; then
      echo "**BUILD_FAIL**: SMC_DISABLE_OPTIMIZED_STREAM_KERNELS flag not verified**" >> "$OUTPUT_FILE"
      BUILD_STATUS[$mode_name]="BUILD_FAIL"
      return
    fi
  else
    if ! make PROFILE_FRAME=1 $build_flags > "$build_log" 2>&1; then
      cat "$build_log" >> "$OUTPUT_FILE"
      echo '```' >> "$OUTPUT_FILE"
      echo "**BUILD_FAIL**" >> "$OUTPUT_FILE"
      BUILD_STATUS[$mode_name]="BUILD_FAIL"
      return
    fi
    cat "$build_log" >> "$OUTPUT_FILE"
    echo '```' >> "$OUTPUT_FILE"
  fi
  
  BUILD_STATUS[$mode_name]="BUILD_PASS"
  
  if [[ ! -x "./build/ascii-fps" ]]; then
    echo "**BUILD_FAIL**: Binary not created**" >> "$OUTPUT_FILE"
    BUILD_STATUS[$mode_name]="BUILD_FAIL"
    return
  fi
  
  echo "### Warmup (discarded)" >> "$OUTPUT_FILE"
  echo '```' >> "$OUTPUT_FILE"
  ./build/ascii-fps --benchmark-raycast 2 > "${log_dir}/warmup.log" 2>&1 || true
  head -20 "${log_dir}/warmup.log" >> "$OUTPUT_FILE"
  echo '```' >> "$OUTPUT_FILE"
  echo "" >> "$OUTPUT_FILE"
  
  echo "### Measured Runs" >> "$OUTPUT_FILE"
  RUN_STATUS[$mode_name]="RUN_PASS"
  
  for run in 1 2 3; do
    run_log="${log_dir}/run_${run}.log"
    echo "#### Run $run" >> "$OUTPUT_FILE"
    echo '```' >> "$OUTPUT_FILE"
    
    if ! ./build/ascii-fps --benchmark-raycast 5 > "$run_log" 2>&1; then
      RUN_STATUS[$mode_name]="RUN_FAIL"
    fi
    
    cat "$run_log" >> "$OUTPUT_FILE"
    echo '```' >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    
    PARSED_VALUES["${mode_name}|${run}|avg_render_ms"]=$(extract_json "$run_log" "avg_render_ms")
    PARSED_VALUES["${mode_name}|${run}|state_pack_ms"]=$(extract_profile "$run_log" "state_pack_ms")
    PARSED_VALUES["${mode_name}|${run}|smc_diff_ms"]=$(extract_profile "$run_log" "smc_diff_ms")
    PARSED_VALUES["${mode_name}|${run}|smc_stream_diff_ms"]=$(extract_profile "$run_log" "smc_stream_diff_ms")
    PARSED_VALUES["${mode_name}|${run}|dirty_check_ms"]=$(extract_profile "$run_log" "dirty_check_ms")
    PARSED_VALUES["${mode_name}|${run}|dirty_iter_ms"]=$(extract_profile "$run_log" "dirty_iter_ms")
    PARSED_VALUES["${mode_name}|${run}|raster_ms"]=$(extract_profile "$run_log" "raster_ms")
    PARSED_VALUES["${mode_name}|${run}|sdl_update_ms"]=$(extract_profile "$run_log" "sdl_update_ms")
    PARSED_VALUES["${mode_name}|${run}|raycast_grid_ms"]=$(extract_profile "$run_log" "raycast_grid_ms")
    PARSED_VALUES["${mode_name}|${run}|checks"]=$(extract_stats "$run_log" "checks")
    PARSED_VALUES["${mode_name}|${run}|changed"]=$(extract_stats "$run_log" "changed")
    PARSED_VALUES["${mode_name}|${run}|unchanged"]=$(extract_stats "$run_log" "unchanged")
    PARSED_VALUES["${mode_name}|${run}|stores"]=$(extract_stats "$run_log" "stores")
    PARSED_VALUES["${mode_name}|${run}|bytes_compared"]=$(extract_stats "$run_log" "bytes_compared")
    PARSED_VALUES["${mode_name}|${run}|out_of_range"]=$(extract_stats "$run_log" "out_of_range")
    PARSED_VALUES["${mode_name}|${run}|fallback_count"]=$(extract_stats "$run_log" "fallback_count")
    PARSED_VALUES["${mode_name}|${run}|cells_processed"]=$(extract_stats "$run_log" "cells_rasterized")
    PARSED_VALUES["${mode_name}|${run}|cells_skipped"]=$(extract_stats "$run_log" "cells_skipped")
  done
  
  for run in 1 2 3; do
    if ! field_present "${mode_name}|${run}|avg_render_ms"; then
      PARSE_STATUS[$mode_name]="PARSE_FAIL (run $run missing avg_render_ms)"
      return
    fi
  done
  
  if [[ "$mode_name" == "smc_indexed" || "$mode_name" == "smc_batch" || "$mode_name" == "smc_stream_opt" || "$mode_name" == "smc_stream_base" ]]; then
    for run in 1 2 3; do
      for field in checks changed unchanged stores bytes_compared out_of_range; do
        if ! field_present "${mode_name}|${run}|${field}"; then
          PARSE_STATUS[$mode_name]="PARSE_FAIL (run $run missing $field)"
          return
        fi
      done
    done
  fi
  
  PARSE_STATUS[$mode_name]="PARSE_PASS"
}

# ============================================
# Execute
# ============================================

for i in "${!MODE_NAMES[@]}"; do
  is_stream_baseline="false"
  if [[ "${MODE_NAMES[$i]}" == "$STREAM_BASE_MODE" ]]; then
    is_stream_baseline="true"
  fi
  run_mode_benchmarks "${MODE_NAMES[$i]}" "${BUILD_FLAGS[$i]}" "$is_stream_baseline"
done

# ============================================
# Summary Table
# ============================================

echo "## Summary (Median Values)" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"
echo "| mode | avg_render_ms | raycast_grid_ms | state_pack_ms | smc_diff_ms | smc_stream_diff_ms | dirty_check_ms | dirty_iter_ms | raster_ms | sdl_update_ms | checks | changed | unchanged | stores | bytes_compared | out_of_range | fallback_count | cells_processed | cells_skipped | parse_status | build_status | run_status |" >> "$OUTPUT_FILE"
echo "|------|-------------|-----------------|---------------|-------------|-------------------|----------------|-------------|---------|--------------|--------|---------|-----------|--------|---------------|-------------|---------------|----------------|---------------|----------------|--------------|------------|" >> "$OUTPUT_FILE"

for mode in "${MODE_NAMES[@]}"; do
  median_avg=$(median_field "$mode" "avg_render_ms")
  
  echo "| $mode | $median_avg | $(median_field "$mode" "raycast_grid_ms") | $(median_field "$mode" "state_pack_ms") | $(median_field "$mode" "smc_diff_ms") | $(median_field "$mode" "smc_stream_diff_ms") | $(median_field "$mode" "dirty_check_ms") | $(median_field "$mode" "dirty_iter_ms") | $(median_field "$mode" "raster_ms") | $(median_field "$mode" "sdl_update_ms") | $(median_field "$mode" "checks") | $(median_field "$mode" "changed") | $(median_field "$mode" "unchanged") | $(median_field "$mode" "stores") | $(median_field "$mode" "bytes_compared") | $(median_field "$mode" "out_of_range") | $(median_field "$mode" "fallback_count") | $(median_field "$mode" "cells_processed") | $(median_field "$mode" "cells_skipped") | ${PARSE_STATUS[$mode]:-PARSE_FAIL} | ${BUILD_STATUS[$mode]:-BUILD_FAIL} | ${RUN_STATUS[$mode]:-RUN_FAIL} |" >> "$OUTPUT_FILE"
done

# ============================================
# Correctness & Decision (Stream)
# ============================================

echo "" >> "$OUTPUT_FILE"
echo "## Correctness Verification (smc_stream_opt)" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

CORRECTNESS_STATUS[smc_stream_opt]="CORRECTNESS_PASS"

for run in 1 2 3; do
  oor=$(get_value "smc_stream_opt|${run}|out_of_range")
  fb=$(get_value "smc_stream_opt|${run}|fallback_count")
  checks=$(get_value "smc_stream_opt|${run}|checks")
  bytes=$(get_value "smc_stream_opt|${run}|bytes_compared")
  
  if [[ "$oor" != "0" ]]; then
    CORRECTNESS_STATUS[smc_stream_opt]="CORRECTNESS_FAIL (run $run: out_of_range=$oor)"
  fi
  if [[ "$fb" != "0" ]]; then
    CORRECTNESS_STATUS[smc_stream_opt]="CORRECTNESS_FAIL (run $run: fallback_count=$fb)"
  fi
  if [[ "$checks" == "missing" || "$bytes" == "missing" ]]; then
    CORRECTNESS_STATUS[smc_stream_opt]="CORRECTNESS_FAIL (run $run: missing checks or bytes_compared)"
  elif ! [[ "$checks" =~ ^[0-9]+$ ]]; then
    CORRECTNESS_STATUS[smc_stream_opt]="CORRECTNESS_FAIL (run $run: invalid checks value)"
  else
    expected=$((checks * 7))
    if [[ "$bytes" != "$expected" ]]; then
      CORRECTNESS_STATUS[smc_stream_opt]="CORRECTNESS_FAIL (run $run: bytes_compared=$bytes, expected=$expected)"
    fi
  fi
done

echo "- out_of_range: $(get_value "smc_stream_opt|1|out_of_range")" >> "$OUTPUT_FILE"
echo "- fallback_count: $(get_value "smc_stream_opt|1|fallback_count")" >> "$OUTPUT_FILE"
echo "- bytes_compared: $(get_value "smc_stream_opt|1|bytes_compared")" >> "$OUTPUT_FILE"
checks1=$(get_value "smc_stream_opt|1|checks")
if [[ "$checks1" =~ ^[0-9]+$ ]]; then
  echo "- checks * 7: $((checks1 * 7))" >> "$OUTPUT_FILE"
else
  echo "- checks * 7: missing" >> "$OUTPUT_FILE"
fi
echo "" >> "$OUTPUT_FILE"
echo "**Result: ${CORRECTNESS_STATUS[smc_stream_opt]}**" >> "$OUTPUT_FILE"

echo "" >> "$OUTPUT_FILE"
echo "## Decision" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

stream_median=$(median_field "smc_stream_opt" "avg_render_ms")
batch_median=$(median_field "smc_batch" "avg_render_ms")

if [[ "$stream_median" == "missing" ]] || [[ "$batch_median" == "missing" ]]; then
  DECISION_STATUS[stream]="DECISION_FAIL (missing median values)"
elif [[ "${PARSE_STATUS[smc_stream_opt]:-PARSE_FAIL}" != "PARSE_PASS" ]] || [[ "${PARSE_STATUS[smc_batch]:-PARSE_FAIL}" != "PARSE_PASS" ]]; then
  DECISION_STATUS[stream]="DECISION_FAIL (parse failure)"
elif [[ "${RUN_STATUS[smc_stream_opt]:-RUN_FAIL}" != "RUN_PASS" ]] || [[ "${RUN_STATUS[smc_batch]:-RUN_FAIL}" != "RUN_PASS" ]]; then
  DECISION_STATUS[stream]="DECISION_FAIL (run failure)"
elif [[ "${CORRECTNESS_STATUS[smc_stream_opt]}" != "CORRECTNESS_PASS" ]]; then
  DECISION_STATUS[stream]="DECISION_FAIL (correctness failed: ${CORRECTNESS_STATUS[smc_stream_opt]})"
elif awk "BEGIN { exit !($stream_median <= $batch_median) }"; then
  DECISION_STATUS[stream]="DECISION_PASS (${stream_median}ms <= ${batch_median}ms)"
else
  DECISION_STATUS[stream]="DECISION_FAIL (${stream_median}ms > ${batch_median}ms)"
fi

echo "- Stream optimized median: $stream_median ms" >> "$OUTPUT_FILE"
echo "- Packed batch median: $batch_median ms" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"
echo "**Final Verdict: ${DECISION_STATUS[stream]}**" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"
echo "---" >> "$OUTPUT_FILE"
echo "*Raw logs in ${LOG_BASE}/*" >> "$OUTPUT_FILE"