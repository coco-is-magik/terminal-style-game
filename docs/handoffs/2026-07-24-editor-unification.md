# Editor Unification Handoff

**Date and time:** 2026-07-27 ~08:33 America/New_York  
**Task objective:** Implement the unified in-world editor vertical slice per  
`docs/EDITOR_UNIFICATION_PLAN.md` (architectural foundation + wall-material MVP).  
**Current status:** Phases P–5 complete and verified. Phase 6 (manual vertical-slice acceptance) is next.  
**Responsible scope:** Editor unification modules only; legacy designers remain  
until Phase 7. SMC/renderer work is out of scope for this track.

---

## Current Understanding

### Problem

Three separate editor application states (`LIVE_EDITOR`, `ASSET_DESIGNER`,
`MATERIAL_DESIGNER`) plus formerly unused `APP_STATE_EDITOR`. The product
direction is one in-world editor: walk → select → adjust → preview on a real
loaded level, with undo/redo/save and one authoritative map.

### Accepted requirements (MVP vertical slice)

1. `APP_STATE_EDITOR` is the future single entry for level/asset editing.
2. Real loaded level; one camera shared by walk and edit modes.
3. Authored map owned by `SceneDocument`; renderer reads it directly.
4. All mutations via undoable commands and document-state IDs.
5. Persist wall materials with existing map format (digits `0`–`9`).
6. First operation: assign an existing loaded material to a wall cell.
7. Legacy editor states stay until Phase 7 migration.

### Forbidden / deferred (this slice)

- Material asset authoring, per-face materials, floors/ceilings, map geometry
  edits, lights/decals/sprites/objects/triggers, painter integration, format
  migration, stable entity IDs, full-world rebuild on material change.

### Invariants

- Dirty iff `current_state != saved_state`.
- No-change assignment: no alloc, no state ID, no redo discard.
- Discarded redo state IDs never reused.
- Save atomic; unrepresentable IDs reject before temp create.
- `light_map` never serialized.
- Face selection is display/aim; material mutation is per-cell.

### Architecture (locked)

| Piece | Role |
|---|---|
| `editor_types.h` | Shared IDs, faces, selection types |
| `SceneDocument` | Owns authored `Map` + path + state IDs |
| `CommandHistory` | Lazy undo/redo; sole caller of internal doc mutators |
| `editor_selection` | Center-ray hit → `EditorHit` / `WallFaceRef` |
| `UnifiedEditorState` | Controller; owns doc+history+UI mode + picker |
| App camera / assets | Borrowed; not copied into editor |

### Authoritative docs

| Document | Role |
|---|---|
| `docs/EDITOR_UNIFICATION_PLAN.md` | Requirements, APIs, phases, exit gates |
| `docs/EDITOR_UNIFICATION_IMPLEMENTATION_NOTES.md` | Working log, decisions, verification |
| This handoff | Session recovery / next actions |
| `docs/handoff.md` | **SMC renderer track only** (unrelated) |

---

## Work Completed

### Phases

| Phase | Status | Summary |
|---|---|---|
| P | Complete 2026-07-24 | Commit + symbol inventory; zero pre-existing unified modules |
| 0 | Complete 2026-07-24 | `editor_types.h`; per-test Makefile source groups |
| 1 | Complete 2026-07-24 | SceneDocument load/save/dirty/internal mutators + 14 tests |
| 2 | Complete 2026-07-24 | Command history set/undo/redo + 16 tests |
| 3 | Complete 2026-07-24 | Wall selection / face calc + 12 tests |
| 4 | Complete 2026-07-24 | Unified editor shell, input, app entry + 11 tests |
| 5 | Complete 2026-07-27 | Material picker + set/undo/redo/save wrappers; 20 unified-editor tests |

### Files added

```text
src/editor_types.h
src/scene_document.h
src/scene_document_internal.h
src/scene_document.c
src/command_system.h
src/command_system.c
src/editor_selection.h
src/editor_selection.c
src/unified_editor.h
src/unified_editor.c
tests/test_scene_document.c
tests/test_command_system.c
tests/test_editor_selection.c
tests/test_unified_editor.c
docs/EDITOR_UNIFICATION_IMPLEMENTATION_NOTES.md
docs/handoffs/2026-07-24-editor-unification.md
```

### Files modified

