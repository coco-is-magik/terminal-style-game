# Editor Unification — Implementation Notes

## Status

- Phase P: complete (2026-07-24)
- Phase 0: complete (2026-07-24)
- Phase 1: complete (2026-07-24)
- Phase 2: complete (2026-07-24)
- Phase 3: complete (2026-07-24)
- Phase 4: complete (2026-07-24)
- Next: Phase 5 (Existing-material assignment inspector)
- Source baseline commit: `b7638887bf85e3909ca49dba28c4a07fb5b4aea0`





No unified-editor production modules existed at Phase P start.
Search confirmed zero matches for `scene_document`, `unified_editor`,
`command_system`, `editor_types`, and `editor_selection`.

## Phase P — Confirmed Declarations

Exact repository declarations checked against source before implementation.
If a later edit changes a signature, update this section and the plan together.

| Symbol | Location | Confirmed declaration |
|---|---|---|
| `Camera` | `src/camera.h:27` | `typedef struct { Entity transform; double fov; double pitch; } Camera;` |
| `camera_init` | `src/camera.h` | `void camera_init(Camera *cam, double x, double y, double angle, double fov);` |
| `camera_update` | `src/camera.h` | `void camera_update(Camera *cam, Map *map, InputState *input, double delta_time_sec);` |
| `Map` | `src/map.h:38` | `typedef struct { int width, height; MapCell *cells; double *light_map; } Map;` |
| `MapCell.material_id` | `src/map.h` | `int material_id;` (`0` = void/passable, `>0` = wall) |
| `map_in_bounds` | `src/map.h:49` | `bool map_in_bounds(Map *map, int x, int y);` |
| `map_get` / `map_set` | `src/map.h` | `MapCell* map_get(Map *map, int x, int y);` / `void map_set(Map *map, int x, int y, int material_id);` |
| `map_create` / `map_destroy` | `src/map.h` | heap `Map*` lifecycle |
| `AssetRegistry` | `src/assets.h:89` | fixed arrays for palettes, materials, sprites, `material_names`, `material_count` |
| `material_id_is_loaded` | `src/assets.h:179` | `bool material_id_is_loaded(const AssetRegistry *reg, int id);` |
| `material_name_by_id` | `src/assets.h` | `const char *material_name_by_id(const AssetRegistry *reg, int id);` |
| `AppState` | `src/config.h:118-125` | includes `APP_STATE_EDITOR` (placeholder), live/asset/material designer states |
| `RayResult` | `src/raycast.h:30` | `hit`, `distance`, `map_x`, `map_y`, `side` |
| `raycast_fire` | `src/raycast.h` | `RayResult raycast_fire(Map *map, Camera *cam, double ray_angle, double max_dist);` |
| `raycast_render` | `src/raycast.h` | `void raycast_render(Grid *grid, Map *map, Camera *cam, AssetRegistry *assets, WorldState *world);` |
| `side` semantics | `src/raycast.h:27-28` | `0` = X-axis step; `1` = Y-axis step |
| `grid_print` | `src/grid.h:54` | `void grid_print(Grid *grid, int x, int y, const char *text, SDL_Color fg, SDL_Color bg);` |
| `input_process` | `src/input.h` | `void input_process(InputState *input, bool headless_mode);` |
| `InputState` | `src/input.h` | movement held flags + designer edge flags; no unified-editor actions yet |
| map load | `src/map_loader.c` | `Map* map_load_from_string(const char *map_txt);` digits `0`-`9`; non-digits → default material |
| map file load | `src/asset_loader.c` | `Map* asset_loader_load_map_data(WorldState *world, const char *base_path, int map_id);` |
| map save | — | **none existed at Phase P**; SceneDocument serializer added in Phase 1 |
| `APP_STATE_EDITOR` branch | `src/app.c:807-813` | placeholder `grid_print` only |
| `world_init` | `src/world.h` | app-owned runtime lights/decals/spawn; not part of SceneDocument MVP |
| Makefile tests | `Makefile` | historically `TEST_SRC := $(filter-out src/main.c src/app.c, $(SRC_FILES))` |

### Camera convention note

`camera_init` documentation states yaw `0` = east. Face-from-ray tests in
Phase 3 must lock cardinal faces against this convention.

### Entry-path gap (implementation decision deferred to Phase 4)

