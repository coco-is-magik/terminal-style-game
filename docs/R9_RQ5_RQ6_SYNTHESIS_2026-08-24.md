# R9 RQ5/RQ6 Synthesis — Ordering Policy and Performance Envelope — 2026-08-24

## Status and scope

**Research synthesis complete; ready for Review H, not implementation planning.**
This document combines the measured P1–P4 findings into the RQ5 ordering policy,
RQ6 budget envelope, and D1–D3 recommendations that Review H must evaluate. It
adds no code and commits no shipping architecture.

## Required invariants

- Geometry, player collision, sight interaction, light interaction, opacity,
  transmission, and reflectivity remain independent typed semantics.
- Legacy/default optics preserve current output exactly.
- Layer collection is bounded to four and reflection to one bounce.
- Ordinary opaque samples retain a nearest-hit fast path.
- Farther tracing and optical composition occur only after explicit transmission.
- A layer-cap exhaustion is not a proven opening.
- Reflected miss/opening has an explicit fallback; P4 recommends darkness.
- Current decal ordering and editor/UI precedence remain unchanged unless a later
  reviewed requirement explicitly changes them.

## RQ5 — unified draw-order policy

### Finding: one policy, two composition domains

The current renderer cannot be accurately represented by one flat numeric z-order.
It has two ordered domains:

1. **World-cell domain:** depth-aware world geometry and world-attached overlays
   produce one logical `Cell` per screen location. Editor highlights then annotate
   that world cell.
2. **UI-pixel domain:** the completed world grid is rasterized, then independently
   ordered UI canvases compose into the pixel buffer.

A unified policy is still possible if it explicitly names the domain, visibility
test, sort key, tie-break, and replacement/composition operation.

### Policy table

Rows are listed in execution/dependency order, not as a proposal for one global
integer z value.

| Domain/stage | Content | Visibility/depth key | Tie-break | Operation and current parity |
|---|---|---|---|---|
| World geometry | Opening fallback, floor, ceiling, generated discontinuity, wall | Near-to-far ray intersections; nearest optically terminal hit ends selective collection | P2 interval order: horizontal intersection before same-interval boundary | Future optical list composes far-to-near under P3. Current opaque path emits nearest hit exactly. |
| Reflected world | One reflected wall/floor/ceiling/generated-boundary result | Reflected ray depth after a mirror hit | One bounce only; reflected mirror is terminal | Reflection contributes to the mirror surface's optical result. Miss/opening uses explicit darkness fallback. |
| World decals | Wall/floor/ceiling decal glyph | Reject behind nearer world unless it belongs to the visible supporting surface; among decals, smallest perpendicular depth wins | At equal depth within `1e-6`, smallest `source_order = decal creation index, then pattern row-major index` wins | Replaces glyph/foreground and preserves existing background. This exactly describes `render_decals`. |
| Future sprites/entities | Reserved only; no current renderer behavior | Proposed perpendicular depth against the same world frontier | Stable scene instance ID after depth; exact policy requires R11 evidence | Must not be implemented or claimed by R9. Reservation prevents creation-order ambiguity later. |
| Runtime light billboard | Existing gameplay `*` marker | Must be in front of the column wall z-buffer | Runtime light array order; later visible light overwrites earlier at the same cell | Runs after decals, replaces glyph/foreground, preserves background. This is current behavior, though a future unified depth frontier should replace wall-only z testing. |
| Editor hover/member annotation | Hover target and non-primary selected members | Target-specific world visibility test; hidden by nearer geometry | Hover first; selected-set iteration order for non-primary members | Overwrites world cell after complete raycast/decal/light rendering. Uses hover/member style. |
| Editor primary selection | Primary/single selected target | Same target-specific visibility test | Selected target after hover; primary after non-primary members | Selected style wins when hover and selection identify the same target. |
| UI base roles | HUD/editor/footer | Pixel-domain `z_order` ascending | Stable insertion order ascending at equal z | Current values: HUD/editor 10, footer 11. Later composition overwrites earlier pixels. |
| UI menu | Active menu | Pixel-domain z 20 | Stable insertion order | Menu state clears the world grid first; the menu canvas then composes at z 20. |
| UI feedback | Scale feedback | Pixel-domain z 90 | Stable insertion order | Composes above ordinary UI roles. |
| UI crosshair | Fixed-scale editor crosshair | Pixel-domain z 100 | Stable insertion order | Last/highest current role; fixed 100% scale. |

