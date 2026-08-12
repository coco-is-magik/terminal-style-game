# R4 Requirements and Implementation Plan — Surface Data and Basic World Construction

**Plan date:** 2026-08-10  
**Status:** Active — Increments A–F and manual-review follow-up implemented;
interactive re-acceptance pending
**Prerequisites:** R2 and R3 Verified  
**Review checkpoint:** Targeted surface architecture review; escalate to Q4 if the
format or runtime adapters reveal broad coupling

## Objective

Replace the current `MapCell.material_id` coupling with explicit fixed-height
authored occupancy and surface appearance. Users must be able to select, render,
edit, undo, and persist one wall material per occupied cell and one floor and
ceiling material per cell; place and remove walls in bounds; and edit scene ambient
intensity. R4 must preserve the current one-level raycaster and prepare stable
surface identities without implementing R8 vertical geometry.

## Current baseline

- `MapCell.material_id == 0` currently means passable empty space; values above zero
  simultaneously mean occupied, collidable, ray-stopping, light-blocking wall and
  wall appearance.
- Native scene v1 persists that value as one three-digit token per cell in `[cells]`.
- Floor and ceiling are fixed reconstructed planes rendered with hard-coded grey
  backgrounds. They are not authored surfaces.
- `SceneDocument` is the sole owner of authored scene state. `Map.light_map` and
  `WorldState` are derived and are never serialized authored truth.
- R3 provides typed selection targets, domain inspector adapters, target-specific
  highlights, and bounded atomic command groups.

**Increment A completed 2026-08-10:** canonical scene v2 now owns independent
occupancy and wall/floor/ceiling material references. Native v1 and legacy maps migrate
transactionally; native v1/v2 open migration-dirty until a successful v3 Save. Missing
surface references preserve authored IDs, enter repair mode, and block Save. See
`R4_INCREMENT_A_IMPLEMENTATION_RECORD_2026-08-10.md` and
`R4_NATIVE_SCENE_V2_SPEC_2026-08-10.md`.

**Manual-review follow-up implemented 2026-08-12:** canonical Save now writes native
v3 growth provenance; v1/v2 remain migration inputs. Surface inspector hierarchy,
unified border highlights, decal-cascade wall removal, and east/south copy-growth /
safe refill-shrink are implemented. See `R4_NATIVE_SCENE_V3_SPEC_2026-08-12.md` and
`R4_MANUAL_REVIEW_FOLLOWUP_IMPLEMENTATION_RECORD_2026-08-12.md`.

## Locked decisions

### 1. R4 cell and surface model

Each in-bounds cell owns these authored values:

```text
occupancy: empty | wall
wall_material: MaterialId
floor_material: MaterialId
ceiling_material: MaterialId
```

- Occupancy is independent of all three appearance references.
- R4 retains one wall material per occupied cell. Independent north, south, east,
  and west wall materials are deferred.
- Every cell has floor and ceiling material references, including occupied cells.
  Outdoor/no-ceiling state is deferred because it requires explicit render, light,
  decal-support, and missing-surface behavior.
- Removing a wall preserves its latent wall material and both horizontal surfaces.
  Placing a wall reuses that latent wall material.
- A material ID never creates or removes geometry. Material `0` is not used as a
  hidden occupancy or missing-surface sentinel in the R4 authored model.
- R4 preserves current gameplay defaults by deriving collision, camera/sight-ray
  stopping, and light blocking from occupancy. It does not yet expose independent
  collision or optical overrides. Future schemas may add those typed policies, but
  renderer/runtime packing may not collapse authored appearance back into occupancy.
- `Map.light_map`, resolved asset pointers, render samples, and projection data stay
  derived, reconstructible, and unsaved.

### 2. Stable surface identity and coordinates

- A wall target remains a coordinate plus cardinal face for selection/highlight
  identity, while its material request addresses the cell-wide wall material.
- A horizontal target is `{map_x, map_y, floor|ceiling}`. It has no scene instance
  ID because it is a structural part of a fixed cell, not a movable instance.
- Floor and ceiling local coordinates use the same deterministic basis: local `u`
  increases with world `+X`, local `v` increases with world `+Y`, and each cell spans
  `[0,1] x [0,1]`. This identity/basis is the R6 anchor seam; R4 materials do not
  require UV-dependent rendering.
- R7 must explicitly preserve or remap coordinate-addressed surfaces during resize.
  R4 does not add resize or map-origin behavior.

### 3. Native scene v2 and migration

R4 introduces `scene_version = 2`. Canonical v2 replaces `[cells]` with four
required row-major grids:

```ini
[occupancy]
1 1 1 1
1 0 0 1

[wall_materials]
001 001 001 001
001 001 001 001

[floor_materials]
001 001 001 001
001 001 001 001

[ceiling_materials]
001 001 001 001
001 001 001 001
```

- Each section has exactly `height` rows and `width` tokens. Occupancy accepts only
  canonical `0` or `1`; material IDs are decimal `001..255` and reference reusable
  materials. One-token spacing is canonical. Separate grids keep maximum-width rows
  below the existing 4096-byte physical-line limit.
- Metadata, lights, decals, IDs, ordering, numeric rules, resource bounds, comments,
  and atomic save guarantees retain v1 behavior unless this plan explicitly changes
  them.
- A native v1 cell migrates as follows: old material `0` becomes empty occupancy;
  old material `1..255` becomes wall occupancy with the same wall material. Empty
  cells receive the configured default material as their latent wall material. Every
  floor and ceiling receives that same default material.
- Legacy digit-grid import first applies the existing v1 interpretation, then the
  same deterministic v1-to-v2 migration.
- Migration is transactional and in memory. Loading never rewrites the source.
  A migrated native v1 document is marked dirty/migration-pending so close uses the
  existing Save/Discard/Cancel workflow. Explicit Save writes canonical v2; legacy
  import still requires Save As and never changes the legacy source.
- New scenes initialize border cells as occupied, interior cells as empty, and all
  three material references to the configured default material.
- The configured default material must be in `1..255`. If it is structurally valid
  but not loaded, migration may commit only in repair mode under the missing-reference
  policy below. Invalid configured IDs fail candidate creation/import without
  changing the live document.

### 4. Missing material references and repair

- Every authored wall, floor, and ceiling material reference is validated against
  `AssetRegistry`, including latent wall materials on empty cells.
- An unresolved material uses the existing `TSG-SCENE-INPUT-0011` meaning: preserve
  the exact typed reference, commit visibly in repair mode, render a conspicuous
  deterministic fallback, list bounded cell/surface context, and block normal Save
  with `TSG-SCENE-INPUT-0012`.
- Repair replaces an explicit target surface reference with a loaded material ID
  through a typed command. It does not silently substitute the configured default,
  mutate the registry, or rewrite unrelated cells.
- Structural errors, invalid material ranges, malformed grids, unsupported versions,
  or allocation failure reject the complete candidate and preserve the live
  document/runtime.

### 5. Wall construction and safety

- `place wall` and `remove wall` are distinct typed mutations. The existing wall
  material mutation remains invalid for empty cells and cannot create geometry.
- Placing an already occupied wall or removing an empty cell is `NO_CHANGE`.
- A transition to occupied is invalid at the authored spawn cell.
- In the interactive editor, execute, undo, or redo is also rejected atomically when
  it would make the current editor-player cell occupied. History state/cursor,
  document state, dirty state, selection, camera, and runtime remain unchanged. The
  status explains that the player must leave the cell before retrying.
- Removing a wall is rejected while any wall decal instance is anchored to that
  cell. R4 neither deletes, moves, nor detaches instances implicitly. Floor/ceiling
  decals do not block the occupancy change because their support surfaces remain.
- Successful occupancy changes invalidate/rebuild derived collision, lighting, and
  runtime views through the existing transactional editor seam.
- Undo restores the complete prior occupancy and surface values exactly when safety
  preconditions hold. Command groups validate every mutation and reserve history
  before changing authored state.

### 6. Selection, highlights, inspector, and controls

- Center-ray selection composes wall, light, floor, and ceiling candidates without
  changing existing wall/light tie and occlusion behavior.
- Horizontal hits use the renderer's current fixed-plane reconstruction and horizon
  offset. They are not true 3D pitch or R8 height intersections.
- The nearest visible eligible target wins. Walls occlude horizontal surfaces behind
  them. Out-of-bounds, non-finite, horizon-singular, and over-range candidates reject
  without changing the current target.
- Floor and ceiling have distinct typed selection variants and visibly distinct
  hover/selected highlights. Highlights remain derived Grid output and never mutate
  authored materials.
- The surface inspector presents loaded materials through the existing bounded
  material-choice workflow. A wall face edits the one cell-wide wall material and
  states that limitation explicitly.
- Construction controls expose explicit Place Wall and Remove Wall actions rather
  than overloading material assignment.
- Scene ambient intensity is a typed scene field in `[0,1]`, editable by bounded step
  and inline numeric replacement. It uses command history and existing runtime
  rebuild/lighting invalidation; UI does not write the document directly.

### 7. Rendering

