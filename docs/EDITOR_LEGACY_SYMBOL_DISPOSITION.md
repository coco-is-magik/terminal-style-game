# Editor Legacy Symbol Disposition

**Phase 7 — 2026-07-27**

## Current recovery status

The interrupted mechanical removal was reconciled on 2026-07-27:

- stale Asset Designer and Live Editor Makefile runners/source groups were removed;
- menu-state tests were migrated to unified-editor Escape ownership;
- generic UI parser/cache/render/layout coverage was retained with current fixtures;
- obsolete legacy-layout assertions were removed;
- the surviving generic quit-confirmation layout/elements were restored; and
- `make -B all` plus `make -B test` pass under strict warnings (166/166 tests).

This does **not** by itself satisfy the plan's complete Phase 7 preservation
gate. The entries below intentionally remove/defer canvas paint and brush
behavior; no extracted painter module or migrated painter tests currently exist.
Phase 7 therefore remains open until that requirement is implemented or the plan
is explicitly amended to make painter authoring deferred scope.

Every externally visible function, significant static helper, state structure,
and relevant test in the three legacy editor modules. Each symbol's disposition
is one of:

- `remove` — obsolete state/UI glue; no migration needed
- `move unchanged` — extracted to a shared module as-is
- `move with adaptation` — extracted with signature or behavior changes
- `retain temporarily` — kept in place until a later phase reworks it
- `replace` — replaced by tested unified-editor behavior

---

## 1. `src/asset_designer.h` / `src/asset_designer.c`

### State

| Symbol | Kind | Role | Disposition |
|--------|------|------|-------------|
| `AssetDesignerState` | `struct` | Transient decal canvas editor state | **remove** — obsolete; decal authoring deferred |
| `AdSurface` | `enum` | Surface selector (WALL, FLOOR, CEILING, ALL) | **remove** — unused outside this module |
| `AssetDesignerMode` | `enum` | Sub-mode dispatch (EDIT, SAVE_PROMPT, etc.) | **remove** — obsolete |
| `AssetDesignerResult` | `enum` | Callback result (NONE, EXIT, CONFIRM_DISCARD) | **remove** — caller removed |

### Public API

| Symbol | Role | Disposition |
|--------|------|-------------|
| `asset_designer_init()` | Heap-allocate pattern, zero state | **remove** — canvas editor deferred |
| `asset_designer_destroy()` | Free pattern | **remove** |
| `asset_designer_update()` | Frame input dispatch | **remove** |
| `asset_designer_render()` | Draw canvas, cursor, overlays | **remove** |
| `ad_validate_basename()` | Filename validator `[A-Za-z0-9_-]` | **remove/defer** — no current caller; no replacement symbol exists in `decal_io` |

### Significant statics / helpers

| Symbol | Role | Disposition |
|--------|------|-------------|
| `ad_finalize_decal()` | Closes decal + converts pattern to asset | **remove** — editors removed |
| `ad_clear_pattern()` | Resets pattern cells | **remove** |
| All static paint/brush/input helpers | Canvas manipulation | **remove** |

### Tests (`tests/test_asset_designer.c`)

| Test | Role | Disposition |
|------|------|-------------|
| All 11 tests | Canvas init, draw, clear, save, load, file path, navigation, basename validation | **remove** — entire module removed; `ad_validate_basename` has no caller |

---

## 2. `src/live_editor.h` / `src/live_editor.c`

### State

| Symbol | Kind | Role | Disposition |
|--------|------|------|-------------|
| `LiveEditorState` | `struct` | Combined decal/material live-preview editor | **remove** — obsolete; unified editor replaces in-world editing |
| `LeMode` | `enum` | DECAL_MODE, MATERIAL_MODE | **remove** |
| `LeInputResult` | `enum` | Consumed flag | **remove** |

### Public API

| Symbol | Role | Disposition |
|--------|------|-------------|
| `live_editor_init()` | Allocate state, load materials, load decals | **remove** — unified editor's `init` does equivalent |
| `live_editor_destroy()` | Free state | **remove** |
| `live_editor_update()` | Input dispatch + render | **remove** |
| `live_editor_render()` | Overlay rendering | **remove** |
| `live_editor_render_glyph_editor()` | Inline glyph pixel editor | **remove** |

### Reusable decal/material tooltip logic

| Symbol | Role | Disposition |
|--------|------|-------------|
| `le_render_decal_tooltip()` | Tooltip for decal slot | **remove/defer** — decal tooltip UI is future work; no current production symbol retained |
| `le_render_material_tooltip()` | Tooltip for material slot | **remove/defer** — material tooltip UI is future work; no current production symbol retained |
| `le_render_decal_pane()` | Decal list pane | **remove** — no slot-based decal UI in unified editor |

### Tests (`tests/test_live_editor.c`)

| Test | Role | Disposition |
|------|------|-------------|
| All tests | Init, destroy, input modes, rendering | **remove** — module removed |

---

## 3. `src/material_designer.h` / `src/material_designer.c`

### State

| Symbol | Kind | Role | Disposition |
|--------|------|------|-------------|
| `MaterialDesignerState` | `struct` | Material field editor state | **remove** — material authoring deferred |
| `MdFieldType` | `enum` | Field selector (GLYPH, PALETTE, etc.) | **remove** |
| `MdEditorMode` | `enum` | EDIT, GLYPH_PICKER, PALETTE_PICKER, SAVE_PROMPT | **remove** |

### Public API

| Symbol | Role | Disposition |
|--------|------|-------------|
| `material_designer_init()` | Init state with default material | **remove** |
| `material_designer_destroy()` | Free state | **remove** |
| `material_designer_update()` | Input dispatch | **remove** |
| `material_designer_render()` | Draw material editor UI | **remove** |

### Tests

No dedicated test file exists for material_designer. It is tested only through
app-level integration. No migration needed.

---

## 4. App-state dispatch (`src/app.c`, `src/config.h`)

| Symbol | Role | Disposition |
|--------|------|-------------|
| `APP_STATE_LIVE_EDITOR` | Enum value 3 | **remove** from enum; no remaining references |
| `APP_STATE_ASSET_DESIGNER` | Enum value 4 | **remove** |
| `APP_STATE_MATERIAL_DESIGNER` | Enum value 5 | **remove** |
| `APP_STATE_EDITOR` | Enum value 2 | **retain** — unified editor lives here |
| Legacy dispatch cases in app.c | `case APP_STATE_LIVE_EDITOR:` etc. | **remove** |
| `open_asset_editor` menu action | Pushes asset-select submenu | **remove** — replaced by `open_level_editor` |
| Legacy discard-change handlers | Checks legacy state before destroy | **remove** |

---

## 5. Menu entries and UI elements

| Element | Role | Disposition |
|---------|------|-------------|
| Main menu "Asset Editor" | Button → `open_asset_editor` | **remove** |
| Main menu "Level Editor" | Button → `open_level_editor` | **retain** as "Editor" |
| Asset-select submenu | Decal/Material/Live submenu | **remove** |
| All asset-select UI elements | `asset_select_*.txt` | **remove** |
| Editor-menu elements | `editor_menu_*.txt` | **remove** (was submenu for discard/back) |
| SLOT/element tooltip files | `slot_*.txt`, `le_*.txt` | **remove** — no slot-based UI |

---

## 6. Test files

| File | Disposition |
|------|-------------|
| `tests/test_asset_designer.c` | **remove** — module removed |
| `tests/test_live_editor.c` | **remove** — module removed |
| `tests/test_menu_state.c` | **update** — fewer menu entries, fewer focusable items |
| `tests/test_ui_ele.c` | **update** — fewer cache entries in master_map |