# SMC Renderer Integration Report

## Summary

The Self-Modifying Calculator (SMC) project was integrated into the terminal-style game as a generated-code optimization path for repeated renderer math. The integration builds cleanly, the generated-code path is exercised on every frame, and correctness is preserved (no fallback calls, no output changes beyond normal floating-point tolerance). However, **no measurable hot-path speedup was observed** in the current benchmark configuration.

## What was optimized

Four high-frequency scalar expressions in `src/raycast.c` were replaced with generated SMC dispatch calls:

1. **Ray angle per column**: `atan(camera_x * tan(fov / 2.0))`
   - Called once per screen column per frame.
2. **Fisheye correction**: `dist * cos(ray_angle - cam_angle)`
   - Called once per column that hits a wall.
3. **Ceiling/floor true distance**: `currentDist / cos(ray_angle - cam_angle)`
   - Called once per ceiling/floor pixel per column.
4. **Light billboard screen X**: `tan(angle_diff) / tan(fov / 2.0)`
   - Called once per visible light source per frame.

These were selected because they are repeated across frames/pixels and involve trigonometric calls that SMC can emit as plain C functions.

## Files changed

- `src/smc_render_opt.h` / `src/smc_render_opt.c` — adapter layer.
- `src/raycast.c` — replaced 4 hot expressions with adapter calls.
- `src/app.c` — SMC init/shutdown and benchmark stats reporting.
- `Makefile` — `USE_SMC=1` build path, SMC runtime + generated code linking.
- `scripts/generate-smc-renderer.lisp` — project-specific SMC warm-cache + C generator.
- `build/smc_generated.c` — generated dispatch table (re-generated at build time when `USE_SMC=1`).

## Build instructions

```bash
# Baseline (no SMC)
make clean && make
./build/ascii-fps --benchmark-raycast 5

# SMC-enabled build
make clean && make USE_SMC=1
./build/ascii-fps --benchmark-raycast 5
```

The SMC-enabled build links `vendor/src/smc/src/c/smc_runtime_stub.c`, `vendor/src/smc/src/c/smc_generated_runtime.c`, and the generated `build/smc_generated.c`. SBCL is required only at build time to regenerate `build/smc_generated.c`; the final binary has no SBCL runtime dependency.

## Expression IDs

The adapter uses the stable macro IDs emitted by the generator:

- `SMC_EXPR_EXPR_EXPR_ATAN_LPAREN__LPAREN_X__MUL__TAN_LPAREN__LPAREN_Y__DIV__2_RPAREN__RPAREN__RPAREN__RPAREN` (arity 2)
- `SMC_EXPR_EXPR_EXPR__LPAREN_X__MUL__COS_LPAREN__LPAREN_Y__MINUS__Z_RPAREN__RPAREN__RPAREN` (arity 3)
- `SMC_EXPR_EXPR_EXPR__LPAREN_X__DIV__COS_LPAREN__LPAREN_Y__MINUS__Z_RPAREN__RPAREN__RPAREN` (arity 3)
- `SMC_EXPR_EXPR_EXPR__LPAREN_TAN_LPAREN_X_RPAREN___DIV__TAN_LPAREN__LPAREN_Y__DIV__2_RPAREN__RPAREN__RPAREN` (arity 2)

## Correctness checks

- The adapter falls back to the original C expression if `smc_call_double` returns an error.
- Benchmark runs report `fallback=0`, `arity_errors=0`, `invalid_ids=0`, confirming the generated path is taken for every call.
- The renderer still produces the same deterministic frame output; no visual regressions were observed.

## Benchmark results

All runs used `--benchmark-raycast 5` with a fixed camera (deterministic scene).

### Baseline (no SMC)

| run | avg_render_ms | worst_render_ms |
|-----|---------------|-----------------|
| 1   | 8.58          | 16.22           |
| 2   | 8.50          | 12.44           |
| 3   | 8.60          | 12.54           |
| 4   | 8.42          | 10.65           |
| 5   | 8.57          | 12.36           |
| 6   | 8.74          | 12.22           |
| 7   | 8.57          | 12.14           |
| 8   | 8.60          | 11.83           |
| 9   | 8.69          | 12.31           |
| 10  | 8.83          | 11.96           |
| 11  | 8.58          | 11.94           |
| 12  | 8.77          | 12.31           |
| 13  | 8.83          | 10.87           |

Median avg: ~8.58 ms

### SMC-enabled

| run | avg_render_ms | worst_render_ms | SMC calls |
|-----|---------------|-----------------|-----------|
| 1   | 8.33          | 12.23           | 6,613,967 |
| 2   | 8.40          | 11.78           | 6,257,292 |
| 3   | 8.71          | 13.44           | 6,269,639 |
| 4   | 8.29          | 11.65           | 6,671,355 |
| 5   | 8.75          | 12.01           | 6,255,292 |
| 6   | 8.78          | 12.42           | 6,449,191 |
| 7   | 8.31          | 11.07           | 6,542,232 |
| 8   | 8.60          | 9.98            | 6,413,109 |
| 9   | 8.97          | 11.34           | 6,169,210 |
| 10  | 8.63          | 12.09           | 6,341,374 |
| 11  | 8.52          | 11.11           | 6,484,844 |
| 12  | 8.36          | 11.92           | 6,642,661 |
| 13  | 8.50          | 17.13           | 6,484,844 |

Median avg: ~8.52 ms

### Interpretation

The difference between baseline and SMC median average render time is well within run-to-run noise (~1%). The generated-code path is exercised millions of times per run with zero fallbacks, but the selected expressions are too cheap relative to the rest of the frame (SDL draw, grid updates, lighting, asset lookups) for the dispatch savings to be measurable.

## Why speedup was not achieved

1. **Hot path is not math-bound**: The benchmark measures `renderer_draw()` time, which is dominated by SDL texture upload / glyph atlas rendering, not the trigonometric expressions.
2. **Expressions are already cheap**: `atan`, `tan`, and `cos` on modern CPUs are fast; the SMC dispatch removes no work of significance.
3. **No vector/matrix work**: The renderer's heavy operations (DDA stepping, grid writes, decal projection) were not candidates for scalar expression replacement.
4. **Benchmark noise**: ~5–10% variance between runs is larger than any potential savings from the replaced expressions.

## What remains to improve

- Profile the renderer to find the actual CPU bottleneck (likely `renderer_draw` / glyph atlas).
- If a math-heavy kernel is identified, generate a larger fused expression (e.g., combine ray-angle + fisheye + true-distance into one SMC call) to amortize dispatch overhead.
- Consider SMC for the stress-pattern mode, which is pure CPU math, rather than the raycast mode where SDL dominates.
- Fix `scripts/generate-smc-renderer.lisp` so it reliably emits the renderer expressions when invoked via `sbcl --script` (currently the inline `--eval` invocation is used as a workaround).

## Acceptance criteria status

- [x] Project builds cleanly with `USE_SMC=1` and without.
- [x] Renderer still produces correct output.
- [x] Hot path does not call `smc_eval_*`.
- [x] Generated-code path is exercised (millions of calls, zero fallbacks).
- [x] Benchmarks show either a measurable speedup or a clear technical explanation for why speedup was not achieved.
