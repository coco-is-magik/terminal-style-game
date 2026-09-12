# R8 Decision Record — Vertical-World Implementation — 2026-08-19

> **Superseded in part by the 2026-08-20 heightfield remediation.** Current
> authority is the amended native-v5 spec and requirements plan. In particular,
> heights are signed `-8..+8`, floor/ceiling presence is explicit, bounded-cell
> tracing replaces flat-plane correction, and ladders are deferred from R8.
> R8 was accepted in the real-video manual inspection and is **Verified
> (2026-08-21)**.

## Authority

This record captures the product and architecture decisions made in the
2026-08-19 design thread that unblock R8 ("Vertical-world implementation" from
`docs/FEATURE_ROADMAP.md`). The decisions are binding for all R8 increments and
are grounded in the locked R1 contract, the R4 surface data model, the R7
structural-editing semantics, and the current code.

The mandatory dedicated requirements and implementation plan
(`R8_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_*`) will specify concrete schema
grammar, numeric representation, limits, editor workflows, performance bounds,
and the test matrix, and must pass the roadmap Q1 gate before any R8 source code
begins. This record fixes the decisions that plan must answer to; it is not the
plan itself.

## Grounding in current code

- The `.tscene` format already authors per-surface, per-cell material grids
  (`occupancy`, `wall_materials`, `floor_materials`, `ceiling_materials` in
  `src/scene_format.c`) and surface-typed decals (wall / floor / ceiling). The
  canonical scene version before R8 was **v4** (R5 token-widening), and the v4
  spec explicitly does not add heights. I1 makes **v5** canonical (Decision 6).
  I2 makes floor/ceiling projection and wall spans consume the v5 heightfield
  through a borrowed view while preserving exact flat-scene output.
- `Camera.pitch` is a horizon offset in logical grid rows, clamped to
  `[-viewport_rows, +viewport_rows]` (`src/camera.c`), not an angle. R1 stated
  that this displacement is preserved until R8 explicitly changes it.
- Editor selection already branches floor vs. ceiling from pitch
  (`src/editor_selection.c`), so height-aware editing can land inside the
  existing surface plumbing.
- R1 locked: one traversable floor/ceiling interval per X/Y; no stacked spaces;
  the renderer remains a ray caster; no sector/portal topology; no full-3D
  geometry. Every decision below stays inside that contract.

## Decision 1 — World model: continuous heightfield

**Chosen:** The vertical world is a **continuous heightfield**. Every cell
authors a floor height `floor_h` and a ceiling height `ceiling_h` as real,
bounded, finite values. A wall is the vertical span between those two heights at
that X/Y column. The one traversable interval per X/Y is unchanged.

**Rationale:** This is the smallest coherent generalization of the existing grid
and v4 format: the ray caster and grid editor survive, and the v4-to-v5
migration is two additional height grids plus one parameter block (Decision 6).

**Rejected:** sector/portal topology and full-3D geometry (both already rejected
by R1), a voxel representation (render pipeline rewrite), and permanently
quantized height levels (would need a later one-way conversion when continuous
values become necessary).

**Consequences:** height values are validated bounded-finite with a structural
minimum head clearance and `floor_h < ceiling_h`; impossible relationships reject
the candidate at commit time.

## Decision 2 — Slope model: cell-aligned ramps only

**Chosen:** Height changes are traversed only across shared cell edges: a ramp is
the difference in floor height across a shared edge between two cells, bounded by
a plan-defined edge delta limit. There are **no arbitrary inclined planes**
interior to a cell; each cell's floor and ceiling are flat at that cell's values.

**Rationale:** A grid ray caster only needs edge-adjacent geometry; authoring,
selection, validation (neighbor delta bounds), and editor handles are all cheap.
Arbitrary planes would pull in a plane-equation pipeline and a polygon-style
editor with no current need.

**Rejected:** arbitrary inclined planes (recorded as a future option only).

**Consequence:** any walkable path follows flat cell segments connected by ramp
edges; a player crosses an edge only when the height delta is within the
step/jump/climb policy, and the edge delta limit is part of the slope data
contract.

## Decision 3 — View: horizon-offset pitch plus eye height; no true angular pitch

**Chosen:** R8 does **not** implement true angular pitch. `Camera.pitch` stays a
screen-row translation (the horizon offset). Height awareness is achieved by
composing the player's eye height into the view:

```
horizon_row = viewport_center + camera_pitch + eye_height_rows
```

where `eye_height_rows` derives from the player's real vertical position (base =
`floor_h` + standing height, modified by jump/fall and by position on ramps).

**Rationale:** This is the cheapest correct way to make height differences
visible (overlooking a lower floor, seeing into a pit, watching a ramp, viewing
a ledge from below) while preserving the exact current look-up/down behavior and
the R1 "pitch displacement preserved until R8 explicitly changes it" contract.

**Rejected:** true angular pitch in R8 — kept as a documented future option that
lands only with its own renderer-cost study and projection rewrite.

## Decision 4 — Movement: fall/gravity, step-up, jump, head-clearance, ladders

**Chosen scope for R8:** fall/gravity, step-up, jump, head-clearance, and
ladders. All operate inside the single traversable interval of each X/Y:

- **step-up** auto-bridges small floor-height deltas between adjacent cells;
- **jump** covers bounded ledges and small gaps, bounded by head-clearance;
- **fall/gravity** applies to downward transitions;
- **ladders** are vertical movement inside one tall column (for example a shaft
  or pit); a ladder must never create or reach a second walkable interval — any
  ladder that would interconnect two stacked rooms is rejected (guardrail G1);
- **head-clearance** is a structural requirement with a per-map minimum standing
  height; movement that would violate it is blocked.

**Excluded from R8 (documented out of scope):** crouch and slide.

**Chosen — parameters as per-map scene data:** every vertical-physics parameter
(gravity, step height, jump height and arc, ladder speed, max climbable slope,
eye height, head-clearance minimum) is **authored versioned scene data** in an
explicit per-map movement-parameter block owned by `SceneDocument`, stored in v4,
changed through the existing transactional command/undo model, tunable at runtime
while the application runs, and validated to bounded-finite ranges with stable
defaults.

**Parameter model:**

- Each parameter has a name, type, bounded-finite range, validation rule, and a
  stable default.
- Defaults reproduce current flat-world behavior exactly; a v4 (or older) scene
  loaded into v5 behaves as it does today.
- The editor surface for these runtime tweaks is specified by the R8 plan.

## Decision 5 — Renderer: no rewrite; height math in the current pipeline

**Chosen:** The renderer remains the software-pixel-buffer ray caster. Height-
aware rendering is per-column arithmetic: wall spans are computed from the
column's floor/ceiling heights at the hit wall; floor and ceiling shading already
exists and becomes height-aware; the eye-height-composed horizon (Decision 3)
selects row ranges. No fundamental projection rewrite.

**Rationale:** Cost stays within the established 4–6 ms measured budget, reusing
the existing renderer, benchmark, and test matrices; this is the smallest change
that makes geometry, collision, and rendering agree on one height source.

**Optimization research stays open:** any further optimization avenue (including
the deferred strict `-O2`/`-O3` profile evaluation) continues to be researched
separately; no optimization lands in R8 unless its baseline is correct output and
a measured real-scenario gain, and no rewrite is smuggled in as "optimization."

**Performance gate:** the R8 benchmark scenario matrix must show the height-aware
stages inside the same budget; a regression in the measured target is a blocker.

## Decision 6 — Format and migration: v5 via the proven machinery

**Chosen:** The v5 scene format extends v4 with exactly two additions:

1. Two per-cell height grids (`floor_heights` and `ceiling_heights`) holding
   bounded real values through the same grid machinery as the existing material
   grids.
2. One per-map movement-parameter block (Decision 4), with validated default
   values.

Migration to v5 uses the proven v1 → v2 → v3 → v4 machinery and the
forward-compat policy already stated by the v4 spec: an older scene loads at
current behavior with migration defaults applied (heights equal to today's
global planes, parameters equal to their defaults); the scene is
migration-pending/dirty until a successful v5 Save; v1–v4 remain accepted
migration inputs; the source is never rewritten; and malformed or unsupported
candidates reject transactionally with the established diagnostic/repair
conventions.

**Evidence required:** deterministic serialized v5, parse/serialize round-trip
equivalence, and a complete inventory of migration defaults with
legacy-equivalence tests.

## Guardrails (binding)

- **G1 — No stacked spaces:** jump, fall, step-up, ramps, and ladders all
  operate inside the single traversable interval of a cell. Nothing in R8
  creates or reaches a second walkable space at any X/Y; a ladder or shaft that
  would interconnect two rooms is invalid.