- Wall rendering samples the occupied cell's wall material and preserves current
  distance glyph, palette, side attenuation, lighting, decal, and highlight behavior.
- Floor and ceiling rendering replace hard-coded colors with the corresponding
  per-cell material. Each framebuffer sample selects the material's existing
  distance-band glyph and palette, then applies the same scalar light-map contract.
- Out-of-bounds plane samples retain the current deterministic hard-coded ceiling and
  floor backgrounds and never read outside map/material arrays. Only in-bounds samples
  resolve authored material references. Exact framebuffer tests lock this compatibility
  behavior before renderer code changes.
- R4 does not add texture maps, UV-dependent material sampling, colored illumination,
  variable heights, slopes, angular pitch, multiple ray hits, transparency, or
  reflections.
- Because horizontal material sampling changes the raycast hot path, deterministic
  framebuffer checksums plus dedicated native renderer benchmark and stability gates
  are mandatory before R4 can become Verified.

## Required outcomes

- Scene v2 owns explicit occupancy and wall/floor/ceiling material grids.
- Supported native v1 and legacy maps migrate transactionally and predictably.
- `SceneDocument` remains the only authored owner; runtime consumers use narrow
  authoritative/derived views.
- Wall, floor, and ceiling appearance round-trips exactly and unresolved materials
  use visible repair without authored substitution.
- Floor and ceiling can be selected, highlighted, edited, undone, redone, saved,
  reopened, and rendered independently.
- Wall placement/removal is bounded, attachment-safe, spawn/player-safe, exactly
  undoable when safety preconditions hold, and distinct from material assignment.
- Ambient intensity can be edited through the same typed command/history boundary.
- Existing wall/light workflows, native scene operations, legacy import, dirty
  prompts, repair, decals, lighting, camera movement, UI scale, and accessibility
  controls remain green.

## Forbidden shortcuts and deferred scope

- No material ID or material `0` as hidden occupancy, collision, optical, outdoor,
  or missing-reference state.
- No per-face wall materials in R4. Revisit only with an approved format, command,
  renderer, migration, and decal-anchor design.
- No outdoor/no-ceiling sentinel in R4. Every cell retains both horizontal surfaces.
- No slopes, variable heights, player/camera Z, vertical collision, or angular pitch;
  these remain R8 scope.
- No automatic map expansion, contraction, origin shift, or bulk selection; these
  remain R7 scope.
- No implicit attachment deletion/reanchoring, direct UI document writes, serializer
  knowledge in widgets, runtime cache as authored truth, or generic property bag.
- No weakening/removing current tests to obtain a pass and no native downgrade to v1
  or legacy digit grids.

## Increment A — Scene v2 authored model and migration

- Introduce bounded authored occupancy and material fields without serializing
  `light_map` or resolved assets.
- Add checked allocation, cleanup, candidate transfer, equality, and validation for
  all grids, including allocation-failure rollback.
- Parse and serialize canonical v2 with exact section/row/token validation.
- Add transactional v1-to-v2 and legacy-to-v2 migration with explicit defaults and
  migration-pending dirty behavior.
- Extend missing-reference diagnostics and repair enumeration to typed surfaces.

**Completed 2026-08-10:**

- Added explicit v1/v2 constants and the bounded `SceneAuthoredCell` value with
  independent occupancy and wall/floor/ceiling material references.
- `SceneFormatCandidate` owns and destroys a checked-size authored-cell array.
- `scene_format_migrate_v1_to_v2()` validates the complete v1 candidate, captures a
  caller-supplied default material in `1..255`, maps occupied wall materials exactly,
  gives empty cells the captured latent wall default, and initializes both horizontal
  surfaces to that default. Failure leaves candidate ownership/version unchanged.
- V2 validation checks authored-cell count, occupancy, material references, and spawn
  passability. Canonical serialization writes the four required grids and never
  silently downgrades v2 data.
- `SceneDocument` owns the authored-cell array after transactional commit while the
  existing `Map` remains a derived compatibility view for current consumers.
- Native v1 load is migration-pending and dirty; successful native v2 Save clears both.
  Legacy import remains non-destructive and initializes all new fields from the
  captured configured default.
- Missing wall/floor/ceiling references preserve exact IDs, emit bounded typed repair
  diagnostics, block Save, and support explicit loaded-material replacement.
- Focused format 14/14, document 37/37, command 22/22, and unified-editor 47/47 pass.
  Aggregate `make check`, full ASan, and full UBSan pass all 28 runners; the feature
  matrix passes all 8 modes.

