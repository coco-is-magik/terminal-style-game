# R8 Requirements and Implementation Plan — Vertical-World Implementation — 2026-08-19

## Status

**Verified (2026-08-21).** The heightfield remediation was implemented on
2026-08-20, all automated gates are green, and the real-video manual acceptance
pass for the amended checklist was recorded on 2026-08-21. Prior I1–I5
verification claims remain superseded by the remediation; current acceptance is
recorded in this document and `docs/reviews/2026-08-20-roadmap-r8-review-g.md`.
All current product and architecture decisions are
locked in `R8_DECISION_RECORD_2026-08-19.md`. The schema is specified in
`R8_NATIVE_SCENE_V5_SPEC_2026-08-19.md`. This plan passes the roadmap Q1 gate
before any R8 source code; the increments below are the Q2/Q3 plan and the
exit-gate evidence for R8 Verification.

## Scope

Implement the vertical world from the record: a **v5 scene format** carrying
authored signed floor/ceiling heights, explicit surface presence, per-cell gravity overrides, and a
per-map movement-parameter block; **height-aware rendering and view**
(eye-height horizon composition, per-column wall spans, height-aware selection
and highlights); and **vertical movement** (fall/gravity, step-up, jump,
head-clearance, full X/Y air control) with per-map runtime-tunable
parameters.

**Out of scope (locked):** true angular pitch, arbitrary inclined planes,
crouch, slide, stacked traversable spaces, per-face wall materials, sprites, and
any renderer rewrite. These must not re-enter R8 disguised as incremental fixes
(G3).

## Locked decisions (condensed)

See the decision record for the full rationale; the binding set is:

1. Continuous per-cell heightfield — `floor_h`/`ceiling_h` per cell; walls are
   vertical spans; one traversable interval per X/Y.
2. Cell-aligned ramps only.
3. Horizon-offset view; `horizon_row = center + pitch + eye_height_rows`.
4. Movement scope: fall/gravity, step-up, jump, and head-clearance.
5. No renderer rewrite; per-column math in the existing pixel-buffer ray caster.
6. v5 format, additive over v4, via the proven migration machinery.
7. Presence = explicit `floor_present` / `ceiling_present`; openings are terminal darkness.
8. Heights = signed fixed-point ⅟₂₅₆ `int16_t`, bounded to `-8..+8`; parameters = scalar doubles.
9. Jump = impulse against the effective gravity vector.
10. Gravity = map-wide default + per-cell override; any airborne body affected.
11. Air control = full X/Y, `air_control_scale` feel parameter.

Guardrails G1 (no stacked spaces), G2 (structural validation config-independent),
and G3 (deferred scope stays deferred) are binding.

## Engineering gates (every increment)

- Strict C11 build: `-Wall -Wextra -Wpedantic -Werror`; `make check`.
- Deterministic cmocka tests: temp dirs, exact `TSG-<DOMAIN>-*-NNNN` identifiers,
  exact result category, preserved-state guarantees, log cardinality
  (`C_STYLE_AND_OWNERSHIP.md`).
- All authored mutations through `command_system` (atomic group
  ≤ `EDITOR_COMMAND_MAX_MUTATIONS`, before/after snapshots, bounded history
  bytes).
- Every new diagnostic: one entry (Planned → Active at implementation) in
  `ERROR_CATALOG.md` with owner, detection, context, recovery, preserved state,
  and a focused test.
- `make asan`, `make ubsan`, leak pass; benchmark/stability matrix green inside
  the 4–6 ms `avg_render_ms` budget; flat-scene framebuffer checksums unchanged
  from the R7 baseline.

## Module boundaries and dependency direction

- `SceneDocument` remains the sole authoritative owner of v5 authored data
  (heights, presence, gravity overrides, movement block).
- New `src/height_view.h`: a borrowed, read-only per-cell view of
  heights/presence/gravity plus the movement parameters; derived from the
  document; disposable; never mutated. Consumed only by the renderer and
  physics.
- `vertical_physics.h/.c`: no ownership; deterministic runtime state for fall,
  grounding, step-up, ramps, clearance, jump, and air control; accepts
  the `HeightView` and input; performs no UI or persistence and owns no authored
  state (R1 Decision 5).
- `Camera` gains runtime `camera_z` (eye height, derived, never authored); the
  existing horizon-offset `pitch` is unchanged.