Nothing currently transitions into `APP_STATE_EDITOR`. Main menu
`open_asset_editor` opens `MENU_ASSET_SELECT` (live/decals/materials).
Phase 4 must add a concrete entry into the unified editor; Phase 7 replaces
legacy menu entries with one Editor action.

### Map ownership note

Production map APIs allocate `Map*` via `map_create` / `map_destroy`.
`SceneDocument` embeds `Map` by value per plan. Load/save code must move
fields explicitly and reset source pointers; no shallow copy of a live map.

## Phase 0 decisions

1. Create `src/editor_types.h` exactly as specified by the plan.
2. Replace broad test linkage with named per-test source groups.
3. Propagate renderer/lighting feature flags into test recipes that compile
   those modules so `make test USE_*=1` remains meaningful.
4. Keep default app build as `$(wildcard src/*.c)`.

## Phase 0 deliverables

| Path | Change |
|---|---|
| `src/editor_types.h` | Created: `DocumentStateId`, `MaterialId`, `WallFace`, refs, `SelectionTarget`, `EditorHit` |
| `Makefile` | Replaced broad `TEST_SRC` filter with named `TEST_*_SRC` groups |
| `docs/EDITOR_UNIFICATION_IMPLEMENTATION_NOTES.md` | Phase P + Phase 0 log |

### Per-test source groups

| Runner | Production sources |
|---|---|
| `test-deps` | none (deps only) |
| `test-menu-state` | `menu_state.c`, `config.c` |
| `test-ui-ele` | `ui_ele.c`, `grid.c` |
| `test-decal-io` | `decal_io`, `assets`, `world`, `map`, `asset_loader`, `map_loader`, `config` |
| `test-asset-designer` | `asset_designer`, `decal_io`, `config`, `input`, `grid`, `assets` |
| `test-live-editor` | live editor + lighting/raycast/ui/map/camera/input stack |
| `test-core` | grid/scale/timing/renderer/math/map/camera/raycast/lighting/config/loaders/assets/world/input |
| `test-decals` | grid/map/camera/raycast/assets/world/lighting/config/loaders/math/input |
| `test-scene-document` | `scene_document`, `map`, `map_loader`, `config` (Phase 1) |
| `test-command-system` | `command_system`, `scene_document`, `map`, `map_loader`, `config` (Phase 2) |
| `test-editor-selection` | `editor_selection`, `raycast`, `camera`, `map`, `config`, `math`, `assets`, `world`, `grid`, `input` (Phase 3) |

Feature-flag modules (`lighting_cache`, `glyph_block_cache`, SMC trackers) are


appended only when the matching `USE_*=1` flag is set. Tests that compile
renderer/lighting/raycast receive `TEST_FEATURE_*` defs/includes/libs.

## Phase 1 decisions

1. Public API matches the plan (`scene_document.h` / `scene_document_internal.h`).
2. Load is transactional: all fallible work uses a temporary heap `Map*` and a
   duplicated path; the live document is replaced only on full success.
3. Ownership move: `map_move_from_heap` copies fields into the embedded `Map`,
   nulls the source pointers, then frees the heap shell (no double-free).
4. Save is atomic: `mkstemp` beside destination → write digit grid → `fflush` →
   `fclose` → `rename`. Failures before rename leave the destination untouched
   and remove the temp file. `saved_state` updates only after successful rename.
5. Serialization writes only `material_id` digits `0`–`9` plus newlines.
   `light_map` is never written. IDs outside `0..9` return
   `SCENE_SAVE_UNREPRESENTABLE_MATERIAL` before any temp is created.
6. Internal mutations (`scene_document_internal_*`) are the only write path for
   map cells and `current_state`. Public code may only load/save/query.
7. `_POSIX_C_SOURCE 200809L` is defined in `scene_document.c` and the test file
   so `mkstemp` / `mkdtemp` / `fdopen` are visible under `-std=c11`.
8. Tests create an isolated `/tmp/tsg_scene_doc_XXXXXX` directory; checked-in
   assets under `assets/maps/` are never written.

## Phase 1 deliverables