**Increment A evidence:** strict scene-format/document builds; canonical v2 exact-byte
round-trip; v1 and legacy migration fixtures; malformed/unknown/duplicate/short/long
grid rejection; invalid/default/missing material cases; checked-size and injected OOM
rollback; full ASan/UBSan; 8-mode matrix.

## Increment B — Typed surface and construction commands

**Completed 2026-08-10.** Typed surface/ambient/construction mutations, duplicate-field
rules, attachment/spawn/player safety, exact execute/undo/redo, controller context, status
mapping, and derived runtime refresh are implemented and verified. See
`R4_INCREMENT_B_IMPLEMENTATION_RECORD_2026-08-10.md`.

- Add typed set-wall/floor/ceiling-material requests and scene-ambient requests.
- Add distinct place/remove occupancy requests preserving latent surfaces.
- Extend grouped-command duplicate-target rules so incompatible mutations of the same
  authored field cannot produce ambiguous snapshots.
- Add wall-decal attachment checks, spawn checks, and explicit current-player safety
  context for execute/undo/redo.
- Keep validation and authored mutation in `SceneDocument`/`command_system`; adapters
  return owned requests and the controller only composes context and runtime rebuilds.

**Increment B evidence:** normal/no-change/invalid target; unloaded material; attachment;
spawn/player collision; grouped rollback; state exhaustion; history allocation failure;
redo invalidation; execute/undo/redo exact restoration; failed-history-cursor preservation.

## Increment C — Typed horizontal selection and highlights

**Completed 2026-08-10.** Typed floor/ceiling targets, fixed-plane picking, strict
wall/light precedence, wall occlusion, controller composition, and distinct derived
highlights are implemented and verified. See
`R4_INCREMENT_C_IMPLEMENTATION_RECORD_2026-08-10.md`.

- Extend `SelectionTarget` with floor and ceiling cell references without changing
  stable-ID light selection or cardinal wall faces.
- Add allocation-free deterministic fixed-plane picking with finite/range/bounds,
  wall-occlusion, horizon, and precedence tests.
- Add target-specific floor/ceiling hover and selected highlights that compose with
  wall/light providers and preserve borrowed inputs.

**Increment C evidence:** all camera directions; floor/ceiling/horizon/extreme pitch;
nearest wall occlusion; map edges; invalid inputs; target precedence; existing wall and
light regression fixtures; deterministic highlight benchmark scenarios.

## Increment D — Surface and ambient editor adapters

**Completed 2026-08-10.** Typed surface/construction/ambient metadata, controller
controls, missing/empty material states, command-only mutation, and failure-atomic
runtime refresh are implemented and verified. See
`R4_INCREMENT_D_IMPLEMENTATION_RECORD_2026-08-10.md`.

- Extend the narrow R3 domain seam with typed surface presentation, material choices,
  construction actions, and ambient numeric metadata.
- Preserve loaded-material enumeration, empty-state behavior, held-key repeat, inline
  numeric entry, input consumption, Escape hierarchy, and command-only mutation.
- Rebuild derived runtime/lighting only after successful authored commits.

**Increment D evidence:** metadata/range/format tests; wall/floor/ceiling inspector
dispatch; no-loaded-material and missing-reference repair; construction status; ambient
step/value/undo/redo; failed runtime rebuild preserves document/runtime/UI state.

## Increment E — Per-cell horizontal-surface rendering

**Completed 2026-08-11.** A zero-copy borrowed surface view now supplies bounded
per-cell floor/ceiling materials to the renderer. NULL/malformed views and out-of-bounds
samples preserve constant backgrounds; missing references use the locked purple/`.`
fallback. See `R4_INCREMENT_E_IMPLEMENTATION_RECORD_2026-08-11.md`.

**Decisions locked 2026-08-10** (see `handoffs/2026-08-10-r4-increment-e-start.md`):
optional borrowed `SceneSurfaceView` passed to `raycast_render` (NULL keeps constant
backgrounds); missing surfaces render a light-independent bright-purple background
with black `.` glyphs; floor/ceiling resolution uses array indexing plus a per-column
cell cache with SMC only if measurement demands it; the cell schema stays at v2.

- Replace constant floor/ceiling fill with bounded authored material lookup while
  preserving current projection, scalar lighting, decals, depth, and highlights.
- Lock the preserved hard-coded out-of-bounds backgrounds with focused framebuffer
  tests before the implementation changes hot-path behavior.
- Keep material resolution outside avoidable inner-loop work and measure rather than
  assume acceptable cost.

**Increment E evidence:** exact wall/floor/ceiling material and lighting checksums;
missing-material fallback; out-of-bounds/extreme pitch; decal/highlight ordering;
multiple dimensions and loaded material gaps; native renderer benchmark/stability;
sustained ASan/UBSan run.

