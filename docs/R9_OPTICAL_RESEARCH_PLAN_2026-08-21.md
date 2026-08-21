# R9 Optical Research Plan — Layered Optical Rendering — 2026-08-21

## Status and authority

**Research track.** This plan scopes the R9 research phase: the questions, headless
prototypes, and evidence targets that must be satisfied before an implementation
plan is written. It does not commit architecture. Per the roadmap exit gate,
research prototypes must establish correctness and performance bounds first, and
**Review H** is the required checkpoint before any architectural commitment and
again after the implemented optical phase. R9's prerequisites — R4
geometry/appearance separation and the final R8 geometry — are satisfied
(recorded in `reviews/2026-08-21-roadmap-r8-phase-closeout.md`).

## Grounding: what the code provides today

| Fact | Location | R9 relevance |
|---|---|---|
| `Material` carries only `palette_id` + four distance glyphs; `Palette` is three `SDL_Color` stops | `src/assets.h` | No opacity, transmission, reflectivity, or collision-class fields exist yet |
| Material ID `0` doubles as "empty/passable" in the cell and map models | `scene_types.h`, `map.h` | The exact encoding `TODO.md` requires replacing with separated properties |
| `HeightfieldHit` is a single nearest hit (kind, distance, material, generated boundary) | `src/heightfield_trace.h` | Multi-hit collection is new; the prepared-column DDA already caches per-interval state, making it the natural extension point |
| Per-pixel `world_depths` / `world_hit_keys` frontier | `Grid` (added for R8 decal occlusion) | Existing ordered-depth primitive; prototype basis for layered compositing |
| Decal compositing already orders by perpendicular depth (`decal_depth`/`decal_order`) | `src/raycast.c` | Existing draw-order pattern to generalize rather than replace |
| Lighting is a scalar per-tile light map; `palette_sample` attenuates RGB by distance and light level | `src/lighting.c`, `src/assets.c` | Transmission hooks in here; colored light accumulation stays in R10 |
| `TODO.md` "Geometry, collision, and appearance separation" names the properties to separate | `docs/TODO.md` | Occupancy, player collision, ray/light interaction, visual material, opacity, reflectivity |

## Research questions

Each question pairs a hypothesis with the evidence the prototype must produce.

### RQ1 — Typed surface-semantics model

**Question:** Can occupancy, player collision, ray interaction, light
interaction, visual opacity, and reflectivity be separated into typed data
without breaking v5 load/round-trip, and where should they live?

**Hypothesis:** Optical properties are intrinsic to a *material*, so material-
level typed defaults (opacity, transmission, reflectivity, ray/collision class)
cover the common cases, with sparse per-cell overrides only where one cell must
differ from its material (e.g., a single glass pane in a stone wall).

**Method:** Extend the `Material` asset and/or `SceneAuthoredCell` behind a
compile-time gate with candidate fields; measure authored-memory cost at the
512×256 map limit; validate that a zero-valued extension reproduces today's
behavior exactly (checksum parity).

**Evidence target:** Memory report, parity checksums, and a written field-by-
field semantics table distinguishing ray-blocking, light-blocking, and
player-blocking as independent booleans/classes.

### RQ2 — Bounded multi-hit tracing

**Question:** Can the prepared column collect an ordered per-ray hit list within
the render budget, and what bound is affordable?

**Hypothesis:** A capped collection (proposed `MAX_RENDER_LAYERS = 4`) built on
the existing interval cache costs a small constant factor over the single-hit
path, because intervals are already enumerated; the cost is in per-hit surface
tests, not extra DDA stepping.

**Method:** Prototype a `heightfield_trace_collect` API returning up to N hits
per screen sample behind a compile-time gate; verify correctness against hand-
built scenes (glass pane in front of a wall; two stacked translucent planes);
benchmark single-hit vs 2/3/4-layer collection on the existing surface benchmark
scenarios.

**Evidence target:** ms/frame for N = 1..4 on the raised-height scenario;
deterministic checksums; a stated recommended cap with measured justification.

### RQ3 — Compositing in a glyph/color terminal

**Question:** How should translucent layers composite when the output alphabet
is one glyph plus fg/bg color pairs per cell?

**Hypothesis:** Background channels composite across layers (alpha-weighted
palette sampling, far-to-near), while the glyph comes from the topmost layer
whose opacity exceeds a threshold; below-threshold layers contribute only color.

**Method:** Prototype the compositor over the P2 hit list; lock behavior with
deterministic checksum fixtures for: single opaque, one glass layer over wall,
two glass layers, glass over an opening (darkness), and glass adjacent to a
generated discontinuity face.

**Evidence target:** Fixture set with exact expected checksums; a written rule
for glyph selection and for how transmission interacts with the scalar light
map (light passes through transmissive cells attenuated by transmission).

### RQ4 — Mirrors under bounded recursion

**Question:** What does one bounded mirror bounce cost, and what is the correct
fallback when a reflected ray hits nothing?

**Hypothesis:** A single reflection pass reusing the prepared-column tracer at
reflected directions is affordable at low screen coverage (mirror surfaces are
sparse); recursion is capped at one bounce for the prototype.

