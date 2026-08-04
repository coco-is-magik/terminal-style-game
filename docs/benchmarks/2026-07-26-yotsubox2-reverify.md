# Benchmark Reverify — yotsubox2 (Ryzen 9 7950X)

**Date**: 2026-07-26T08:50:34-04:00  
**Host**: yotsubox2  
**OS**: Linux 6.15.4-gentoo x86_64  
**CPU**: AMD Ryzen 9 7950X 16-Core (32 threads)  
**RAM**: 125 GiB  
**Git commit**: a60c7e7 (dev)  
**SMC submodule**: 3783ae9  
**Grid**: 160×260 (41600 cells)  
**Method**: `scripts/smc_benchmark.sh` — warmup 2s discarded, 3× measured runs of `--benchmark-raycast 5`, medians  
**Display**: DISPLAY=:0 (real X11)  
**Build**: `PROFILE_FRAME=1` + mode flag, release-style gcc flags from Makefile  

Raw logs: `docs/smc_benchmark_logs/<mode>/`  
Full script output: `docs/SMC_BENCHMARK_RESULTS.md` (main modes section only is authoritative)

## Status

| Item | Status |
|------|--------|
| baseline / dirty_cells / SMC modes | **Verified** |
| Correctness (stream opt) | **CORRECTNESS_PASS** |
| Stream vs batch decision | **DECISION_PASS** |
| Dynamic scenario matrix | **Incomplete** — X11 instability mid-run; not required for primary mode comparison |

## Median Results (this device)

| mode | avg_render_ms | worst (median run) | raycast_ms | dirty/raster_ms | sdl_ms | skip_rate | result |
|------|---------------|--------------------|------------|-----------------|--------|-----------|--------|
| baseline | **2.56** | 4.09 | 0.848 | 1.798 | 0.766 | 0.0% | ideal |
| dirty_cells | **1.06** | 1.70 | 0.859 | 0.235 | 0.824 | 100.0% | ideal |
| smc_indexed | **1.44** | 1.90 | 0.828 | 0.602 | 0.838 | 100.0% | ideal |
| smc_batch | **1.12** | 1.64 | 0.837 | 0.094 | 0.847 | 100.0% | ideal |
| smc_stream_opt | **1.00** | 1.56 | 0.806 | 0.008 | 0.806 | 100.0% | ideal |
| smc_stream_base | **1.25** | 2.16 | 0.826 | 0.008 | 0.831 | 100.0% | ideal |

### Raw avg_render_ms runs

| mode | run1 | run2 | run3 | median |
|------|------|------|------|--------|
| baseline | 2.56 | 2.60 | 2.54 | **2.56** |
| dirty_cells | 1.06 | 1.07 | 1.06 | **1.06** |
| smc_indexed | 1.42 | 1.44 | 1.44 | **1.44** |
| smc_batch | 1.12 | 1.12 | 1.11 | **1.12** |
| smc_stream_opt | 1.00 | 0.99 | 1.02 | **1.00** |
| smc_stream_base | 1.27 | 1.25 | 1.25 | **1.25** |

All six modes: BUILD_PASS, PARSE_PASS, RUN_PASS. Every measured run reported `"result": "ideal"`.

## Speedup vs baseline (this device)

```text
custom_gain   = 2.56 - 1.06 = 1.50 ms
indexed_gain  = 2.56 - 1.44 = 1.12 ms   → 1.12/1.50 = 74.7% of custom
batch_gain    = 2.56 - 1.12 = 1.44 ms   → 1.44/1.50 = 96.0% of custom
stream_gain   = 2.56 - 1.00 = 1.56 ms   → 1.56/1.50 = 104.0% of custom (faster than dirty)
stream_base   = 2.56 - 1.25 = 1.31 ms   → 1.31/1.50 = 87.3% of custom
```

### Ranking (fastest → slowest avg)

1. **smc_stream_opt** — 1.00 ms  
2. **dirty_cells** — 1.06 ms  
3. **smc_batch** — 1.12 ms  
4. **smc_stream_base** — 1.25 ms  
5. **smc_indexed** — 1.44 ms  
6. **baseline** — 2.56 ms  