| Path | Change |
|---|---|
| `src/scene_document.h` | Public lifecycle, load/save results, accessors, dirty query |
| `src/scene_document_internal.h` | Command-only wall material + current-state setters |
| `src/scene_document.c` | Implementation (transactional load, atomic save, ownership) |
| `tests/test_scene_document.c` | 14 cmocka tests covering plan exit criteria |
| `Makefile` | `TEST_SCENE_DOCUMENT_*` group + runner wired into `make test` |

### Phase 1 test coverage vs plan

| Plan requirement | Test |
|---|---|
| Init/destroy empty | `test_init_destroy_empty` |
| Load valid fixture | `test_load_valid_fixture` |
| Repeated load no leak/double-free | `test_repeated_load_no_leak` |
| Failed load preserves document | `test_failed_load_preserves_document` |
| Save/reload round trip | `test_save_reload_round_trip` |
| Reject material > 9 | `test_save_rejects_material_above_nine` |
| Reject material < 0 | `test_save_rejects_material_below_zero` |
| Save failure leaves `saved_state` | unrepresentable + replace-failed cases |
| Successful save updates `saved_state` | `test_successful_save_updates_saved_state` |
| Failed replace preserves destination | `test_failed_replace_preserves_destination` |
| Temps removed after failure | `test_temp_files_removed_after_unrepresentable`, replace-failed |
| `light_map` not serialized | `test_light_map_not_serialized` |
| Temp-dir-only I/O | group setup `mkdtemp` + teardown |

## Phase 2 decisions

1. Public API matches the plan (`command_system.h` types and functions).
2. Command history is the sole caller of `scene_document_internal_*`.
3. Lazy allocation: `command_history_init` sets `commands = NULL`, capacity 0;
   first successful mutating command grows via overflow-checked `realloc`.
4. Transaction order for `set_wall_material`:
   validate + read old → NO_CHANGE if equal → STATE_ID_EXHAUSTED check →
   reserve capacity → discard redo `[cursor, count)` → build command →
   internal mutate → append/advance cursor/count/next_state_id/current_state.
5. Reserve runs **before** redo discard so a failed grow leaves redo intact.
   Structural note: with invariant `cursor <= count <= capacity`, grow is never
   required while a redo branch exists (`cursor < count` ⇒ `cursor < capacity`).
6. Discarded redo state IDs are never reused; `next_state_id` only advances on
   successful mutation.
7. `initial_state == UINT64_MAX` leaves `next_state_id = UINT64_MAX` so the next
   mutating command returns `CMD_RESULT_STATE_ID_EXHAUSTED`.
8. Test-only allocator hooks (`command_history_set_allocator_for_test` /
   `reset`) inject OOM without changing production defaults.
9. Tests use isolated `/tmp/tsg_cmd_sys_XXXXXX`; checked-in assets are never written.
10. Save after undo marks historical `current_state` clean via existing
    `scene_document_save` (`saved_state = current_state`); history is untouched.

## Phase 2 deliverables

| Path | Change |
|---|---|
| `src/command_system.h` | Public history API, `EditorCommand`, `CommandResult` |
| `src/command_system.c` | Lazy history, set_wall_material, undo/redo, test alloc hooks |
| `tests/test_command_system.c` | 16 cmocka tests covering plan exit criteria |
| `Makefile` | `TEST_COMMAND_SYSTEM_*` group + runner wired into `make test` |

### Phase 2 test coverage vs plan

| Plan requirement | Test |
|---|---|
| First command before/after states | `test_first_command_states` |
| Undo first restores state 1 + old material | `test_undo_first_restores_state_one` |
| Redo restores new material + after-state | `test_redo_restores_new_material` |
| No-change assignment | `test_no_change_assignment` |
| Invalid coordinates | `test_invalid_coordinates` |
| Undo at cursor zero | `test_undo_at_zero` |
| Redo at cursor count | `test_redo_at_count` |
| Execute after undo discards redo | `test_execute_after_undo_discards_redo` |
| Discarded state IDs never reused | `test_discarded_state_ids_never_reused` |
| Undo to saved reports clean | `test_undo_to_saved_reports_clean` |
| Redo away from saved reports dirty | `test_redo_away_from_saved_reports_dirty` |
| Save after undo marks historical clean | `test_save_after_undo_marks_historical_clean` |
| Allocation failure unchanged | `test_allocation_failure_unchanged` |
| State-ID exhaustion | `test_state_id_exhaustion` |
| Multi-step undo/redo chain | `test_multiple_undo_redo_chain` |
| Lazy init / destroy | `test_init_lazy_no_alloc` |

