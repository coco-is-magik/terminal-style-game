# R9 P4 Findings — Bounded Single-Bounce Mirrors — 2026-08-24

## Outcome

**P4 complete; per-cell reflected prepared tracing is a no-go, even at 10%
coverage. Stop before RQ5/RQ6 synthesis pending confirmation.** The gated
prototype proves correct cardinal and oblique wall reflection, reflected wall
hits, explicit darkness fallback for reflected openings/misses, and a structural
one-bounce/no-recursion rule. It also measures approximately 1.8–1.85 ms overhead
at only 10% mirror coverage and 18.4–18.5 ms at 100% in stable trials, before
reflected material sampling or P3 composition.

This remains Review H research evidence. No shipping renderer, persisted scene,
material registry, selection, lighting, or editor behavior changed.

## Prototype boundary

- `src/r9_mirror_trace.h/.c` is active only with
  `R9_OPTICAL_RESEARCH=1`.
- Only `HEIGHTFIELD_HIT_WALL` may be a mirror. R8 wall faces are vertical;
  floor/ceiling mirrors are outside P4.
- Any nonzero P1 reflectivity makes the research surface mirror-eligible. P4 does
  not blend partial reflection strength; that would consume P3 composition rules.
- Reflection uses the incoming prepared column direction and hit side:
  - side 0, Y-aligned wall: negate direction X;
  - side 1, X-aligned wall: negate direction Y.
- Incoming vectors are normalized before reflection.
- The reflected origin is the mirror hit X/Y plus `1e-6` world units along the
  reflected direction to avoid immediate self-intersection.
- Reflected eye Z is the mirror hit Z. The original horizon offset and screen row
  are retained.
- A one-column reflected camera has center coordinate zero, so its prepared ray is
  exactly the reflected camera angle with correction 1. The existing heightfield
  DDA and bounded-surface rules are reused; no second arbitrary-ray tracer exists.
- A reflected miss or terminal opening returns explicit
  `R9_MIRROR_RESULT_DARKNESS_FALLBACK`.

## One-bounce rule

`R9_MIRROR_MAX_BOUNCES` is exactly 1. The sampling API accepts an incoming column,
not a prior mirror result or bounce counter, and always returns `bounce_count=1`
for successful sampling. If the reflected hit is itself mirror-like, it is
returned as ordinary terminal geometry. The prototype has no recursive call or
API path that can request another bounce.

Entities are intentionally absent because they arrive in R11. P4 does not claim
entity reflections.

## Reflection math

For normalized incoming direction `(dx,dy)`:

```text
side 0: reflected = (-dx,  dy)
side 1: reflected = ( dx, -dy)
```

This is the axis-aligned specialization of `r = d - 2(d·n)n`. Face-normal sign
does not change the reflected result for the same plane.

## Correctness fixtures

Focused runner: `tests/test_r9_mirror_trace.c`, 6/6 pass.

| Fixture | Result |
|---|---|
| Cardinal side-0 and side-1 vectors | Correct component negated |
| Oblique `(2,2)` for both sides | Normalized `±1/sqrt(2)` components |
| East ray, mirror material 10, west wall material 20 | Reflected wall hit material 20 at expected cell |
| Floor-facing row reflected into removed floor | Explicit darkness fallback, no hit |
| Reflected wall also uses mirror material 10 | Returned as terminal hit, bounce count remains 1 |
| Non-wall, zero reflectivity, invalid vectors/inputs | Rejected transactionally |

The opening fixture captures the incoming mirror hit first, then removes the
floor immediately on the reflected side. This proves that the reflected tracer,
not the incoming path, applies the opening fallback.

## Coverage benchmark method

Command: `make benchmark-r9-mirror-trace`.

- 20x12 flat-default heightfield, 260x160 = 41,600 samples per frame.
- One incoming prepared center column hits mirror material 10; the reflected ray
  hits wall material 20.
- Selected 10/25/50/100% samples each execute the real P4 operation: vector
  reflection, reflected camera construction, one-column
  `heightfield_trace_prepare_column`, and `heightfield_trace_prepared_sample`.
- Remaining samples execute only the same result-hash loop using a fixed fallback
  sample. Coverage overhead is measured against 0%.