**Method:** Prototype a mirror flag on the typed model from RQ1; render one
reflected sample per mirror-covered screen cell with a documented fallback
(darkness, or the pre-existing background) when the reflected ray misses;
measure cost as a function of mirror screen coverage (10/25/50/100%).

**Evidence target:** Coverage-vs-cost curve; correctness fixtures (reflected
wall, reflected entity placeholder omitted — entities arrive in R11 — reflected
opening); an explicit statement that reflected rays never recurse into further
mirrors in the prototype.

### RQ5 — Unified draw-order policy

**Question:** Can decal, sprite, highlight, light-marker, and world-layer
ordering be expressed as one explicit layer policy instead of per-pass depth
comparisons?

**Method:** Write the policy table first (layer, sort key, tiebreak) as a design
note inside the research findings; validate it reproduces today's decal ordering
exactly on existing fixtures before any code changes.

**Evidence target:** Policy table + fixture parity statement. No new ordering
code in the research phase.

### RQ6 — Performance envelope and quality presets

**Question:** What is the measured cost envelope across layer counts and mirror
coverage, and what quality-preset shape does it justify?

**Method:** Aggregate RQ2/RQ4 measurements into a table; map presets (e.g.,
low = 1 layer + no mirrors, medium = 2 layers + mirrors, high = 4 layers +
mirrors) to measured frame times against the repository's 6 ms budget.

**Evidence target:** The budget table that R9's implementation plan will cite.

## Prototypes

All prototypes are headless, deterministic, allocation-bounded per frame, and
compile-time gated so the shipping renderer is untouched until Review H accepts
a design.

| Prototype | Question | Deliverable | Correctness criterion | Performance criterion |
|---|---|---|---|---|
| P1 | RQ1 | Typed optical fields + semantics table + memory report | Zero-valued extension is checksum-identical to today | Authored-memory delta reported at max map size |
| P2 | RQ2 | `heightfield_trace_collect` bounded hit list + focused runner | Hand-built scene fixtures match expected hit sequences | N=1..4 timing table vs single-hit baseline |
| P3 | RQ3 | Compositor + checksum fixtures | Fixture set (opaque/glass/stacked/opening/discontinuity) exact | Compositor cost within P2 envelope |
| P4 | RQ4 | Single-bounce mirror + coverage benchmark | Reflected-wall and reflected-opening fixtures exact | Coverage-vs-cost curve documented |

Each prototype lands with its own focused test runner and a findings note under
`docs/` recording measured numbers and go/no-go input for Review H.

## Product decisions surfaced early

These gate prototype design and should be settled during research, before
Review H:

1. **Schema vehicle (D1).** v5 shipped with R8; per-face wall materials were
   already "reserved for a future appended block." Optical fields likely follow
   that appended-block pattern or warrant **v6**. This interacts with the
   long-standing "extensible per-cell block serialization" idea in `TODO.md`.
   *Lean:* appended optional blocks in an amended v5 only if the block machinery
   already supports optional sections; otherwise v6. Decided by a schema spike
   in P1.
2. **Property placement (D2).** Material-level defaults vs per-cell overrides.
   *Lean:* material-level defaults, sparse per-cell overrides; decided by P1
   memory and semantics evidence.
3. **Caps and presets (D3).** `MAX_RENDER_LAYERS`, mirror bounce count, quality
   preset shape. *Lean:* 4 layers, 1 bounce, three presets; decided by RQ6
   measurements.

## Non-goals and forbidden shortcuts

Per the roadmap, the research and any later implementation must not:

- introduce one ambiguous "invisible" flag — ray, light, and player interaction
  are independent;
- allow unbounded recursion or unbounded layer collection;
- infer collision behavior from visual alpha;
- implement mirrors without depth, entity, and documented-fallback rules;
- write an implementation plan before prototypes produce evidence;
- commit any gated prototype code to the shipping render path before Review H.

Colored/spot lighting remains R10; sprite/entity ordering arrives with R11 and
is only *reserved for* in the RQ5 policy table.

## Review H entry criteria

Review H is scheduled when all of the following hold:

- [ ] P1–P4 findings notes exist with measured numbers (no "estimated").
- [ ] D1–D3 have recorded decisions with rationale.
- [ ] The RQ6 budget table states the proposed implementation budget.
- [ ] The RQ5 policy table reproduces current decal ordering on fixtures.
- [ ] No shipping-path behavior changed during research (checksum parity
      demonstrated by the gated builds).

## Deliverables and order

1. P1 semantics spike and memory report (settles D1/D2 direction).
2. P2 multi-hit tracer prototype and timing table.
3. P3 compositor fixtures.
4. P4 mirror prototype and coverage curve.
5. RQ5 policy table + RQ6 budget table.
6. Findings summary feeding **Review H**.

## Cross-references

- Roadmap: `docs/FEATURE_ROADMAP.md` (R9 section, review schedule).
- Separation requirements: `docs/TODO.md` — "Geometry, collision, and appearance
  separation" and "Mirrors."
- Closeout confirming R9 readiness: `docs/reviews/2026-08-21-roadmap-r8-phase-closeout.md`.