- All authored edits flow `UI → command_system → SceneDocument`; the renderer and
  physics only consume `HeightView`. No reverse edges; `app.c` wires but does not
  own domain logic.

```
UI / editor ───► command_system ───► SceneDocument ───► HeightView ◄── renderer
                                                            ▲       ◄── vertical_physics
```

## Failure paths and transaction boundaries

Every abnormal path asserts the exact diagnostic ID, category, preserved state,
and log cardinality (`C_STYLE_AND_OWNERSHIP.md`). Category is `INPUT`/`ENV`;
expected `STATUS` emits no error log.

| Failure | Category | Transaction boundary | Test |
|---|---|---|---|
| Lexical/block parse error in a v5 grid | INPUT (0015) | Reject candidate; preserve live doc | test_scene_v5_format |
| `floor_h ≥ ceiling_h` / under `SCENE_MIN_CLEARANCE` | INPUT (0014) | Reject candidate; preserve live doc | test_scene_v5_format / scene_document |
| Invalid surface-presence token | INPUT numeric; 0017 reserved | Reject candidate; preserve live doc | test_scene_v5_format |
| `[movement]` key/value failure | INPUT (0016) | Reject candidate; preserve live doc | test_movement_params |
| Gravity override out of bounds | INPUT (0018) | Reject; preserve live doc | test_gravity |
| Migration allocation failure | ENV (0001) | Abort transaction; preserve live doc | scene_document alloc hooks |
| v1/v2/v3/v4 migration | — | Pending/dirty until v5 Save; source untouched | scene_document equivalence |
| Save while repair-required | INPUT (0012) | Refuse Save; preserve destination + dirty | existing save tests |
| Height/presence/param command | — | One undoable group; before/after snapshot; rollback | editor-domain / command-system / unified-editor tests |
| Resize east/south | — | Copies new grids; shrink still content-blocked | scene_document / editor_domain |
| Runtime view build | ENV (runtime OOM) | No partial mutation; document intact | scene_document runtime build |

## Increments

### I1 — v5 heightfield schema, migration, and validation

**Status:** Remediated; the earlier implementation record is historical and its
ladder/unsigned-height details are superseded by the amended v5 spec.

**Goal:** Scenes can author and round-trip signed floor/ceiling heights, presence,
gravity overrides, and the movement block with deterministic bytes and strict
validation, and v1–v4 scenes migrate with byte-identical current behavior.

**Tasks:**

1. `SceneAuthoredCell` extension: `floor_h_step`/`ceiling_h_step` (`int16_t`),
   `floor_present`/`ceiling_present`, `gravity_scale` (`uint16_t`), `gravity_orientation`
   (`uint8_t`); add a layout/alignment static assert and document the ~16-byte
   cell cost at full dimensions (≈2 MB authored data, not per-frame).
2. `scene_types.h`: `SCENE_VERSION_V5`, `SCENE_VERSION` → v5, height bounds
   (`-0x0800`..`0x0800`), `SCENE_MIN_CLEARANCE`, movement-parameter defaults and
   ranges, effective-gravity bound.
3. `scene_format.c`: v5 parse/serialize for `[movement]` and the six new grid
   sections in canonical order (block codec for heights/gravity scales; decimal
   tokens for presence and orientations).
4. Migration: v4→v5 defaults per the spec (`0000`/`0100` heights, both surfaces present, no
   overrides, default movement block); keeps pending/dirty semantics; never
   rewrites sources.
5. Validation: height range, floor/ceiling relation + clearance
   (`TSG-SCENE-INPUT-0014` activates), presence token checks, `[movement]`
   block checks, gravity override bounds; preserve `TSG-SCENE-INPUT-0015/16/17/18`
   to the catalog.
6. Surface/view exposure: a borrowed, read-only, derived height/parameter view
   for the runtime.
7. Tests: deterministic v5 round-trip and byte equality, v1–v4 migration
   equivalence, range/rejection diagnostics with preserved state, allocation
   failure rollback, memory accounting.

**Exit gate:** parse/serialize/parse byte equality; v1/v2/v3/v4 fixtures migrate
and behave identically (rendering checksums equal); every new diagnostic has an
Active catalog entry and a focused test; strict build, asan/ubsan, leak pass.

**Estimated effort:** 2–3 days.

### I2 — Height-aware view and rendering