- 5 untimed warm-up frames and 100 measured frames per coverage.
- Three consecutive reported trials after the initial exploratory run; no
  best-run selection.
- Strict default `-O2 -Wall -Wextra -Wpedantic -Werror` build.
- Palette sampling, P3 composition, SDL rasterization, and entities are excluded.

### Measured ms/frame

| Coverage | Trial 1 | Trial 2 | Trial 3 | Stable overhead observation |
|---:|---:|---:|---:|---:|
| 0% baseline | 1.475496 | 0.738387 | 0.735704 | Trial 1 host-noise outlier; trials 2–3 ≈0.74 ms |
| 10% | 4.276084 | 2.588254 | 2.538641 | +1.802937–1.849867 ms in trials 2–3 |
| 25% | 5.369236 | 5.307694 | 5.260584 | +4.524880–4.569306 ms in trials 2–3 |
| 50% | 9.950330 | 9.883443 | 10.215379 | +9.145056–9.479675 ms in trials 2–3 |
| 100% | 19.378202 | 19.106888 | 19.241042 | +18.368501–18.505338 ms in trials 2–3 |

Stable reflected-sample costs are approximately 433–456 ns. Trial 1's baseline
and 10% path experienced host noise, while 25–100% and all checksums remained
consistent. The earlier exploratory run measured the same linear shape at roughly
497–532 ns per reflected sample.

Deterministic checksums across all reported trials:

| Coverage | Checksum |
|---:|---:|
| 0% | 7389715055198195715 |
| 10% | 9809526082360161027 |
| 25% | 8351295799531547779 |
| 50% | 8207368896826018051 |
| 100% | 511591132268614915 |

## Hypothesis result

**Correctness supported:** a vertical-wall single bounce can reuse the existing
heightfield tracer exactly, with bounded epsilon, explicit fallback, and no
recursion.

**Performance hypothesis rejected for per-cell preparation:** the shipping raised
path is currently near 5.2 ms against a 6 ms budget. Stable 10% mirror overhead
alone is about 1.8 ms, before reflected palette sampling and composition. Therefore
even “low” 10% screen coverage does not fit available headroom under this design.

## Review H input

- Keep the hard one-bounce rule.
- Keep explicit darkness fallback for reflected miss/opening unless product review
  chooses a different visible background.
- Do not prepare a complete reflected column independently for every mirror cell.
- Investigate coarser reuse, especially reflected XY interval preparation per
  mirror-covered screen column, while preserving per-row reflected Z correctness.
- Require an explicit mirror coverage/quality policy and benchmark it with P2/P3
  selective continuation; D3 remains open until RQ6 synthesis.
- Reflectivity magnitude and mirror/translucency mixing need an explicit P3-layer
  rule before implementation planning.

The prototype itself does not perform this redesign because Review H must precede
architectural commitment.

## Verification evidence

- Focused strict runner: 6/6 pass.
- Gate-off strict translation-unit compile: pass.
- Ordinary strict application build with research symbols absent: pass.
- Strict optimized aggregate `make -j2 check`: pass, including P1–P4 runners.
- Shipping benchmark raised path: 5.134157 ms, deterministic and below 6 ms.
- The first shipping stability run measured 6.097629 ms and failed narrowly;
  three immediate retries passed at 4.901847, 4.852817, and 4.876414 ms with
  identical checksums. P4 is compiled out of this target, and repeated evidence
  records the first result as host variance rather than a persistent regression.
- Exact flat-default framebuffer parity and occluded-decal/raised-height checksum
  equality remain intact.
- Sequential clean ASan/LeakSanitizer and UBSan full suites: pass without
  diagnostics, including P4 6/6.

## Limitations

- Vertical walls only; no horizontal mirrors.
- No reflected decals, highlights, lights, sprites, or entities.
- No partial-reflectivity blend with the direct surface.
- The benchmark repeats reflected XY preparation per cell by design to test and
  reject that straightforward implementation.
- No full shipping trace/material/compositor integration.
- RQ5 draw ordering and RQ6 combined budget/preset synthesis remain open.

## Reproduction

```sh
make build/test-r9-mirror-trace
./build/test-r9-mirror-trace
make benchmark-r9-mirror-trace
```

Next increment, only after confirmation: RQ5 ordering policy and RQ6 measured
budget/preset synthesis, followed by Review H preparation.