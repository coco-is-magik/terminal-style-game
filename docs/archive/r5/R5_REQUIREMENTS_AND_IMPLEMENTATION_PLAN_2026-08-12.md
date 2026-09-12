# R5 Requirements and Implementation Plan — 2026-08-12

## Status

**Verified.** I1–I4, Q1–Q3, and Review E are complete. See the increment
implementation records and `reviews/2026-08-12-roadmap-r5-review-e.md`.

## Scope

Establish a safe, tested authoring lifecycle for reusable assets, then build
`MaterialDocument` and `DecalDocument` through it.

## Locked decisions

See `R5_ASSET_IDENTITY_DECISION_RECORD_2026-08-12.md` for:
- ID allocation (lowest-free ≥ 1)
- Deletion (refuse-if-referenced)
- Capacity widening (`uint16_t`, 65 536)
- v4 token grammar (4-hex `XXXX`)
- Registry refresh model (commit-only, not per frame)
- Picker model (map-scoped shortlist, 4 rows, search)

## Increments

### I1 — Block-codec, v4 spec, registry widening, identity policy

**Status: Complete and verified (2026-08-12).** See
`R5_INCREMENT_I1_IMPLEMENTATION_RECORD_2026-08-12.md`.

**Goal:** The foundation is widened, parseable, and policy-enforced.

**Tasks:**
1. Add `SCENE_VERSION_V4` to `scene_types.h` / `scene_format.h`.
2. Implement `scene_block_codec.{h,c}` with parse / format / is_null / count.
3. Add v4 parse path to `scene_format.c` (read 4-hex blocks; delegate to codec).
4. Widen types:
   - `SceneAuthoredCell` materials → `uint16_t`
   - `PatternCell.material_id` → `uint16_t`
   - `AssetRegistry` fixed-capacity direct-indexed storage → 65,536 slots
5. Update all `1..255` bounds checks to `1..65535`:
   - `assets.c`: `material_id_is_loaded`, `material_name_by_id`,
     `material_find_by_name`, `asset_registry_set_material`,
     `asset_registry_set_decal_pattern`, `asset_registry_get_decal_pattern`
   - `asset_loader.c`: `first_free_material_id`, all `1..256` loops,
     `asset_loader_load_registry`, `asset_loader_load_materials`
   - `scene_format.c`: `parse_uint_range` caps, validation checks
   - `scene_document.c`: default material / palette validation
   - `config.c`: default ID validation
   - `command_system.c`: material loaded check
6. Implement `0000` null-reference handling in v4 parser and missing-ref scan.
7. Identity policy helpers:
   - `scene_document_find_material_reference()`: scan cells and scene-used
     decal patterns for the first blocking reference.
   - `asset_registry_allocate_material_id()`: return the lowest free ID ≥ 1.

**Exit gate: Passed.** All existing tests pass after widening; v4 parse/format,
v1/v2/v3 migration, high-ID disk loading, identity allocation/reference checks,
and missing-material repair are covered.

**Estimated effort:** 1 day.

### I2 — Shared document lifecycle, MaterialDocument, picker shortlist

**Status: Complete and verified (2026-08-12).** Shared asset state identity,
`MaterialDocument`, atomic persistence, map-derived shortlist, four-row scrolling,
prefix search, and inline default-material creation are implemented. Full visual
material-property editor UI remains deferred as explicitly decided; the headless
document API owns and tests edit / undo / redo / preview / Save / Save As / Discard.

**Goal:** Users can create, edit, undo, preview, save, and discard materials.

**Tasks:**
1. Extract common document lifecycle from `SceneDocument`:
   - dirty flag, undo / redo stack (via `command_system`), validation,
     preview, atomic Save / Save As / Discard.
2. Implement `MaterialDocument` with fields:
   - `name` (string, unique within registry)
   - `palette_id` (reference)
   - `glyphs[4]` (distance bands)
3. Material picker rewrite:
   - Build shortlist at map open from cell references + latent + decal refs.
   - Alphabetise by name.
   - 4 visible rows (`EDITOR_PICKER_VISIBLE` `6` → `4`).
   - Incremental prefix search while the Material submenu is open.
   - Empty-result row: **Create new material…** (inline minimal form:
     name + default palette + default glyphs).
4. Remove old picker scans (`editor_count_loaded_materials`,
   `editor_material_at_picker_index`, `editor_find_picker_index_for_material`).