### Optical-world ordering details

For a future selectively collected sample:

1. geometry hits remain near-to-far in the P2 list;
2. P1 semantics decide whether each hit terminates sight;
3. P3 composes only the consumed list, far-to-near;
4. decals attached to a visible surface participate after that surface result,
   preserving today's own-surface exception and nearest-decal rule;
5. mirror reflection is a contribution at the mirror layer, not an unbounded
   second world pass;
6. editor highlights remain annotations above the completed world domain;
7. UI remains a separate pixel domain.

The exact blend between partial reflectivity and direct/transmitted surface color
is unresolved and must be decided by Review H or a required follow-up prototype.

### Current fixture parity

No RQ5 ordering code was added. The policy was checked against existing source and
fixtures:

- `test-decals` preserves front-face rejection, surface projection, glyph order,
  row-major pattern order, and perspective behavior. Source inspection locks the
  nearest-depth/`source_order` overlap rule; there is no dedicated equal-depth
  overlap fixture yet.
- `benchmark-surface-render` proves an occluded decal produces exactly the same
  checksum as the raised-height frame without that visible decal.
- `test-editor-highlight` proves nearer-world occlusion, hover-vs-selection
  precedence, selected light-marker precedence, and selected-set primary behavior.
- `test-ui-compositor` proves ascending z plus stable insertion ordering.
- Application composition confirms world rendering precedes editor highlights,
  which precede pixel-domain UI layers; the crosshair uses z 100.

This reproduces current behavior. Future sprites/entities remain a documented
reservation because R11 has not supplied fixtures or product semantics.

## RQ6 — measured performance envelope

### Shipping budget and current headroom

The hard surface-render budget is 6.000 ms. Latest synthesis-run evidence:

| Shipping path | Measured result | Headroom to 6 ms |
|---|---:|---:|
| Raised benchmark | 5.222781 ms | 0.777219 ms |
| Raised stability | 4.885022 ms | 1.114978 ms |

An earlier unchanged stability run measured 6.097629 ms before three immediate
passes at 4.852817–4.901847 ms. It is retained as host-variance evidence. Budget
decisions should therefore not consume all nominal average headroom.

### Direct prototype measurements

These measurements have different scopes and **must not be added as though they
were one end-to-end frame**. They bound candidate operations.

| Research path | Scope | Direct measured cost | Decision |
|---|---|---:|---|
| P2 existing nearest hit | Prepared sampling, 41,600 samples | ≈5.0 ms, one 6.81 ms noisy trial | Preserve as fast path |
| P2 collect N=1 | Prepared sampling with exact truncation detection | ≈12.9 ms | Reject unconditional collection |
| P2 collect N=2 | Same | ≈14.5 ms | Reject unconditional collection |
| P2 collect N=3 | Same | ≈17.7 ms | Reject unconditional collection |
| P2 collect N=4 | Same | ≈17.6 ms | Reject unconditional collection; four remains correctness cap |
| P3 compose N=1 | Synthetic full-screen cells; overhead over matched hash baseline | +0.793–0.851 ms | Opaque parity proven; ordinary path should bypass compositor |
| P3 compose N=2 | Same | +1.409–1.432 ms | Full-screen use exceeds benchmark headroom |
| P3 compose N=3 | Same | +1.999–2.065 ms | Full-screen use rejected |
| P3 compose N=4 | Same | +2.600–2.613 ms | Full-screen use rejected |
| P4 mirror 10% | Per-cell reflected prepare+sample | +1.803–1.850 ms stable trials | Reject; exceeds current headroom before sampling/composition |
| P4 mirror 25% | Same | +4.525–4.569 ms | Reject |
| P4 mirror 50% | Same | +9.145–9.480 ms | Reject |
| P4 mirror 100% | Same | +18.369–18.505 ms | Reject |

### Derived projections — not acceptance evidence