## Correctness (smc_stream_opt)

- out_of_range: 0  
- fallback_count: 0  
- bytes_compared == checks × 7: 185494400 == 26499200 × 7  

**CORRECTNESS_PASS**

## Decision (stream vs batch)

- Stream optimized median: 1.00 ms  
- Packed batch median: 1.12 ms  

**DECISION_PASS (1.00 ≤ 1.12)**

## Comparison to previous device (2026-07-17)

Previous environment path referenced `/bigdisk/programming/C/...` (older machine). Medians from that run:

| mode | old avg_ms | new avg_ms | speedup factor |
|------|------------|------------|----------------|
| baseline | 8.59 | 2.56 | 3.36× |
| dirty_cells | 5.05 | 1.06 | 4.76× |
| smc_indexed | 6.25 | 1.44 | 4.34× |
| smc_batch | 5.20 | 1.12 | 4.64× |
| smc_stream_opt | 5.19 | 1.00 | 5.19× |
| smc_stream_base | 5.84 | 1.25 | 4.67× |

### Relative ordering preserved?

| Claim | Old device | New device | Preserved? |
|-------|------------|------------|------------|
| dirty ≪ baseline | yes (5.05 vs 8.59) | yes (1.06 vs 2.56) | yes |
| batch near dirty | yes (5.20 vs 5.05) | yes (1.12 vs 1.06) | yes |
| indexed slower than dirty/batch | yes | yes | yes |
| stream_opt ≤ batch | yes (5.19 ≤ 5.20) | yes (1.00 ≤ 1.12) | yes |
| stream_opt vs dirty | slightly slower old | **faster** new | ordering flipped |
| all modes fail 120fps spare budget | RUN_FAIL all | **ideal** all | absolute perf changed |

On the old device every mode reported `fail_performance` (negative min_spare_ms). On yotsubox2 every mode is `ideal` with positive spare.

## Partial scenario matrix (not complete)

Completed before X11 crash:

| scenario | dirty_cells | smc_batch | smc_stream_opt |
|----------|-------------|-----------|----------------|
| idle | 0.91 ms ideal | 0.95 ms ideal | 0.89 ms ideal |
| camera | 0.98 ms ideal | 0.99 ms ideal | RUN_FAIL (X11 BadValue) |
| rotate / flicker / ui / fullchange | not run | not run | not run |

Scenario matrix is **unverified** beyond idle + partial camera. Do not treat it as complete.

## Environment notes

1. `error: XDG_RUNTIME_DIR is invalid or not set` appears in logs; did not block main-mode benchmarks.  
2. Long multi-mode shell loops and scenario matrix hangs froze the agent terminal; prefer one mode per command.  
3. `SDL_VIDEODRIVER=dummy` runs but inflates SDL/update (~4 ms) — do not compare dummy numbers to DISPLAY=:0 numbers.  

## How to re-run one mode (simple)

```bash
make clean
make PROFILE_FRAME=1                    # baseline
./build/ascii-fps --benchmark-raycast 5

make clean
make PROFILE_FRAME=1 USE_DIRTY_CELLS=1
./build/ascii-fps --benchmark-raycast 5

make clean
make PROFILE_FRAME=1 USE_SMC_STREAM_STATE_TRACKER=1
./build/ascii-fps --benchmark-raycast 5
```

Or full main suite only (no scenario matrix edit needed if scenarios hang):

```bash
bash scripts/smc_benchmark.sh
```

## Conclusion

On **AMD Ryzen 9 7950X / Gentoo (yotsubox2)**:

1. **Baseline** median **2.56 ms** (ideal).  
2. **Custom dirty cells** median **1.06 ms** — ~2.4× faster than baseline.  
3. **SMC stream opt** median **1.00 ms** — fastest mode; correctness pass; beats batch.  
4. **SMC batch** median **1.12 ms** — preserves **96%** of custom dirty speedup.  
5. **SMC indexed** median **1.44 ms** — preserves **75%** of custom dirty speedup.  
6. Absolute times are ~3–5× faster than the 2026-07-17 device; relative ranking of dirty/batch/indexed/baseline is preserved; stream_opt is now slightly ahead of dirty_cells.