```text
Makefile
src/app.c
src/input.h
src/input.c
tests/test_ui_ele.c
assets/ui_layouts/main_menu.txt
assets/ui_layouts/master_map.txt
assets/ui_elements/main_menu_asset_editor.txt
assets/ui_elements/main_menu_level_editor.txt   (added)
docs/EDITOR_UNIFICATION_PLAN.md
docs/EDITOR_UNIFICATION_IMPLEMENTATION_NOTES.md
```

### Key decisions (see implementation notes for full detail)

1. `SceneDocument` embeds `Map` by value; load moves from heap `Map*` then frees shell.
2. Command reserve-before-discard; grow never needed while redo branch exists.
3. Test allocator hooks on command history for OOM coverage.
4. Selection reuses `raycast_fire`; no duplicated DDA.
5. Cardinal faces locked: yaw 0 = +X/east, PI/2 = +Y/south;
   side0 +x→WEST, −x→EAST; side1 +y→NORTH, −y→SOUTH.
6. Const public selection API casts away const only at historical non-const
   map/camera call sites that do not mutate.
7. Phase 5 apply path: selection + `material_id_is_loaded` →
   `command_history_set_wall_material` only; no world rebuild.
8. Picker enumerates loaded IDs ascending (1..255 scan); Up/Down/Enter in inspector.
9. IDs > 9 live-allowed with immediate `EDITOR_STATUS_UNSAVABLE_MATERIAL_ID`.
10. Dirty F5 opens reload prompt; confirm calls `unified_editor_load_scene`.

### Baseline commit

`b7638887bf85e3909ca49dba28c4a07fb5b4aea0` (pre-unification source baseline).

---

## Work In Progress

None. Phase 5 is closed. Phase 6 (manual acceptance) not started.

Automated controller coverage for apply/undo/redo/save/picker/reload is done.
Manual in-game workflow acceptance remains.

---

## Investigation And Evidence

### Confirmed (post Phase 5)

- `APP_STATE_EDITOR` loads a real level via `unified_editor_*` and renders world + overlay.
- Main menu temporary entry: `LEVEL EDITOR` / `open_level_editor`.
- Edge-triggered `editor_*_pressed` actions exist on `InputState`.
- Public wrappers: `unified_editor_set_wall_material`, `_undo`, `_redo`, `_save`.
- Material picker + status mapping + dirty reload prompt implemented.
- Map save owned by SceneDocument; command history sole mutator path.
- Main app builds via `$(wildcard src/*.c)`; tests use narrow `TEST_*_SRC` groups.

### Verification evidence (Phases 0–5)

| Check | Result |
|---|---|
| `make -B all` | Success after each phase |
| `make test` / `make -B test` | All runners passed after each phase |
| `./build/test-scene-document` | 14/14 |
| `./build/test-command-system` | 16/16 |
| `./build/test-editor-selection` | 12/12 |
| `./build/test-unified-editor` | 20/20 (Phase 5) |
| Checked-in `assets/maps/` writes | None (tests use `/tmp/tsg_*`) |

### Remaining knowledge gaps for Phase 6

1. Manual visual confirmation that material change appears on next frame without
   world rebuild (unit tests cover mutation path; eyes-on still required).
2. Interactive Escape exit-prompt choices (Resume / Save+Exit / Discard+Exit)
   beyond unit-level hierarchy tests.
3. Whether any material-ID-keyed cache needs narrow invalidation after apply
   (plan: only if an existing public API requires it; no world rebuild).

---

## Failures And Rejected Approaches

| Attempt | Result | Lesson |
|---|---|---|
| Pre-plan: separate preview map for editing | Rejected by product direction | Editor uses real loaded level map only |
| Duplicating DDA in selection | Rejected | Reuse `raycast_fire` |
| Linking all `src/*.c` into every test | Rejected Phase 0 | Per-test source groups |
| Command grow after redo discard | Rejected | Reserve first so OOM leaves redo intact |
| Serializing `light_map` | Forbidden | Derived runtime only |
| Stub `unified_editor_set_wall_material` in Phase 4 | Forbidden by plan | No stub apply APIs; apply is Phase 5 |

SMC-related failures live in `docs/handoff.md` / SMC reports — not this track.

---

## Verification (latest full suite)

Last full run after Phase 5 (2026-07-27):