**Status:** Remediated. Rendering, picking, and horizontal highlights now share
the bounded-cell tracer; the earlier flat-plane implementation is superseded.

**Goal:** The view and ray caster make heights visible and remain identical for
flat scenes.

**Tasks:**

1. `Camera` gains runtime eye-height state (`camera_z`, derived, never authored)
   and the horizon composition `horizon_row = center + pitch + eye_height_rows`;
   document the row-scale constant.
2. `raycast.c`: preserve the exact flat-default fast path; non-flat scenes use
   bounded per-cell floor/ceiling intersections and generated vertical
   discontinuity faces. Missing surfaces terminate into darkness.
3. `editor_highlight`/`editor_selection`: height-aware projection so highlights
   and pick math agree with drawn columns.
4. Benchmark: add flat-vs-height scenario lines to `benchmark_surface_render.c`;
   flat grid equality checksum parity.

**Exit gate:** flat-scene checksums byte-identical to pre-R8; height scenes
deterministically correct (golden frames in tests); benchmark inside the 4 ms
ideal / 6 ms minimum; Q2 tests pass.

**Estimated effort:** 2–3 days.

### I3 — Movement core: gravity, fall, step-up, head-clearance, ramps

**Status:** Complete and verified. Evidence is recorded in
`R8_INCREMENT_I3_IMPLEMENTATION_RECORD_2026-08-19.md`.

**Goal:** A → B traversal in a height field: the vertical-movement backbone that
jump and air control build on.

**Tasks:**

1. Shared motion integrator: integrate Z and lateral acceleration against the
   effective gravity vector of the body's current cell.
2. Grounding: rest on `floor_h`; land after fall/step.
3. Step-up: auto-climb floor deltas ≤ `step_height`; larger deltas block
   traversal unless overcome by a jump (I4).
4. Head-clearance: entry blocked when the interval is below `head_clearance`;
   never weakens the `SCENE_MIN_CLEARANCE` structural bound (G2).
5. Cell-aligned ramps: crossing an edge is allowed when the target interval is
   traversable; the configured climbability derives from `step_height`/`jump`.
6. All numbers come from the movement block through one runtime view; the
   integrator is frame-rate independent exactly like `camera_update`.

**Exit gate:** deterministic tests for fall, land, step-up, clearance blocks,
ramp crossing, and mid-transition blocking; nothing creates a second interval
(G1); no per-frame allocation; Q2.

**Estimated effort:** 2–3 days.

### I4 — Jump and air control

**Status:** Remediated. Ladder behavior from the historical implementation record
is deferred and removed from active scope.

**Goal:** The bounded-vertical moves on top of I3.

**Tasks:**

1. Jump: apply `jump_impulse` opposite the cell's effective gravity vector;
   apex = `impulse²/(2·|g|)`; capped by ceiling/head-clearance.
2. Air control: reuse ground movement plus collision while airborne, with
   `air_control_scale` (0–1) applied to input.

**Exit gate:** jump apex/cap tests (including low-g and up-g cells), air-control
tests and G1 guard tests proving openings never create stacked intervals.

**Estimated effort:** 2–3 days.

### I5 — Editor workflows and parameter tuning

**Status:** Remediated in automated gates; pending the replacement real-video
acceptance pass. The earlier I5 record is historical where it conflicts with this
plan or the amended v5 spec.

**Goal:** Height/presence editing and per-map runtime parameter tuning through the
existing editor surface.

**Tasks:**

1. Typed vertical and movement-parameter mutations with complete before/after
   snapshots, including floor/ceiling presence,
   atomic groups, and resize interactions (growth/shrink copy the new grids;
   shrink stays content-blocked).
2. `unified_editor` gestures and HUD: signed height change, atomic multiselect
   remove/restore, and highlights on bounded authored/generated geometry.
3. Height-aware cursor/selection handles consistent with I2 projections.
4. Parameter-block editor for the running map with validation feedback.
5. Update `README.md` last (roadmap rule 9), editor tool-tips, and docs.

**Exit gate:** interactive acceptance passes (checklist below), undo/redo of
every height/presence/parameter edit, editor stability under Q2/Q3, full benchmark
matrix green. Reorderable with I3/I4 if acceptance patterns require.

**Interactive acceptance checklist (recorded at exit, R4-style):**

1. Select a floor/ceiling face and change its height → render updates live and
   the highlight tracks the new column.