Linear projections are useful only to show where measurements cannot justify a
preset. Using the latest benchmark headroom of 0.777219 ms:

- P3-only composition would consume that headroom at roughly 55% N=2 coverage,
  38% N=3 coverage, or 30% N=4 coverage. This excludes selective tracing,
  semantic lookup, palette sampling, and integration overhead, so the real limits
  must be lower.
- P4's stable ≈0.18 ms per 1% per-cell mirror coverage reaches the same headroom
  around 4% coverage. This excludes reflected material sampling and P3
  composition.

These are **no-go bounds**, not proposed quality settings.

### Required implementation budget

Review H should require a later implementation plan to reserve:

| Category | Proposed gate |
|---|---:|
| Existing raised shipping path | Must remain ≤6.000 ms total |
| Default opaque optical overhead | Target ≤0.100 ms; hard requirement that legacy/default checksum stays exact |
| Selective translucency + composition | Must fit remaining measured headroom in an end-to-end representative coverage benchmark; no standalone microbenchmark approval |
| Mirrors | Disabled until a coarser-reuse implementation proves end-to-end budget compliance |
| Per-frame allocation | Zero |
| Layer/bounce bounds | Four layers; one bounce |

The 0.100 ms default-overhead target is a proposed engineering budget for Review
H, not a measured result. It protects the already narrow shipping margin and
forces the opaque fast path to remain genuinely cheap.

## D1–D3 research recommendations

### D1 — schema vehicle

**Recommend v6.** Existing v5 block code is fixed-width token machinery, not
unknown optional-section preservation. Amending shipped v5 would make one version
number describe incompatible grammars and permit lossy older-writer round trips.

### D2 — property placement

**Recommend material defaults plus sparse per-cell overrides.** P1 demonstrates
typed inheritance and favorable memory cost. Runtime representation may use a
derived lookup/cache, but authored semantics must remain independent and must not
collapse into one invisible flag.

### D3 — caps and presets

**Recommend hard caps of four optical layers and one mirror bounce. Do not approve
three shipping presets yet.** Current evidence supports:

| Candidate preset | Definition | Status |
|---|---|---|
| Compatibility/default | Existing nearest opaque hit; no mirrors; zero/default optics | **Validated** by exact checksum and shipping gates |
| Selective translucency | Up to 2 or 4 layers only on transmitting samples; no per-cell mirrors | **Shape recommended, performance unverified end-to-end** |
| Mirrors/high quality | Up to 4 layers, one bounce, bounded mirror coverage | **Not admissible:** current per-cell mirror design fails even at 10% |

Naming these low/medium/high would imply a validated product promise that the
measurements do not support. Preset names and thresholds remain open until an
optimized selective integration benchmark exists.

## Rejected approaches retained for Review H

| Attempt | Result | Failure mode | Reason rejected |
|---|---|---|---|
| Unconditional full-screen multi-hit collection | Deterministic and correct | 12.9–17.8 ms sampling-only | Exceeds entire frame budget before composition |
| Unconditional full-screen optical composition | Correct and bounded | Adds up to ≈2.6 ms | Exceeds available headroom at useful layer counts |
| Per-cell reflected column preparation | Correct one-bounce output | +1.8 ms at only 10% coverage | Exceeds headroom before reflected appearance work |
| One flat global draw z-order | Cannot describe current behavior faithfully | Conflates depth-aware world cells with post-raster UI pixels | Use explicit two-domain policy |

## Remaining unknowns before implementation planning

1. Whether selective continuation can be integrated into the existing prepared
   interval walk within the proposed default and coverage budgets.
2. Whether sparse optical overrides need a derived per-cell lookup table or can
   remain sparse without hot-path regression.
3. How partial reflectivity mixes direct surface, transmission, and reflected
   color under the P3 equation.
4. Whether reflected XY intervals can be prepared/reused per mirror-covered column
   while retaining correct per-row Z intersections.
5. What end-to-end coverage scenarios define acceptable translucency and mirror
   presets.
6. Future sprite/entity depth and tie-break details, deferred to R11.

These unknowns do not invalidate P1–P4 evidence. Review H must decide whether they
are implementation-plan requirements or require another bounded research loop.
