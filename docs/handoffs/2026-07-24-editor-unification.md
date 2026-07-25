# Editor Unification Handoff

**Date and time:** 2026-07-24 ~16:26 America/New_York  
**Task objective:** Implement the unified in-world editor vertical slice per  
`docs/EDITOR_UNIFICATION_PLAN.md` (architectural foundation + wall-material MVP).  
**Current status:** Phases P–4 complete and verified. Phase 5 is next.  
**Responsible scope:** Editor unification modules only; legacy designers remain  
until Phase 7. SMC/renderer work is out of scope for this track.

---

## Current Understanding

### Problem

Three separate editor application states (`LIVE_EDITOR`, `ASSET_DESIGNER`,
`MATERIAL_DESIGNER`) plus an unused `APP_STATE_EDITOR` placeholder. The product
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
| `UnifiedEditorState` | Phase 4+ controller; owns doc+history+UI mode |
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

### Baseline commit

`b7638887bf85e3909ca49dba28c4a07fb5b4aea0` (pre-unification source baseline).

---

## Work In Progress

None. Phase 4 is closed. Phase 5 not started.

Do not treat material-apply, inspector apply, undo/redo/save wrappers, or
picker UI as done — those are Phase 5–6. `APP_STATE_EDITOR` shell + entry
are done.

---

## Investigation And Evidence

### Confirmed

- `APP_STATE_EDITOR` is still a placeholder `grid_print` branch in `app.c`.
- No production transition into `APP_STATE_EDITOR` yet (menu opens asset select).
- Map save did not exist before Phase 1; SceneDocument owns serialization now.
- `InputState` has designer edge flags but no unified-editor actions yet.
- Main app still builds via `$(wildcard src/*.c)` so new modules link automatically.
- Tests use narrow `TEST_*_SRC` groups (Phase 0).

### Verification evidence (Phases 0–3)

| Check | Result |
|---|---|
| `make -B all` | Success after each phase |
| `make test` / `make -B test` | All runners passed after each phase |
| `./build/test-scene-document` | 14/14 |
| `./build/test-command-system` | 16/16 |
| `./build/test-editor-selection` | 12/12 |
| Checked-in `assets/maps/` writes | None (tests use `/tmp/tsg_*`) |

### Remaining knowledge gaps for Phase 4

1. Exact entry path into `APP_STATE_EDITOR` (temporary menu hook vs debug key)
   — plan allows Phase 4 to wire placeholder; Phase 7 cleans menu.
2. Whether editor entry should reset camera to spawn or preserve current camera
   — plan: preserve unless existing entry contract initializes spawn.
3. How `WorldState` lights/decals are supplied during editor world render
   (app-owned; pass through existing `raycast_render`).
4. Exact `AssetRegistry` / `InputState` / `Grid` include set for `unified_editor.h`
   (confirm against headers at Phase 4 start).

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

Last full run after Phase 3:

```text
make -B all          → success
make test            → all runners OK including:
  test-scene-document (14)
  test-command-system (16)
  test-editor-selection (12)
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
2. **Camera on load:** Spawn vs preserve must match existing app contracts.
3. **Material IDs > 9:** Live-allowed, unsaveable; inspector must surface this
   (Phase 5+); save path already rejects.
4. **Legacy coexistence:** Three designer states remain; avoid breaking their
   tests while adding unified editor.
5. **Input consumption:** One key must not fire multiple layers; needs explicit
   consumed flags (Phase 4 tests).
6. **Git:** Project skill forbids git operations; baseline hash recorded from
   workspace metadata only.

---

## Next Actions (safe order)

### 1. Start Phase 5 — material assignment wrappers

**Action:** Implement `unified_editor_set_wall_material`, `unified_editor_undo`,
`unified_editor_redo`, `unified_editor_save` in `src/unified_editor.c`. Validate
loaded materials via `material_id_is_loaded`; route mutations only through
`command_history_*`. Map command/save results to `EditorStatus`.
**Files:** `src/unified_editor.h`, `src/unified_editor.c`
**Expected:** Apply changes authoritative map immediately; no world rebuild.
**Verification:** New controller tests + existing suite green.
**Stop:** Any path that mutates map cells outside command history.

### 2. Inspector picker UI

**Action:** Enumerate loaded materials; Up/Down highlight; Enter applies;
show dirty + unsaveable (ID > 9) warnings; cell-wide material limitation text.
**Files:** `src/unified_editor.c` (`unified_editor_render_overlay` + update)
**Expected:** Keyboard-driven picker; empty-state when no materials.
**Verification:** Controller tests for validation/status mapping.
**Stop:** Synthetic integer ranges instead of loaded materials.

### 3. Wire shortcuts already in InputState

**Action:** Consume `editor_undo_pressed` / `redo` / `save` / `reload` /
`previous` / `next` / `confirm` in update path with correct priority.
**Files:** `src/unified_editor.c`
**Expected:** Edge-triggered; modal/inspector priority preserved.
**Verification:** Unit tests + no multi-layer consumption regressions.
**Stop:** Held-key repeat on one-shot actions.

### 4. Tests

**Action:** Extend `tests/test_unified_editor.c` for Phase 5 plan items
(material validation, status mapping, undo/redo, selection revalidation,
unsaveable reporting).
**Files:** `tests/test_unified_editor.c`
**Expected:** Full `make test` green.
**Stop:** Any legacy runner failure.

### 5. Documentation after Phase 5 verify

**Action:** Update implementation notes + plan status + this handoff.
**Expected:** Phase 5 complete with evidence; Phase 6 acceptance next.

### Phase 6+ (do not start until Phase 5 exit gate)

Manual vertical-slice acceptance, then legacy migration (Phase 7).

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

6. **Phase 4 incomplete signals:** missing `src/unified_editor.c`, or
   `APP_STATE_EDITOR` still only `grid_print`, or no `editor_*_pressed` in
   `input.h`.  
7. **Do not repeat:** broad test linkage; stub material-apply APIs in Phase 4;
   writing tests into `assets/maps/`; git operations if misc-forbid-git applies.  
8. **Success for Phase 4:** plan exit gate — default build + all tests green;
   `APP_STATE_EDITOR` loads/renders real level; walk/edit preserves camera;
   legacy editors still functional.

---

## Quick file index (unified editor)

| Path | Phase |
|---|---|
| `src/editor_types.h` | 0 |
| `src/scene_document.*` | 1 |
| `src/command_system.*` | 2 |
| `src/editor_selection.*` | 3 |
| `src/unified_editor.*` | 4–6 (not created yet) |
| `src/input.*` | 4 (actions) |
| `src/app.c` | 4, 6, 7 |
| `tests/test_scene_document.c` | 1 |
| `tests/test_command_system.c` | 2 |
| `tests/test_editor_selection.c` | 3 |
| `tests/test_unified_editor.c` | 4–6 (not created yet) |
