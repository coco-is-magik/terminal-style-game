# R7 Requirements and Implementation Plan — 2026-08-13

## Status

**Verified; I1 and I2 complete.** Phase R7 (Structural editing and scale) is complete and its
product/architecture decisions are locked in
`R7_DECISION_RECORD_2026-08-13.md`. R6 is Verified and R7 builds on its placed-
instance ownership plus the R3 command groups and R4 construction foundations.
I1 (multiselect + atomic batch operations) and I2 (limits/stress hardening) are
complete. Evidence is in `R7_INCREMENT_I1_IMPLEMENTATION_RECORD_2026-08-13.md`
and `R7_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-13.md`.

## Scope

Add resize-safe bulk world construction to the editor: an editor-transient
multiselect over homogeneous wall/ceiling/floor surfaces with a primary face, and
three atomic batch operations (remove, apply material, add decal) committed as
single undoable steps. The existing east/south copy-growth and safe refill-shrink
are retained as-is; R7 formalizes origin semantics and hardens limits.

## Locked decisions

See `R7_DECISION_RECORD_2026-08-13.md` for:

- **Origin model (B):** fixed top-left `(0,0)`; east/south expansion/contraction
  only, LIFO, shrink content-blocked; coordinates never shift; no format bump.
- **Multiselect:** editor-transient selection set with a single primary;
  **Ctrl + arrow** extends one axis, constrained to the same surface family and
  orientation, occlusion-respecting.
- **Reduced batch ops:** Remove, Apply material, and Add decal, each one undoable
  group; "Add decal" places one instance per selected surface as one batch.
## Increments

### I1 — Multiselect and atomic batch operations

**Status:** Complete and verified. See
`R7_INCREMENT_I1_IMPLEMENTATION_RECORD_2026-08-13.md`.

**Goal:** Users can extend a face selection along one axis with **Ctrl + arrow**
(staying on the same wall/ceiling/floor family and orientation, never selecting an
occluded face), see all selected faces with a distinct primary, and apply one of
three atomic batch operations (Remove, Apply material, Add decal) as a single
undoable step.

**Tasks:**

1. Selection-set domain: an editor-transient set of `SelectionTarget` members with
   a single primary; stable across resize via the existing
   `editor_selection_is_valid_for_map` validity check; bounded by map extent.
2. Multiselect gesture: **Ctrl + arrow** extends the set one axis at a time under
   the family/orientation constraint, using the surface pick to reject occluded
   faces (a hidden face stops the run instead of joining).
   - **Default axis mapping:** for floor/ceiling, arrows extend the two in-plane
     axes; for a wall face, arrows extend along that wall's face run. Edge cases
     are recorded in the decision record's open follow-ups.
3. Highlight: every selected face uses the existing face-highlight vocabulary,
   with the primary shown distinctly (primary badge / brighter outline).
4. Batch commands (one undoable group each):
   - **Remove:** remove wall material / surface content for every member,
     including attached wall decals (reuse the wall-decal cascade).
   - **Apply material:** set one material reference on every member.
   - **Add decal:** place one decal instance per selected surface as a single
     undoable batch; atomic rejection if capacity/size bounds are exceeded.
5. Reduced action surface: while the set has more than one member, only Remove /
   Apply material / Add decal are offered; per-field property editing stays
   single-selection only.

**Exit gate:** Deterministic tests cover set construction, primary tracking,
Ctrl+arrow axis/family/orientation constraint, occlusion rejection, highlight
state, batch remove/material/add-decal atomicity, capacity rejection, undo/redo
of every batch as one step, and reload round-trip. Strict build and Q2 pass.

**Estimated effort:** 1.5–2 days.

### I2 — Limits and stress hardening

**Status:** Complete and verified. See
`R7_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-13.md`.

**Goal:** Validate map-size, command-memory, and performance limits for resize and
batch operations, and prove allocation-failure rollback under stress.

**Tasks:**

1. Explicit map-size enforcement (`SCENE_MAX_WIDTH` / `SCENE_MAX_HEIGHT`) at every
   resize allocation with `checked_size_2d`; expansion rejects at the limit.
2. Command-history bounded-memory accounting for resize and batch command groups.
3. Stress tests: large maps, deep resize histories, large batch sets, and forced
   allocation-failure rollback for resize and batch commands (no partial
   mutation).
4. Stable-selection-across-resize tests: shrinking clears out-of-map set members;
   surviving members keep validity and the primary resolves safely.

**Exit gate:** Large-map and allocation-failure tests pass; no unbounded allocation
or unchecked dimension multiplication; strict build and Q2 pass.

**Estimated effort:** 0.5–1 day.

## Test impacts

- `test_editor_domain.c`: selection-set model, primary tracking, family/orientation
  constraint, occlusion rejection, reduced action surface.
- `test_command_system.c`: batch remove/material/add-decal mutations, atomicity,
  undo/redo-as-one-group, capacity rejection, ID allocation.
- `test_unified_editor.c`: Ctrl+arrow gesture wiring, highlight + primary state,
  application of each batch op, save/reopen stability.
- `test_scene_document.c`: batch-driven internal mutations and resize/selection
  interaction where exercised through the document.
- New stress/hardening tests for limits and allocation failure (I2).
- No new format/migration tests (selection is transient; no version bump).

## Affected files checklist

| File | I1 | I2 |
|---|---|---|
| `src/editor_selection.h` / `.c` | ✓ |  |
| `src/editor_domain.h` / `.c` | ✓ | ✓ |
| `src/command_system.h` / `.c` | ✓ | ✓ |
| `src/unified_editor.h` / `.c` | ✓ | ✓ |
| `src/editor_types.h` | ✓ |  |
| `src/input.h` / `.c` | ✓ |  |
| `src/scene_document.c` (batch-driven wiring) | ✓ | ✓ |
| `tests/test_editor_domain.c` | ✓ |  |
| `tests/test_command_system.c` | ✓ | ✓ |
| `tests/test_unified_editor.c` | ✓ |  |
| `tests/test_scene_document.c` | ✓ | ✓ |
| New stress/hardening tests |  | ✓ |