# R5 Increment I1 Implementation Record — 2026-08-12

## Status

**Complete and verified.** I1 delivers the v4 block codec and scene format,
65,536-slot material/palette/decal capacity, identity/reference policy helpers,
and sparse eager disk loading for widened IDs.

## Delivered behavior

1. **Version 4 scene format**
   - `SCENE_VERSION_V4` is the canonical native-save version.
   - v1, v2, and v3 remain readable migration inputs and open dirty with
     `migration_pending` until a successful v4 Save.
   - Material grids use exact uppercase four-hex blocks (`0000`..`FFFF`).
   - `0000` is a null reference in v4. Occupied walls still require a nonzero
     wall material; empty cells may retain a null or latent wall material.
   - v2/v3 decimal parsing and v1 legacy import remain intact.

2. **Dedicated block codec**
   - Added `src/scene_block_codec.h` and `src/scene_block_codec.c`.
   - The module owns strict parse, uppercase format, null test, row-block count,
     and semantic token-equality operations.
   - Version-specific field assembly remains in `scene_format.c`; v4 does not
     silently accept unknown future fields.

3. **Widened asset/reference path**
   - `SceneAuthoredCell` wall/floor/ceiling IDs and `PatternCell.material_id`
     are `uint16_t`.
   - Material, palette, and decal capacity is 65,536 slots (`0..65535`).
   - Sprite storage and sprite ID validation remain at 256 slots as explicitly
     deferred by the R5 decision record.
   - Config, command, editor-domain, scene-document, parser, loader, and decal
     I/O validation accept IDs through 65,535.

4. **Fixed-capacity registry without stack inflation**
   - `AssetRegistry` owns heap-allocated fixed-capacity arrays for palettes,
     materials, decal patterns, and material names.
   - Renderer lookup remains direct indexing (`assets->materials[id]`), so no
     hash or sparse-map CPU cost enters the render path.
   - `asset_registry_init()` reports allocation failure; production startup and
     tests handle it explicitly. `asset_registry_clear()` releases all ownership.
   - A generation counter advances once after each completed bulk registry load.

5. **Identity and reference policy**
   - `asset_registry_allocate_material_id()` returns the lowest free ID ≥ 1 and
     reuses freed slots.
   - `scene_document_find_material_reference()` reports the first blocking cell
     surface or scene-used decal pattern reference, supporting refuse-if-referenced
     deletion without mutating references.
   - Missing-reference diagnostics skip v4 null (`0000`) references and retain
     the existing `SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING` repair boundary.

## Disk loading

- Eager startup loading remains the selected model.
- Existing palette and decal files are enumerated by directory, so sparse IDs
  such as `50000.txt` load without probing 65,535 absent paths.
- Material loading retains deterministic numeric-first/named-second ordering,
  removes the old 256-file collection cap, and accepts explicit IDs through
  65,535.
- Decal structured material rows now have enough bounded storage for 255
  comma-separated five-digit material IDs.
- Sprite loading intentionally keeps its legacy 1..255 behavior.

## Tests added or updated

- Block codec: `0000`, `FFFF`, uppercase format, equality, count, lowercase and
  malformed rejection.
- v4 scene parsing/serialization: high IDs, null references, malformed hex,
  occupied-wall null rejection, and parse/save/reparse.
- Migration: v1/v2/v3 open pending and canonical Save writes v4.
- Identity: lowest-free reuse; high material ID 60,000; first scene/decal
  reference reporting.
- Disk formats: named material ID 60,000; sparse palette/decal ID 50,000;
  decal material ID 65,535 round trip; generation bump.
- Existing test fixtures now release heap-owned registry storage; no sanitizer
  suppression was introduced.

## Verification

Passed on 2026-08-12:

- `make all` — clean under `-std=c11 -Wall -Wextra -Wpedantic -Werror`.
- `make test` — full headless repository suite passed.
- `make asan` — full suite passed with AddressSanitizer/LeakSanitizer.
- `make ubsan` — full suite passed with UndefinedBehaviorSanitizer.
- `make check-legacy-unused` — no new deprecated legacy-format callers.
- `make leak` — skipped: Valgrind is not installed.
- `make style` — skipped: cppcheck is not installed.

## Explicitly deferred

- Map-scoped material shortlist, four-row picker, and name search are I2.
- The current picker loops remain bounded at legacy IDs rather than being
  widened to a 65,535-slot per-frame scan. I2 removes those loops entirely.
- MaterialDocument/shared lifecycle is I2.
- DecalDocument is I3; decal placement remains R6.
- Sprite widening and per-face wall materials remain future decisions.

## Next action

Begin I2 with the map-scoped material shortlist and shared asset-document
lifecycle, then build `MaterialDocument` through that lifecycle.