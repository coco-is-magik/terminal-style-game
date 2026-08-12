# R5 Asset Identity Decision Record — 2026-08-12

## Authority

This record captures the identity, deletion, capacity-widening, and picker-policy
decisions required before R5 implementation begins. Decisions were made in the
2026-08-12 design thread and are binding for all R5 increments.

## 1. ID allocation policy

**Decision:** Lowest free ID ≥ 1, with reuse of freed slots.

**Rationale:** Matches the existing `first_free_material_id()` scan in
`asset_loader.c` (line 285) and preserves continuity with the legacy 1..9
digit-map range.

**Mechanism:** Scan `material_names[]` for the first empty slot
(`name[0] == '\0'`). This is O(pool) but amortised by the infrequency of
material creation.

**Rejected alternatives:**
- Monotonic counter only: wastes slots and complicates 16-bit exhaustion.
- Explicit freelist: extra bookkeeping with no measurable gain at 65K capacity.

## 2. Deletion policy

**Decision:** Refuse deletion if the material is referenced.

**Rationale:** Prevents accidental surface destruction and maintains
referential integrity between scenes and assets.

**Mechanism:** Before delete, scan the active scene’s authored cells
(wall / floor / ceiling) and decal patterns for the target ID. If any
reference exists, reject and report the first referencer (cell coordinate +
surface field).

**Rejected alternatives:**
- Rename-when-referenced: silently mutates shared state.
- Cascade to void / default: forbidden shortcut per roadmap R5 exit gate.

## 3. Legacy digit-map migration

**Decision:** 1:1 digit mapping. Legacy `"3"` → `0003`.

**Rationale:** All legacy materials are already 1..9. The v1→v2 migration
already split occupancy from material; v2/v3→v4 only rewrites token width.

**Mechanism:** 3-digit decimal tokens `NNN` become 4-hex `0NNN`.
Example: `001` → `0001`, `255` → `00FF`.

## 4. Null reference token

**Decision:** `0000` is the null / no-reference token. It is **not** a material ID.

**Rationale:** Separates "empty / default / no material" from valid material
references, enabling explicit unassignment and latent empty cells.

**Mechanism:** `material_id_is_loaded()` returns false for ID 0
(`assets.c:192`). The v4 parser accepts `0000` in any material grid cell.
The missing-reference scan must **not** flag `0000` as missing.

**Correction from prior draft:** An earlier proposal to migrate legacy `0000`
to the lowest free ID was rejected because it would turn empty passable cells
into solid walls.

## 5. Capacity widening

**Decision:** Widen to `uint16_t` / 65 536 capacity across all material
reference paths.

**Affected types and arrays (I1):**

| Symbol | File | Change |
|---|---|---|
| `SceneAuthoredCell.wall_material` | `scene_types.h` | `uint8_t` → `uint16_t` |
| `SceneAuthoredCell.floor_material` | `scene_types.h` | `uint8_t` → `uint16_t` |
| `SceneAuthoredCell.ceiling_material` | `scene_types.h` | `uint8_t` → `uint16_t` |
| `PatternCell.material_id` | `assets.h` | `uint8_t` → `uint16_t` |
| `AssetRegistry.palettes[]` | `assets.h` | `[256]` → `[65536]` |
| `AssetRegistry.materials[]` | `assets.h` | `[256]` → `[65536]` |
| `AssetRegistry.decal_patterns[]` | `assets.h` | `[256]` → `[65536]` |
| `AssetRegistry.material_names[]` | `assets.h` | `[256][64]` → `[65536][64]` |
| `AssetRegistry.material_count` | `assets.h` | review `int` → `uint16_t` or `size_t` |

**Deferred (out of R5 I1 scope):**
- `SpriteAsset` / sprite IDs — sprite feature not yet implemented.
- Per-face wall materials — reserved as a future appended-block extension,
  not present in v4.

**Rejected alternatives:**
- Sparse dynamic registry: violates CPU-priority (hash / indirection cost).
- Keep decals at 256 until I3: introduces a later double-migration.

## 6. Registry refresh and missing-reference reporting

**Decision:** Refresh loads all asset directories and reruns scene repair on
successful asset commit **only**. Never per frame.

**Mechanism:** Reuse `asset_loader_load_registry()` and
`refresh_document_repair_diagnostics()` (`scene_document.c`). Bump a registry
generation counter so derived caches can invalidate.

**Missing references:** Integrate into the existing scene-document repair
diagnostic list (`SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING`). Reuse the existing
cell scan (`scene_document.c:438-440`) and repair pass (`:486`, `:545`).

## 7. Picker model

**Decision:** Map-scoped shortlist, 4 visible rows, incremental prefix search,
inline **Create new material** on empty results.

**Rationale:** Eliminates the O(65 536) per-frame scan that would result from
widening the current `1..255` picker loop (`unified_editor.c:177/196/220`).

**Mechanism:** Build the shortlist once at map open from:
- Referenced wall / floor / ceiling materials in authored cells
- Latent wall materials on empty-occupancy cells
- Materials referenced by decal patterns in the scene

Alphabetise by name. The overlay draws only visible rows from the cached
shortlist.

**Rejected alternatives:**
- Registry-generation cached index: superseded by the shortlist model.
- Lazy per-map loading: rejected in favour of retaining the existing eager
  startup load (`asset_loader_load_registry`).
- Material file chooser: dropped because the UI cost exceeds the value; all
  materials are already loaded eagerly.

## 8. v4 token grammar and block-codec module

**Decision:** 4-hex uppercase `XXXX` fixed-width blocks. A dedicated
`scene_block_codec` module owns parse / format / equality / count rules.

**Block semantics:**
- One block = one `uint16_t` field.
- `0000` = null / no reference.
- `0001`..`FFFF` = valid ID (1..65 535).

**Reserved blocks:** Added only when a real future field exists. No
placeholder reserved blocks in v4.

**Migration:** v1 / v2 / v3 remain migration inputs. v4 is the canonical
save format. Migration reuses R4 pending / repair mechanics.

## Affected files checklist

| File | Change |
|---|---|
| `src/scene_types.h` | `SceneAuthoredCell` materials → `uint16_t` |
| `src/assets.h` | `PatternCell.material_id` → `uint16_t`; registry arrays → `[65536]` |
| `src/assets.c` | Bounds checks `255` → `65535`; `material_count` type review |
| `src/asset_loader.c` | Loops `1..256` → `1..65536`; `first_free_material_id` range |
| `src/scene_format.c` | `parse_uint_range` caps → `65535`; add v4 parse path |
| `src/scene_format.h` / `src/scene_types.h` | Add `SCENE_VERSION_V4` |
| `src/scene_document.c` | Default material / palette validation → `65535` |
| `src/unified_editor.c` | `EDITOR_PICKER_VISIBLE` `6` → `4`; replace picker scans with shortlist |
| `src/config.c` | Default ID validation → `65535` |
| `src/command_system.c` | Material validation caps → `65535` |
| `src/raycast.c` | Direct indexing unchanged (type width only) |

## Sign-off

Decisions recorded 2026-08-12. Implementation proceeds in
`R5_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-12.md`.
