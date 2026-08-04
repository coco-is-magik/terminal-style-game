# Handoff: Benchmark reverify on yotsubox2

**Date**: 2026-07-26  
**Task**: Reverify baseline, custom dirty cells, and SMC benchmarks on new device  
**Status**: Main mode suite **complete and verified**. Dynamic scenario matrix **incomplete**.

## Current understanding

Primary comparison is six compile-time renderer modes on a fixed idle-ish raycast benchmark (`--benchmark-raycast 5`), 160×260 grid, PROFILE_FRAME=1.

Authoritative new-device writeup:

- `docs/benchmarks/2026-07-26-yotsubox2-reverify.md`

Raw per-mode logs:

- `docs/smc_benchmark_logs/{baseline,dirty_cells,smc_indexed,smc_batch,smc_stream_opt,smc_stream_base}/`

Script-generated dump (main modes good; scenario section truncated):

- `docs/SMC_BENCHMARK_RESULTS.md`

## Work completed

1. Confirmed vendor libs + SMC present; built baseline with PROFILE_FRAME=1.
2. Smoke-tested `--benchmark-raycast 2` successfully on DISPLAY=:0.
3. Ran `scripts/smc_benchmark.sh` through all six main modes (3 runs each).
4. Recorded medians, correctness, stream-vs-batch decision, old-vs-new comparison.
5. Stopped scenario-matrix completion after hangs/X11 failures per user direction (keep simple).

## Verified medians (yotsubox2, Ryzen 9 7950X)

| mode | avg_ms | vs baseline |
|------|--------|-------------|
| baseline | 2.56 | — |
| dirty_cells | 1.06 | 2.42× faster |
| smc_indexed | 1.44 | 74.7% of dirty gain |
| smc_batch | 1.12 | 96.0% of dirty gain |
| smc_stream_opt | 1.00 | fastest; CORRECTNESS_PASS |
| smc_stream_base | 1.25 | 87.3% of dirty gain |

- All modes: BUILD_PASS / PARSE_PASS / RUN_PASS / result=ideal  
- DECISION_PASS: stream_opt 1.00 ≤ batch 1.12  
- ~3–5× faster absolute times than 2026-07-17 device  

## Incomplete / failed

| Attempt | Result | Lesson |
|---------|--------|--------|
| Full scenario matrix inside smc_benchmark.sh | Hung/interrupted at camera stream (X11 BadValue) | Scenario path less stable under agent terminal |
| Multi-scenario shell loops / complete_scenario_matrix.sh | Terminal freeze | Too complex; one mode per command |
| SDL_VIDEODRIVER=dummy | Runs but SDL ~4ms inflated | Not comparable to real display |

## Next actions (if needed)

1. **Optional**: finish scenario matrix one command at a time, e.g.  
   `make clean && make PROFILE_FRAME=1 USE_DIRTY_CELLS=1 && ./build/ascii-fps --benchmark-scenario rotate --frames 600`  
2. Do **not** re-run the full hung multi-loop approach without fixing XDG_RUNTIME_DIR / display stability first.
3. README was **not** updated (no user-facing feature change; metrics only).

## Recovery

- Main numbers are already in `docs/benchmarks/2026-07-26-yotsubox2-reverify.md`.
- Re-check one mode: build with flag, run `--benchmark-raycast 5`, compare to table above.
- LICENSE untouched.