- **G2 — Structural validation is config-independent:** the schema validation
  tier (`floor_h < ceiling_h`, fixed conservative head clearance, bounded finite
  heights, edge delta limit) is fixed and never influenced by runtime movement
  parameters. Runtime parameters may only *tighten* gameplay feasibility (for
  example, a smaller configured step height makes a steeper edge
  untraversable); they never weaken structural validity.
- **G3 — Deferred scope stays deferred:** true angular pitch, arbitrary
  inclined planes, crouch, slide, a renderer rewrite, and stacked spaces are
  outside R8 and must not re-enter disguised as incremental fixes.

## Consequences

- Editor, renderer, collision, and persisted data share one height source per
  cell — a single coordinate model, with no editor/renderer divergence.
- Height-aware selection, highlights, decals, lights, objects, and editor
  handles are required for R8 coherence and ride the existing surface plumbing.
- A v4 (or older) scene opened in v5 behaves identically to today (default
  heights and default parameters), preserving the legacy-equivalence evidence
  requirement.
- The movement-parameter block is tunable during an application run on a
  per-map basis through the existing command/undo model.

## Addendum — movement, air, storage, and gravity decisions (locked 2026-08-19)

Resolved in the design thread to complete the physics/authoring model. Binding
for R8; these parameters belong to the movement-parameter block introduced in
Decision 4.

- **Air control (full X/Y):** airborne movement uses the same free X/Y input,
  diagonal normalization, and axis-aligned sliding collision as ground movement
  (`src/camera.c`). Feel is tuned by a single `air_control_scale` parameter
  (default 1.0, range 0–1).
- **Gravity — map default plus per-cell override:** gravity is a world-space
  acceleration vector with a map-wide default (magnitude + orientation) and an
  optional per-cell override. The override is dense in `SceneAuthoredCell`:
  `gravity_magnitude_scale` (fixed-point `uint16_t`, `0` = inherit) and
  `gravity_orientation` (`uint8_t` enum, `0` = inherit, then 1=down, 2=up,
  3=north, 4=south, 5=east, 6=west). Gravity applies to **any airborne body**;
  grounded bodies rest on `floor_h`. The camera never rotates to follow gravity
  — rendering stays per Decisions 3 and 5.
- **Jump as impulse against gravity:** a jump is an impulse of `jump_impulse`
  (cells/second) applied opposite the effective gravity vector at the player's
  cell; airborne motion shares one gravity/fall integrator. Apex equals
  `jump_impulse² / (2·|g_effective|)`, capped by head-clearance/ceiling.
- **Ladder — per-cell field:** `has_ladder` boolean in `SceneAuthoredCell`;
  climbs within one tall interval only and never between stacked intervals (G1);
  no direction field in v1. Material-derived ladder behaviour is rejected (R1
  Decision 3).
- **Storage — fixed-point heights, scalar parameters:** per-cell heights store
  as fixed-point `uint16_t` at ⅟₂₅₆ world-unit resolution for byte-exact
  deterministic round-trip; movement/gravity parameters are scalar doubles in
  the v5 block (low volume). `SceneAuthoredCell` grows from 8 to about 16 bytes
  at full dimensions (≈2 MB committed authored data, not a per-frame
  allocation).

## Initial increment sketch (plan-shaping only, not binding)

- I1 — v5 heightfield schema: height grids, movement-parameter block, structural
  validation, and the v4 → v5 migration with round-trip and default-equivalence
  evidence.
- I2 — Height-aware rendering: per-column wall spans, eye-height horizon
  composition, and updated benchmark scenario lines.
- I3 — Movement core: fall/gravity, step-up, head-clearance enforcement, and
  cell-aligned ramp traversal with the edge delta rule.
- I4 — Jump and ladders within a single interval (bounded ledges; shaft/pit
  columns with a tall interval).
- I5 — Editor workflows: height painting/editing, ramp highlight, height-aware
  selection/handles, and the runtime per-map parameter editor.

Ordering and detail are decided by the R8 requirements and implementation plan,
which must pass Q1.

## Open follow-ups (recorded, not R8 blockers)

- True angular pitch, only if the world model later calls for it.
- Arbitrary inclined planes / interior slopes.
- Crouch and slide rules.
- Height-aware lighting refinements beyond the current light model.