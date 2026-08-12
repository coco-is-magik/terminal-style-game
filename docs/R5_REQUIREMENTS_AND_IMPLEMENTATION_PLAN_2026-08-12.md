# R5 Requirements and Implementation Plan — 2026-08-12

## Status

**Active.** Prerequisites satisfied: R0–R4 Verified, material identity policy
resolved in `R5_ASSET_IDENTITY_DECISION_RECORD_2026-08-12.md`.

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

**Goal:** The foundation is widened, parseable, and policy-enforced.

**Tasks:**
1. Add `SCENE_VERSION_V4` to `scene_types.h` / `scene_format.h`.
2. Implement `scene_block_codec.{h,c}` with parse / format / is_null / count.
3. Add v4 parse path to `scene_format.c` (read 4-hex blocks; delegate to codec).
4. Widen types:
   - `SceneAuthoredCell` materials → `uint16_t`
   - `PatternCell.material_id` → `uint16_t`
   - `AssetRegistry` arrays → `[65536]`
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
   - `material_can_delete(AssetRegistry *, SceneDocument *, MaterialId)`:
     scan cells + decals for references.
   - `material_allocate_id(AssetRegistry *)`: wrap `first_free_material_id`
     logic into a public helper.

**Exit gate:** All existing tests pass after widening; new v4 parse / format
tests added; v2 / v3→v4 migration tested with missing-material repair.

**Estimated effort:** 1 day.

### I2 — Shared document lifecycle, MaterialDocument, picker shortlist

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

**Exit gate:** Deterministic and interactive tests for new / edit / save /
discard; picker navigation tests updated; no per-frame scan remains.

**Estimated effort:** 1.5–2 days.

### I3 — DecalDocument

**Goal:** Reusable decal pattern authoring through the same lifecycle.

**Tasks:**
1. Implement `DecalDocument` reusing `decal_painter` and `decal_io`
   boundaries (existing headless primitives).
2. Pattern grid editor (cols × rows, glyph + material per cell).
3. Decal picker shortlist (analogous to material picker, using
   `AssetRegistry.decal_patterns` and scene decal instances).
4. Asset / instance separation: `DecalDocument` edits the reusable pattern;
   scene placement remains in `SceneDocument` (R6).

**Exit gate:** Save / discard / undo / redo pass; pattern validation;
missing-reference repair detects missing decal patterns.

**Estimated effort:** 1 day.

### I4 — Asset refresh, dependency reporting, history boundary, gates

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

**Exit gate:** Q1–Q3 pass; Review E scheduled; no per-frame refresh;
dependency rules enforced.

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

Begin I1: create `scene_block_codec.h`, add `SCENE_VERSION_V4`, and widen
`SceneAuthoredCell` to `uint16_t`.