```text
make -B all          → success
make test            → all runners OK including:
  test-scene-document (14)
  test-command-system (16)
  test-editor-selection (12)
  test-unified-editor (20)
  test-ui-ele (updated main-menu counts)
```

Not yet run for this track (deferred to Phase 8 matrix):

```text
USE_SMC_STREAM_STATE_TRACKER=1
USE_DIRTY_CELLS=1
USE_LIGHTING_CACHE=1
make benchmark / make stability
```

---

## Risks And Unknowns

1. **Entry UX:** Temporary `LEVEL EDITOR` menu entry exists; Phase 7 collapses legacy entries.
2. **Camera on load:** Spawn `(1.5, 1.5)` on editor entry; walk/edit share one camera.
3. **Material IDs > 9:** Live-allowed, unsaveable; inspector surfaces status; save rejects.
4. **Legacy coexistence:** Three designer states remain; avoid breaking their tests.
5. **Input consumption:** Covered by Phase 4–5 tests; keep modal/inspector priority.
6. **Git:** Project skill forbids git operations; baseline hash recorded from
   workspace metadata only.
7. **Manual acceptance:** Phase 6 is primarily interactive; automate only where cheap.

---

## Next Actions (safe order)

### 1. Start Phase 6 — manual vertical-slice acceptance

**Action:** Run the plan Phase 6 checklist end-to-end from main menu:

1. Open `LEVEL EDITOR`.
2. Load real level (already automatic: `assets/maps/1.txt`).
3. Walk with existing movement / mouse-look.
4. Aim at a wall; `Tab` → edit (camera must not jump).
5. `E` select hovered face; Up/Down materials; `Enter` apply.
6. Confirm immediate visual material change (no world rebuild).
7. `Ctrl+Z` / `Ctrl+Y` restore old/new material.
8. `Ctrl+S` save serializable document; reload and confirm persistence.
9. Assign loaded ID > 9 → unsaveable warning; Save must leave destination unchanged.
10. Undo/replace unrepresentable ID; save successfully.
11. Confirm dirty across undo/redo/save/reload.
12. Exit via dirty prompt without silent data loss.

**Files:** none required unless a defect is found.  
**Expected:** All 18 plan steps pass.  
**Verification:** `make test` still green after any fix.  
**Stop:** Any world rebuild, silent data loss, or multi-layer input consumption.

### 2. Record Phase 6 evidence

**Action:** Update implementation notes + plan status + this handoff with
pass/fail per checklist item and any defects fixed.  
**Expected:** Phase 6 complete or a short defect list with repro.

### Phase 7+ (do not start until Phase 6 exit gate)

Legacy symbol disposition table, then remove separate editor states/menu entries.

---

## Recovery Guidance

1. **Authoritative plan:** `docs/EDITOR_UNIFICATION_PLAN.md`  
2. **Working log:** `docs/EDITOR_UNIFICATION_IMPLEMENTATION_NOTES.md`  
3. **This handoff:** next actions and verified state  
4. **SMC handoff:** `docs/handoff.md` — do not mix tracks  
5. **Prove tree health first:**

```bash
make -B all
make test
./build/test-unified-editor
./build/test-editor-selection
./build/test-command-system
./build/test-scene-document
```

6. **Phase 5 incomplete signals (should be gone):** missing
   `unified_editor_set_wall_material`, stubs, or apply bypassing command history.
7. **Do not repeat:** broad test linkage; world rebuild on material change;
   writing tests into `assets/maps/`; git operations if misc-forbid-git applies.
8. **Success for Phase 6:** plan exit gate — all automated tests + all manual
   workflow steps; no renderer/camera/light/decal/asset reset on material edits.

---

## Quick file index (unified editor)

| Path | Phase |
|---|---|
| `src/editor_types.h` | 0 |
| `src/scene_document.*` | 1 |
| `src/command_system.*` | 2 |
| `src/editor_selection.*` | 3 |
| `src/unified_editor.*` | 4–5 (shell + apply/inspector done); 6 acceptance |
| `src/input.*` | 4 (edge actions present) |
| `src/app.c` | 4 (entry + render); 6–7 later |
| `tests/test_scene_document.c` | 1 |
| `tests/test_command_system.c` | 2 |
| `tests/test_editor_selection.c` | 3 |
| `tests/test_unified_editor.c` | 4–5 (20 tests) |