## Verification log

| When | Check | Result |
|---|---|---|
| 2026-07-24 Phase P | Symbol inventory vs headers | Confirmed |
| 2026-07-24 Phase P | Unified module search | Zero matches |
| 2026-07-24 Phase 0 | `make -B all` | Success |
| 2026-07-24 Phase 0 | `make -B test` | All runners passed |
| 2026-07-24 Phase 0 | Compile TU including `editor_types.h` | Success (`build/check_editor_types.o`) |
| 2026-07-24 Phase 0 | `test-menu-state` symbols | Only `menu_stack_*` + `config_*` (narrow linkage confirmed) |
| 2026-07-24 Phase 1 | `make -B all` | Success (includes `scene_document.c` via wildcard) |
| 2026-07-24 Phase 1 | `./build/test-scene-document` | 14/14 passed |
| 2026-07-24 Phase 1 | `make -B test` | All runners passed (deps, core, decals, menu, decal-io, asset-designer, live-editor, ui-ele, scene-document) |
| 2026-07-24 Phase 1 | Checked-in assets modified | None (tests use `/tmp/tsg_scene_doc_*` only) |
| 2026-07-24 Phase 2 | `make -B all` | Success (includes `command_system.c` via wildcard) |
| 2026-07-24 Phase 2 | `./build/test-command-system` | 16/16 passed |
| 2026-07-24 Phase 2 | `make test` | All runners passed including command-system |
| 2026-07-24 Phase 2 | Checked-in assets modified | None (tests use `/tmp/tsg_cmd_sys_*` only) |
| 2026-07-24 Phase 3 | `make -B all` | Success (includes `editor_selection.c` via wildcard) |
| 2026-07-24 Phase 3 | `./build/test-editor-selection` | 12/12 passed |
| 2026-07-24 Phase 3 | `make test` | All runners passed including editor-selection |
| 2026-07-24 Phase 3 | Cardinal face convention locked | 0=+X/east, PI/2=+Y/south; four ray + four pure-face tests |
| 2026-07-24 Phase 4 | `make -B all` | Success (includes `unified_editor.c` via wildcard) |
| 2026-07-24 Phase 4 | `./build/test-unified-editor` | 11/11 passed |
| 2026-07-24 Phase 4 | `make test` | All runners passed including unified-editor + updated ui-ele |
| 2026-07-24 Phase 4 | Main menu entry | `open_level_editor` → `APP_STATE_EDITOR` loads `assets/maps/1.txt` |

## Phase 3 decisions


1. Public API matches the plan (`editor_selection.h` functions).
2. `editor_raycast_selection` reuses `raycast_fire` with center-camera yaw;
   no duplicated DDA.
3. Coordinate convention confirmed against `raycast.c` / `camera.h`:
   angle 0 = +X/east, PI/2 = +Y/south (Y increases south).
4. Face mapping locked (plan §10):
   - side 0, ray_dir_x > 0 → WEST
   - side 0, ray_dir_x < 0 → EAST
   - side 1, ray_dir_y > 0 → NORTH
   - side 1, ray_dir_y < 0 → SOUTH
5. Const-correct public API casts away const only at historical non-const
   map/camera call sites (`raycast_fire`, `map_get`, `map_in_bounds`); those
   callees do not mutate.
6. Max ray distance comes from `config_get()->raycast_max_distance` with a
   positive fallback of 20.0.
7. Validation requires `SELECTION_WALL_FACE`, in-bounds cell, and
   `material_id > 0`.
8. `editor_wall_face_to_material_ref` copies only `map_x` / `map_y`.

## Phase 3 deliverables

| Path | Change |
|---|---|
| `src/editor_selection.h` | Public selection / face helpers |
| `src/editor_selection.c` | Raycast selection, face calc, validation |
| `tests/test_editor_selection.c` | 12 cmocka tests covering plan exit criteria |
| `Makefile` | `TEST_EDITOR_SELECTION_*` group + runner wired into `make test` |

### Phase 3 test coverage vs plan