2. Raise one cell relative to a neighbor → crossing the edge is a ramp (small
   delta) or is blocked/needs a jump (large delta).
3. Jump in a default-g cell vs. a low-gravity cell → apex visibly differs and
   matches the impulse/gravity formula.
4. Stand under a low ceiling → movement through an undersized interval is
   blocked; the blocking is observable in the HUD/diagnostic.
5. Remove a floor and ceiling separately → the floor opens downward to darkness,
   the ceiling upward; neither opening exposes a second interval (G1).
6. Tune a movement parameter on the running map → behavior changes immediately;
   Save/reopen preserves it; Undo/redo restores the previous value.
7. Grow east/south on a scene with heights/presence → new cells get defaults;
   shrink of an occupied ring is still content-blocked.
8. Load a v4 (and v1/v2/v3) fixture → renders byte-identical to pre-R8; it is
   dirty until a v5 Save; opening a v5 file round-trips cleanly.
9. Flat scene checksum parity holds; benchmark stays inside the 4–6 ms budget.

**Estimated effort:** 3–4 days.

## Test impacts

**New test files:**

- `test_scene_v5_format.c` — v5 round-trip, byte determinism, token-grammar
  rejection, grid cardinality.
- `test_movement_params.c` — `[movement]` block parse/range/duplicate/missing-key
  failures with exact identifiers.
- `test_vertical_motion.c` — fall, land, step-up, clearance block, ramp
  crossing, mid-transition blocking.
- `test_jump_air.c` — apex/cap across gravity cells, air control scaling.
- Heightfield opening regressions — absent floors/ceilings are not pickable or traversable.
- `test_gravity.c` — map default, per-cell magnitude/scale, orientation override.
- Editor/command regressions — v5 height/presence/parameter groups and undo/redo.

**Updated test files and survivors:** `test_scene_document.c` (load/migrate/save
v5, dirty/pending semantics), `test_scene_format.c` (v5 section ordering),
`test_command_system.c` (new mutation groups, batch/undo), `test_editor_domain.c`
(height edits + batch), `test_unified_editor.c` (gestures, HUD), `test_camera.c`
(horizon/eye-height composition), `test_editor_selection.c` (height-aware pick),
`benchmark_surface_render.c` (flat vs height scenario lines), plus the existing
sanity/smoke and `make check`/`asan`/`ubsan`/leak survivors under Q2/Q3.

## Affected-files checklist (spread per increment)

| File | I1 | I2 | I3 | I4 | I5 |
|---|---|---|---|---|---|
| `src/scene_types.h` | ✓ |  |  |  |  |
| `src/scene_format.{h,c}` | ✓ |  |  |  |  |
| `src/scene_block_codec.{h,c}` | ✓ |  |  |  |  |
| `src/scene_document.{h,c}` / `scene_document_internal.h` | ✓ | ✓ |  |  | ✓ |
| `src/surface_view.h` | ✓ | ✓ |  |  |  |
| `src/map.h` / `map.c` (authoring helpers) | ✓ |  | ✓ |  |  |
| `src/camera.{h,c}` |  | ✓ | ✓ | ✓ |  |
| `src/raycast.{h,c}` |  | ✓ |  |  |  |
| `src/renderer.{h,c}` |  | ✓ |  |  |  |
| `src/editor_highlight/selection` |  | ✓ |  |  | ✓ |
| `src/command_system.{h,c}` |  |  |  |  | ✓ |
| `src/unified_editor.{h,c}` / `editor_types.h` / `editor_domain` |  |  |  |  | ✓ |
| `ERROR_CATALOG.md` (0014–0018 → Active), README, tool-tips | ✓ |  |  |  | ✓ |

## Phase exit gate (roadmap)

- **Review G** before implementation and again before Verified.
- Geometry/collision/render agreement; format migration and editor workflows
  complete; Q1–Q3 pass; new catalog entries Active; benchmark matrix green
  inside budget; README updated last.
- Each increment keeps an `R8_INCREMENT_IX_IMPLEMENTATION_RECORD_*.md` with
  evidence, and a `docs/reviews/` entry for Review G.

## Out of scope / recorded follow-ups

True angular pitch, arbitrary inclined planes, crouch, slide, stacked spaces,
per-face wall materials, sprites, and any change that would break the
flat-world default-behavior guarantee. Recorded under "Open follow-ups" in the
decision record, not R8 blockers.