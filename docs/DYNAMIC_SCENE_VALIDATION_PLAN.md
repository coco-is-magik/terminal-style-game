# Dynamic Scene Validation Implementation Plan

> **Completed historical gate:** The instruction below not to make stream the
> default applied during validation. Validation later passed, and stream tracking
> is now the default when no alternate or explicit no-tracker mode is selected.
> Deferred methodology improvements are tracked in `docs/TODO.md`.

## Objective
Create a deterministic second benchmark suite to validate `USE_SMC_STREAM_STATE_TRACKER=1` against `USE_SMC_BATCH_STATE_TRACKER=1` and `USE_DIRTY_CELLS=1`. Do not make stream default yet. Do not remove packed batch.

## Files to Modify

| File | Changes |
|------|---------|
| `src/timing.h` | Add `smc_batch_diff_ms` and `smc_stream_diff_ms` to `FrameProfileStats` |
| `src/timing.c` | Initialize and print new fields |
| `src/renderer.c` | Route profile accumulation correctly |
| `src/config.h` | Add `RUN_MODE_BENCHMARK_SCENARIO` enum value |
| `src/app.c` | Implement deterministic scenarios with fixed timestep |
| `scripts/smc_benchmark.sh` | Extend for 54-run matrix |
| `SMC_INTEGRATION_REPORT.md` | Add Dynamic Scene Benchmark Results |

## Benchmark Scenarios (Deterministic)

- `benchmark_dt = 1.0 / 60.0` seconds per frame
- `frames = 600` (10 seconds simulated time)

### Scenario Definitions

**idle**: Fixed camera, static scene, no mutations after warm-up frame

**camera**: In-place yaw rotation at 36 deg/sec for 360 deg sweep over 600 frames

**rotate**: In-place yaw rotation at 144 deg/sec for 4 full rotations (1440 deg)

**flicker**: Deterministic modulation for cells where `((idx * 1103515245u + frame * 12345u) % 100) < 3`

**ui**: Bottom 2-row HUD animation using `row_start = grid_h >= 2 ? grid_h - 2 : 0`

**fullchange**: Every cell changes visibly: `cells[i].bg.r = (frame & 1) ? 0xFF : 0x00`

## Pass/Fail Criteria

Stream passes if ALL true:
1. `out_of_range == 0` in all runs
2. `fallback_count == 0` in all runs
3. `bytes_compared == checks * 7` in all runs
4. `framebuffer_checksum` matches correct modes per scenario
5. `median_stream <= median_batch` for idle, camera, rotate, flicker, ui
6. `median_stream <= median_batch + max(0.1ms, 3%)` for fullchange
7. Any >0.1ms regression vs custom dirty explained

## Status Tracker
 
 - [x] Fix profile labels (timing.h, timing.c, renderer.c)
 - [x] Add RUN_MODE_BENCHMARK_SCENARIO enum
 - [x] Implement scenario CLI parsing
 - [x] Implement 6 deterministic scenarios
 - [x] Extend benchmark script for matrix execution
 - [x] Run benchmarks and generate report
 - [x] Update SMC_INTEGRATION_REPORT.md
 
 ## Result: COMPLETE
 
 All dynamic scene validation benchmarks passed. See `SMC_INTEGRATION_REPORT.md` for results.