## Increment F — Workflow integration and closeout

**Implemented 2026-08-11; interactive acceptance pending.** The complete automated v2
workflow, representative checked-in scene, Q3 gates, and targeted architecture review
are complete. R4 remains Active until the required real-video checklist passes.

- Integrate New/Open/Import/Save/Save As/Reload and dirty/repair prompts for v2 without
  changing their accepted menu and failure-atomicity behavior.
- Add a representative checked-in v2 scene and preserve v1/legacy migration fixtures.
- Run Q3 sequentially where Make targets share/clean the build directory.
- Perform the targeted architecture review; escalate to full Q4 if the implementation
  duplicates authored maps, broadens controller ownership, or requires cross-domain
  format/runtime coupling beyond this plan.
- Mark R4 Verified only after automated and interactive gates pass and evidence is
  recorded in an implementation record and review.

## Verification inventory

### Format, ownership, and failure

- Exact canonical v2 parse/serialize/parse equality and deterministic ordering.
- v1 native and legacy migration defaults, migration-pending dirty behavior, explicit
  Save/Save As, no source rewrite on load/import, and no downgrade.
- Malformed, duplicate, unknown, unsupported, invalid material, allocation, read,
  write, flush, sync, close, replace, and repair-blocked paths preserve prior state.
- Runtime build failure never promotes a derived view into authored truth or partially
  switches document/runtime state.

### Commands and editor behavior

- Independent wall/floor/ceiling edits; ambient; place/remove; groups; undo/redo;
  no-change; invalid bounds; stale selection; missing assets; state/OOM exhaustion.
- Spawn, current-player, and attached-wall-decal rejection leave document, runtime,
  history, dirty state, camera, and target unchanged.
- Wall/light selection, inspectors, highlights, held input, inline entry, Save/Open,
  repair, Escape, UI scaling, and accessibility behavior remain unchanged.

### Rendering, performance, and interactive acceptance

- Authored floor/ceiling materials visibly differ per cell and follow scalar ambient
  and point-light shading; walls retain current distance/side behavior.
- Wall/floor/ceiling decals and target highlights retain documented occlusion/order.
- Strict build, aggregate suite, applicable feature matrix, legacy-symbol guard,
  full-suite ASan/UBSan, deterministic renderer/editor benchmarks, and stability pass.
- Interactive acceptance edits each surface and ambient, places/removes walls, checks
  safety/attachment refusals, exercises undo/redo and Save/reopen, compares v1/legacy
  migration, and continuously alternates wall/light/horizontal selection without
  visible corruption or interaction degradation.

## Rejected approaches

- **Four wall-face materials in R4:** rejected to keep the first surface migration,
  renderer, commands, and attachment semantics bounded. Cardinal face identity remains
  available for later use.
- **Keep `material_id == 0` as occupancy:** rejected because it preserves the coupling
  R4 exists to remove and prevents appearance-only references on empty cells.
- **Infer floors/ceilings from neighboring walls or renderer constants:** rejected
  because authored appearance would not round-trip or remain deterministic.
- **Delete wall attachments on wall removal:** rejected because it silently mutates a
  second domain and complicates exact undo. Explicit attachment authoring belongs to R6.
- **Variable heights/slopes now:** rejected as premature R8 geometry and collision work.
- **One packed multi-field text row:** rejected because maximum-width rows risk the
  existing physical-line bound and reduce parser/diagnostic clarity.

## Known risk

Horizontal material sampling adds asset lookup and glyph/palette sampling to a hot
path that currently writes constant colors. The implementation must avoid repeated
avoidable resolution work, preserve out-of-bounds backgrounds, and use deterministic
checksums plus benchmark/stability evidence. Performance work may optimize derived
views but may not weaken the authored model or silently change rendering semantics.

## Exit gate

R4 becomes Verified only when current scenes migrate without silent loss; v2
wall/floor/ceiling and ambient edits round-trip and undo/redo exactly; construction is
attachment-, spawn-, and player-safe; fixed-height horizontal surfaces render, select,
and highlight correctly; existing R0–R3 contracts remain green; Q1–Q3 pass; and the
targeted review records no blocker.

## Next action

Run the interactive checklist in
`reviews/2026-08-11-roadmap-r4-targeted-review.md`. Increment F implementation and
automated closeout evidence are in `R4_INCREMENT_F_IMPLEMENTATION_RECORD_2026-08-11.md`.
Do not mark R4 Verified until the checklist passes and its findings are recorded.