| Plan requirement | Test |
|---|---|
| No-hit invalid | `test_no_hit_is_invalid` |
| X-positive → west | `test_face_x_positive_is_west`, `test_ray_east_hits_west_face` |
| X-negative → east | `test_face_x_negative_is_east`, `test_ray_west_hits_east_face` |
| Y-positive → north | `test_face_y_positive_is_north`, `test_ray_south_hits_north_face` |
| Y-negative → south | `test_face_y_negative_is_south`, `test_ray_north_hits_south_face` |
| Boundary coordinates safe | `test_boundary_coordinates_safe` |
| Face → material drops face | `test_face_to_material_ref_drops_face` |
| Validation rejects OOB / empty | `test_selection_validation_rejects_oob_and_empty` |

## Phase 4 decisions

1. Public controller API matches the plan shell (init/destroy/load/update/
   render_overlay). **No** material-apply / undo / redo / save wrappers yet —
   those are Phase 5; no stub apply APIs.
2. Edge-triggered editor actions added to `InputState` and reset each frame in
   `input_process` (Tab, E, Enter, Esc, Ctrl+Z/Y/S, F5, Up/Down).
3. Escape is owned by the unified editor while `APP_STATE_EDITOR` is active and
   no menu overlay is open; `app.c` does **not** push `MENU_EDITOR` on Esc.
4. Escape hierarchy: inspector close → exit modal → confirm exit sets
   `request_exit_to_main_menu`.
5. Walk mode calls `camera_update` on the authoritative SceneDocument map;
   edit mode does not move the camera. Tab toggles mode without camera jump.
6. Hover is invalidated at the start of every update; select copies valid hover
   only; invalid select preserves prior selection and sets status.
7. Input consumption: modal/inspector/edit-mode pointer flags prevent one key
   from activating multiple layers (covered by unit tests).
8. Temporary main-menu entry: `LEVEL EDITOR` button with action
   `open_level_editor` (Phase 7 collapses legacy editor menu entries).
9. Entry loads `assets/maps/1.txt` and places the camera at the usual spawn
   `(1.5, 1.5)`.
10. Overlay is temporary `grid_print` status text (mode, hover, selection,
    dirty, shortcuts) — full material picker is Phase 5.
11. UI tests updated for main-menu focusable count 4 and master_map cache_next 6.

## Phase 4 deliverables

| Path | Change |
|---|---|
| `src/unified_editor.h` | Controller state + public shell API |
| `src/unified_editor.c` | Init/load/update/overlay; walk/edit; hover/select; Escape hierarchy |
| `tests/test_unified_editor.c` | 11 cmocka tests covering plan Phase 4 list |
| `src/input.h` / `src/input.c` | Edge-triggered `editor_*_pressed` actions |
| `src/app.c` | `APP_STATE_EDITOR` world render + update; menu dispatch entry |
| `Makefile` | `TEST_UNIFIED_EDITOR_*` group + runner |
| `assets/ui_layouts/main_menu.txt` | Added `main_menu_level_editor` |
| `assets/ui_layouts/master_map.txt` | Cache list includes level-editor element |
| `assets/ui_elements/main_menu_level_editor.txt` | New button (`open_level_editor`) |
| `assets/ui_elements/main_menu_asset_editor.txt` | Y shifted to avoid overlap |
| `tests/test_ui_ele.c` | Main-menu focus + master_map counts updated |

### Phase 4 test coverage vs plan

| Plan requirement | Test |
|---|---|
| Init/destroy | `test_init_destroy`, `test_init_null_rejects` |
| Successful load resets history/selection | `test_load_success_resets_history_and_selection` |
| Failed load preserves state | `test_load_failure_preserves_state` |
| Tab toggles mode without camera move | `test_tab_toggles_mode_without_moving_camera` |
| Hover invalidated each frame | `test_hover_invalidated_each_frame` |
| Select copies valid hover | `test_select_copies_valid_hover` |
| Invalid select preserves prior | `test_invalid_select_preserves_prior` |
| Input consumption multi-layer | `test_input_consumption_blocks_multi_layer` |
| Escape hierarchy | `test_escape_hierarchy_inspector_before_exit` |
| Edit mode consumes pointer | `test_edit_mode_consumes_pointer` |

## Phase 5 preview (next)

Implement material picker + controller wrappers:
`unified_editor_set_wall_material`, undo/redo/save; route through
`command_history_set_wall_material`; dirty/unsaveable status; no world rebuild.