**Exit gate: Passed.** Deterministic tests cover new / edit / save / discard /
undo / redo / preview and failure paths. Picker tests cover map-derived ordering,
four visible rows, scrolling, prefix search, empty-result create/save/apply, and
updated navigation. The retired registry-scan helpers no longer exist; overlay
rendering reads only the filtered shortlist and visible rows.

**Estimated effort:** 1.5–2 days.

### I3 — DecalDocument

**Status: Complete and verified (2026-08-12).** `DecalDocument` provides the
owned pattern-grid lifecycle through `AssetDocumentState`, `decal_painter`, and
`decal_io`. The editor owns a scene-derived, deduplicated decal shortlist model;
in-world placement and surface-projected painting remain R6.

**Goal:** Reusable decal pattern authoring through the same lifecycle.

**Tasks:**
1. Implement `DecalDocument` reusing `decal_painter` and `decal_io`
   boundaries (existing headless primitives).
2. Pattern grid editor (cols × rows, glyph + material per cell).
3. Decal picker shortlist (analogous to material picker, using
   `AssetRegistry.decal_patterns` and scene decal instances).
4. Asset / instance separation: `DecalDocument` edits the reusable pattern;
   scene placement remains in `SceneDocument` (R6).

**Exit gate: Passed.** Save / Save As / discard / undo / redo / preview and
atomic failure paths pass. Pattern dimensions and material references are
validated. Existing repair mode detects missing decal patterns through the
full 16-bit ID range, and the scene-scoped shortlist retains missing IDs for
repair visibility without scanning the registry per frame.

**Estimated effort:** 1 day.

### I4 — Asset refresh, dependency reporting, history boundary, gates

**Status: Complete and verified (2026-08-12).** Commit-only eager refresh,
transactional registry replacement, dependency diagnostic refresh, independent
history boundaries, Q1–Q3, and Review E are complete.

**Goal:** Safe registry refresh after commit and explicit history boundaries.

**Tasks:**
1. Registry refresh on material / decal Save:
   - Reload via `asset_loader_load_registry`.
   - Bump registry generation counter.
   - Rerun `refresh_document_repair_diagnostics`.
2. Missing-reference report surface:
   - Reuse scene repair diagnostic list
     (`SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING`).
   - Ensure clear error text (e.g. "material asset 1234 is not loaded").
3. Scene-history vs asset-history boundary:
   - Scene Save must not persist open asset docs.
   - Asset Save must not trigger scene save.
   - Dirty flags are per-document.
4. Pass Q2 per increment and Q3 at phase completion.
5. Schedule and pass Review E (asset-document reuse, transactions,
   over-abstraction).

**Exit gate: Passed.** Q1–Q3 pass and Review E is complete with no blocker.
Refresh is called only after successful asset Save/Save As, never per frame.
Scene, decal-pattern, and transitive material dependencies are enforced through
the existing repair list without mutating scene history.

**Estimated effort:** 0.5–1 day.

## Test impacts

- `test_scene_format.c`: Add v4 parse / format tests; v2 / v3→v4 migration tests.
- `test_scene_document.c`: Widen fixture material IDs; test null-reference handling.
- `test_unified_editor.c`: Update picker tests for shortlist model;
  add search / empty-result tests.
- `test_command_system.c`: Widen material ID validation.
- `tests/benchmark_surface_render.c`: Unaffected (renderer direct-index).

## Affected files checklist

| File | I1 | I2 | I3 | I4 |
|---|---|---|---|---|
| `src/scene_types.h` | ✓ | | | |
| `src/scene_format.h` | ✓ | | | |
| `src/scene_format.c` | ✓ | | | |
| `src/scene_block_codec.{h,c}` | ✓ | | | |
| `src/assets.h` | ✓ | | | |
| `src/assets.c` | ✓ | | | |
| `src/asset_loader.c` | ✓ | | | |
| `src/scene_document.c` | ✓ | ✓ | | ✓ |
| `src/unified_editor.c` | ✓ | ✓ | | |
| `src/config.c` | ✓ | | | |
| `src/command_system.c` | ✓ | | | |
| `src/raycast.c` | — | — | — | — |
| `src/decal_io.h` | | | ✓ | |
| `src/decal_painter.h` | | | ✓ | |
| `src/world.h` | | | | (sprites deferred) |

## Next action

R5 is Verified. Begin R6 planning for decal placement and point-light authoring
without reopening the R5 asset/instance ownership boundary.
