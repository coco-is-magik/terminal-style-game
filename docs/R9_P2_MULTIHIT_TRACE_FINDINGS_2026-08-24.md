# R9 P2 Findings — Bounded Multi-Hit Tracing — 2026-08-24

## Outcome

**P2 complete; unconditional full-screen collection is a no-go. Stop before P3
pending confirmation.** The gated prototype proves that the existing prepared
column can produce deterministic ordered hit lists without repeating DDA work.
It also disproves the hypothesis that collecting every possible geometry layer
for every screen sample is a small enough constant factor: sampling-only N=1–4
all exceed the repository's entire 6 ms shipping frame budget.

This is research evidence for Review H, not shipping architecture. Existing
single-hit rendering, selection, highlighting, and scene behavior are unchanged.

## Prototype boundary

- `heightfield_trace_collect` and its types exist only under
  `R9_OPTICAL_RESEARCH=1`.
- Output is a fixed four-element `R9HeightfieldHitList`; no allocation occurs.
- Caller capacity is bounded to 1–4.
- The collector walks `HeightfieldTraceColumn`'s already-prepared interval arrays
  once; it performs no DDA preparation or stepping of its own.
- Geometry and optics remain separate. The collector does not inspect P1 opacity,
  transmission, reflectivity, or blocking values.
- Missing relevant floor/ceiling remains a terminal opening under R8's one-
  traversable-interval contract.
- Invalid calls reject without modifying caller output.

## Ordering contract demonstrated

For each near-to-far prepared interval:

1. collect a horizontal floor/ceiling intersection inside the interval, if one
   exists;
2. then collect a generated boundary at the interval exit, if one exists;
3. stop at a missing relevant horizontal surface and report `terminal_opening`;
4. retain at most N hits and scan only until one additional hit proves
   `truncated`.

P2 intentionally does not stop at an apparently opaque wall because geometry
cannot infer optical behavior. A glass-pane/rear-wall fixture therefore uses an
explicit opening after the rear wall to terminate the geometric ray.

## Correctness fixtures

Focused runner: `tests/test_r9_multihit_trace.c`, 6/6 pass.

| Fixture | Result |
|---|---|
| Front pane material 10 before rear wall material 20 | Two ordered wall hits at increasing distance |
| Alternating raised/default floor planes | Multiple ordered floor/discontinuity hits |
| Missing floor in camera cell | Zero hits, terminal opening reported |
| Five wall boundaries with capacity four | Four retained hits, truncation reported |
| Capacity one vs existing prepared single-hit API | Every `HeightfieldHit` field equal |
| Null/out-of-range/capacity-invalid calls | Rejected transactionally |

The full-screen benchmark also proves exact N=1 checksum parity:
`7030799798860159300` for both the shipping prepared single-hit path and the
capacity-one collector. Checksums for N=2–4 are deterministic and distinct:

| Capacity | Checksum |
|---:|---:|
| 1 | 7030799798860159300 |
| 2 | 5203727586689278859 |
| 3 | 9794927642847536411 |
| 4 | 16101309613481390685 |

## Benchmark method

Command: `make benchmark-r9-multihit-trace`.

- Scenario: the existing 20x12 raised-height geometry used by the surface
  benchmark, sampled at 260x160 = 41,600 screen samples per frame.
- Scope: **sampling only**. All 260 DDA columns are prepared before timing, which
  directly tests P2's hypothesis about reuse of cached intervals.
- 10 untimed warm-up frames per path; 200 measured frames per path.
- Strict default `-O2 -Wall -Wextra -Wpedantic -Werror` build.
- Three consecutive trials; no best-run selection.

### Measured ms/frame

| Path | Trial 1 | Trial 2 | Trial 3 | Stable observation |
|---|---:|---:|---:|---|
| Existing single hit | 5.055506 | 6.813468 | 4.988635 | One host-noise outlier; other trials ≈5.0 ms |
| Collect N=1 | 12.905191 | 12.798077 | 12.903880 | ≈12.9 ms |
| Collect N=2 | 14.574266 | 14.353794 | 14.467319 | ≈14.5 ms |
| Collect N=3 | 17.708892 | 17.600883 | 17.822563 | ≈17.7 ms |
| Collect N=4 | 17.623276 | 17.659573 | 17.633762 | ≈17.6 ms |

Trial 1 per-sample costs were 121.527 ns existing single hit and 310.221,
350.343, 425.695, and 423.636 ns for N=1–4. Collector measurements are stable
across trials; the ratio is noisy because its denominator contains the single-hit
outlier, so absolute times are the decision evidence.

This microbenchmark is intentionally not the shipping surface-render gate. Its
sampling-only single-hit baseline already approaches that whole gate because it
hashes every result and runs outside renderer batching. Relative collector cost
and the absolute failure to fit 6 ms are still decisive; adding preparation and
compositing cannot make unconditional collection cheaper.

## Hypothesis result and cap recommendation

**Rejected:** “collect all layers for every screen sample” is not affordable.
Even N=1 with exact truncation detection scans beyond the nearest hit and costs
about 12.9 ms. N=2 costs about 14.5 ms. N=3/4 cost about 17.6–17.8 ms before any
compositing.

**Review-H input:** retain four as the correctness/storage cap for research, but
do not invoke full collection unconditionally. The next design needs selective
continuation:

1. use the existing nearest-hit path first;
2. resolve that hit's P1 optical semantics;
3. continue the prepared interval scan only when transmission requires another
   layer;
4. stop at the first optically terminal hit, opening, or four-layer cap.

N=3 and N=4 have nearly identical timings because most samples in this scenario
do not expose a fourth hit. That supports four as a bounded worst-case cap but
does not establish a quality preset; D3 remains open through P3/P4/RQ6.

## Important design consequence for P3

P3 cannot be a compositor pasted after unconditional P2 collection. It must
prototype the handoff between optical resolution and selective trace
continuation, or explicitly benchmark a prefiltered synthetic list while leaving
the continuation redesign as a Review-H blocker. Geometry must not infer opacity,
and optical code must not duplicate DDA.

## Verification evidence

- Focused strict runner: 6/6 pass.
- Gate-off strict translation-unit compile: pass.
- Ordinary strict application build with research symbols absent: pass.
- Strict optimized aggregate `make -j2 check`: pass, including P1 and P2.
- Sequential clean ASan/LeakSanitizer and UBSan full suites: pass without
  diagnostics. A parallel attempt was discarded because both Make targets clean
  the same build directory.
- Shipping surface benchmark raised path: 5.140533 ms; stability: 5.247474 ms;
  both deterministic and below 6 ms.
- Exact flat-default framebuffer parity and occluded-decal/raised-height checksum
  equality remain intact.

## Limitations

- The benchmark measures prepared sampling, not DDA preparation, palette work,
  compositing, or SDL rendering.
- No sparse optical lookup from P1 is integrated; that access cost remains open.
- “Glass” is represented by geometry fixtures only. P2 does not apply optical
  semantics.
- No quality-preset decision is made.
- No shipping API or behavior changes under ordinary compilation.

## Reproduction

```sh
make build/test-r9-multihit-trace
./build/test-r9-multihit-trace
make benchmark-r9-multihit-trace
```

Next increment, only after confirmation: P3 terminal glyph/color compositor
research, incorporating the selective-continuation constraint above.