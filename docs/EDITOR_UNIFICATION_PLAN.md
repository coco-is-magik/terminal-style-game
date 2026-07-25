# Unified In-World Asset and Level Editor

## Implementation Plan: Architectural Foundation and Wall-Material Vertical Slice

## Status

Repository research is complete. Phase P through Phase 4 are complete (2026-07-24).
See `docs/EDITOR_UNIFICATION_IMPLEMENTATION_NOTES.md` for the working log.
Next implementation phase: Phase 5 (Existing-material assignment inspector).






The first vertical slice creates the unified editor application state, permits seamless switching between walking and editing, supports first-person wall selection, allows an existing material to be assigned to the selected wall cell, and provides undo, redo, save, reload, and dirty-state reporting.

This plan does not complete the broader Forge-like editor vision. Map construction, material authoring, painter integration, decals, sprites, animation, placed objects, lights, triggers, and entity identifiers remain later work.

## Product Direction

The world is the editor workspace. Walking, selecting, adjusting, and previewing must occur in one running level without loading a synthetic preview map or entering a separate asset-authoring application state.

The long-term interaction loop is:

```text
Walk through world -> select target -> adjust properties or assets -> preview immediately -> continue walking
```

The first vertical slice proves this architecture with one operation: assigning an existing material to a wall cell.

## Goals

1. Make `APP_STATE_EDITOR` the single future entry point for level and asset editing.
2. Use a real loaded level in the editor rather than a synthetic preview map.
3. Preserve one camera while switching between walk and edit modes.
4. Make authored map data authoritative and directly visible to the renderer.
5. Route every authored mutation through undoable commands.
6. Persist wall-material changes using the existing map format.
7. Keep the existing editor states available until reusable code has been classified and migrated.
8. Remove the legacy editor application states after the unified vertical slice is validated.

## Non-Goals for This Vertical Slice

1. Creating, modifying, renaming, or deleting material assets.
2. Assigning a different material to each face of one wall cell.
3. Editing floors or ceilings.
4. Adding or deleting map cells.
5. Placing lights, decals, sprites, objects, spawn points, or triggers.
6. Integrating the painter or animation editor.
7. Changing the map file format.
8. Introducing stable entity IDs. No placed entities are edited in this slice.

## Verified Repository Inventory

The implementation model must record the current `dev` commit hash before modifying source. The declarations below were verified during plan preparation and must be checked once against that exact commit before editing.

| Item | Location | Verified declaration or behavior | Ownership in MVP |
|---|---|---|---|
| Camera | `src/camera.h:27` | `typedef struct { Entity transform; double fov; double pitch; } Camera;` | Application-owned; borrowed by editor update and render paths |
| Map | `src/map.h:38` | `typedef struct { int width, height; MapCell *cells; double *light_map; } Map;` | Owned by `SceneDocument` while `APP_STATE_EDITOR` is active |
| Map cell material | `src/map.h` | `MapCell.material_id` is `int` | Authored map field |
| Material collection | `src/assets.h:89` | Fixed arrays for palettes, materials, sprites, material names, and `material_count` | Application-owned; borrowed by editor |
| Application state | `src/config.h:118-125` | Includes `APP_STATE_MAIN_MENU`, `APP_STATE_PLAYING`, `APP_STATE_EDITOR`, `APP_STATE_LIVE_EDITOR`, `APP_STATE_ASSET_DESIGNER`, and `APP_STATE_MATERIAL_DESIGNER` | Application-owned |
| Ray result | `src/raycast.h:30` | Contains `hit`, `distance`, `map_x`, `map_y`, and `side` | Caller-owned stack value |
| Ray side semantics | `src/raycast.h:27-28` | `side == 0` is an X-axis grid step; `side == 1` is a Y-axis grid step | Used to derive cardinal wall face |
| Material validation | `src/assets.h:179` | `material_id_is_loaded()` exists | Called by unified editor controller before command execution |
| Bounds validation | `src/map.h:49` | `map_in_bounds()` exists | Called by SceneDocument and command code |
| Grid text output | `src/grid.h:54` | `grid_print()` exists | Used for the temporary keyboard-driven inspector |
| Map format | `src/map_loader.c:127`, `assets/README.md` | One character per cell; decimal digits encode IDs `0` through `9`; non-digits load as the default material ID | No format migration in MVP |
| Unified editor state | `src/app.c:807-813` | `APP_STATE_EDITOR` has an unused placeholder branch | Replaced by the unified editor controller |
| Material designer state | `src/app.c:699-708` | `APP_STATE_MATERIAL_DESIGNER` exists and is dispatched | Retained until Phase 7 |
| Build discovery | `Makefile` | Main sources use `$(wildcard src/*.c)`; test programs currently use a broad filtered source set | Test linkage must be narrowed as modules are added |

## Architectural Decisions

### 1. Controller Ownership

The application owns one `UnifiedEditorState` while `APP_STATE_EDITOR` is active.

`UnifiedEditorState` owns:

1. `SceneDocument`.
2. `CommandHistory`.
3. Hover and persistent selection state.
4. Editor mode and temporary inspector state.
5. Editor status and modal state.

`UnifiedEditorState` borrows:

1. The application camera through function parameters.
2. The application asset collection through a pointer stored in the editor state.
3. The render grid through render-function parameters.

No new `WorldState` abstraction is introduced by this plan. Existing lights, decals, and other application-owned runtime data remain where they currently live and are passed to the existing renderer by `app.c`.

### 2. Authoritative Map Ownership

`SceneDocument` owns the only authored `Map` used by `APP_STATE_EDITOR`.

The editor camera movement, selection raycast, collision checks, and world renderer all receive a pointer to `SceneDocument.map`. No second editable map copy exists inside the editor.

Because all systems read the same map allocation, assigning a wall material requires no world reconstruction and no map synchronization copy. The next world-render pass reads the new material ID directly.

`world_init()` or equivalent full-runtime reconstruction must not be called for a wall-material change, undo, or redo.

### 3. Derived Runtime Data

`Map.cells` is authored data and is serialized.

`Map.light_map` is derived runtime data. `SceneDocument` owns its allocation only because it owns the complete `Map`, but the map serializer must not write `light_map` into the map file.

A wall-material reassignment changes appearance but not wall occupancy or geometry. It therefore must not reset lights, decals, the camera, simulation state, or editor state. If repository inspection identifies a cache keyed directly by material ID, the controller must call the existing narrow invalidation function after execute, undo, and redo. It must not rebuild the world.

The renderer's dirty-cell or SMC tracker remains responsible for detecting changed rendered grid cells. The editor must not manipulate renderer tracker state directly unless an existing public invalidation API explicitly requires it.

### 4. Command Validation Boundary

The unified editor controller validates that a material ID is currently loaded by calling `material_id_is_loaded()`.

The command system validates only document-level concerns:

1. The target coordinate is in bounds.
2. The target refers to an occupied wall cell where required by existing map semantics.
3. The command can allocate history storage.
4. A new document-state ID can be assigned.
5. The requested value differs from the existing value.

The command system does not include or depend on `assets.h`.

Material IDs greater than `9` may be assigned for live preview if they are loaded. Such a document is dirty and temporarily unsaveable under the MVP map format. The inspector must show this immediately. Save validation rejects the document until every cell contains a serializable ID from `0` through `9`.

### 5. History-State Identity

`DocumentStateId` is an immutable identity for an authored document state. It is not a command index or cursor position.

The invariants are:

1. A newly loaded document starts at state `1`.
2. `SceneDocument.current_state` identifies the state currently represented by the map.
3. `SceneDocument.saved_state` identifies the state successfully written to the current path.
4. The document is dirty exactly when `current_state != saved_state`.
5. `CommandHistory.cursor` is the number of commands currently applied and ranges from `0` through `count`.
6. When `cursor < count`, `commands[cursor]` is the next command to redo.
7. Every command stores both `before_state` and `after_state`.
8. A no-change operation does not allocate memory, allocate a state ID, discard redo history, alter the document, or change dirty status.
9. Executing after undo discards commands in `[cursor, count)`.
10. State IDs belonging to discarded commands are never reused.
11. Save does not modify command history. A successful save sets `saved_state = current_state`.
12. A failed save leaves `saved_state` unchanged.
13. Successful scene load clears history and creates a new base state of `1`.
14. State-ID overflow returns an explicit error and performs no mutation.

### 6. Serialization

The MVP preserves the current map format.

The serializer writes exactly one decimal digit per map cell and one newline after each row. It serializes `Map.cells[*].material_id` only. It does not serialize `light_map` or any other derived data.

Before creating a temporary file, save validation checks:

1. The map has positive dimensions.
2. The cell allocation is present.
3. Every cell material ID is between `0` and `9`, inclusive.
4. The stored destination path is nonempty.

A material ID outside `0` through `9` returns `SCENE_SAVE_UNREPRESENTABLE_MATERIAL`. No partial output replaces the existing file.

The loader retains its existing behavior for non-digit input. The new serializer never emits non-digit cell values.

### 7. Atomic Save

A save operation follows this order:

1. Validate the document before opening a file.
2. Create a unique temporary file in the destination directory.
3. Write every row and check every write.
4. Flush the stream and check the result.
5. Close the stream and check the result.
6. Atomically replace the destination using the repository's supported platform behavior.
7. Set `saved_state = current_state` only after replacement succeeds.
8. Remove the temporary file after every failure that occurs before replacement.
9. Preserve the previous destination after failure.

Tests use a test-created temporary directory and never write to checked-in files under `assets/maps/`.

### 8. Camera Ownership and Mode Switching

The application camera remains authoritative. `UnifiedEditorState` does not contain a camera copy.

Switching between walk and edit modes never changes camera position, orientation, FOV, or pitch.

In walk mode, the application calls the existing camera update using `SceneDocument.map`.

In edit mode, camera movement and mouse-look are disabled. The center-camera ray determines hover. The user returns to walk mode to adjust aim and position.

### 9. Input State Machine

The MVP is keyboard-driven. World selection always uses the center crosshair.

Bindings:

| Action | Binding | Behavior |
|---|---|---|
| Toggle walk/edit | `Tab` | Toggles modes when no modal interface is active |
| Select hovered wall | `E` | Copies a valid hover into persistent selection and opens the inspector |
| Picker previous/next | `Up` / `Down` | Moves through loaded materials while inspector has focus |
| Apply picker choice | `Enter` | Assigns the highlighted existing material through the controller command wrapper |
| Undo | `Ctrl+Z` | Undoes one applied command |
| Redo | `Ctrl+Y` | Redoes one command |
| Save | `Ctrl+S` | Saves the current document |
| Reload | `F5` | Reloads the current document after confirmation if dirty |
| Cancel or exit | `Escape` | Closes the inspector first; otherwise opens the editor exit prompt |

The exit prompt provides Resume, Save and Exit, Discard and Exit, and Cancel.

Input priority is:

```text
Exit/reload modal -> focused inspector -> editor shortcuts -> world selection -> camera controls -> application shortcuts
```

Input handling must have a concrete consumption mechanism. Editor actions return consumed keyboard and pointer flags, or equivalent existing input-system state, so one event cannot activate more than one layer.

Save, reload, select, apply, undo, redo, mode toggle, and Escape are edge-triggered. Up and Down may use the existing deliberate key-repeat behavior.

### 10. Selection Semantics

The MVP supports wall faces only.

A persistent `WallFaceRef` records the face the user aimed at. A material mutation converts it to `WallMaterialRef`, which contains only the cell coordinates.

Because the current map stores one material per cell, assigning a material after selecting one face changes the material for the entire wall cell. The inspector must display this limitation explicitly.

For a coordinate system where X increases east and Y increases south, cardinal face calculation is:

```text
side == 0 and ray_dir_x > 0 -> WEST face
side == 0 and ray_dir_x < 0 -> EAST face
side == 1 and ray_dir_y > 0 -> NORTH face
side == 1 and ray_dir_y < 0 -> SOUTH face
```

The implementation must confirm the coordinate convention against camera movement and lock it with four unit tests.

Hover behavior is:

1. Set `hover.valid = false` at the start of every editor update.
2. Fire the center-camera selection ray.
3. Replace hover only after a valid wall hit.
4. A miss leaves hover invalid.
5. Pressing `E` with an invalid hover leaves the prior selection unchanged and sets an invalid-selection status.
6. Successful load clears hover and persistent selection.
7. Execute, undo, and redo revalidate the persistent selection against the current map.

## Shared Editor Types

Create `src/editor_types.h`:

```c
#ifndef EDITOR_TYPES_H
#define EDITOR_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t DocumentStateId;
typedef int MaterialId;

typedef enum {
    WALL_FACE_NORTH = 0,
    WALL_FACE_SOUTH,
    WALL_FACE_EAST,
    WALL_FACE_WEST
} WallFace;

typedef struct {
    int map_x;
    int map_y;
} WallMaterialRef;

typedef struct {
    int map_x;
    int map_y;
    WallFace face;
} WallFaceRef;

typedef enum {
    SELECTION_NONE = 0,
    SELECTION_WALL_FACE
} SelectionType;

typedef struct {
    SelectionType type;
    union {
        WallFaceRef wall_face;
    } value;
} SelectionTarget;

typedef struct {
    SelectionTarget target;
    double distance;
    bool valid;
} EditorHit;

#endif
```

No floor, ceiling, object, or stable entity-ID types are introduced until a phase that uses them.

## SceneDocument API

Create `src/scene_document.h`:

```c
#ifndef SCENE_DOCUMENT_H
#define SCENE_DOCUMENT_H

#include "editor_types.h"
#include "map.h"

#include <stdbool.h>

typedef struct {
    Map map;
    DocumentStateId current_state;
    DocumentStateId saved_state;
    char *path;
} SceneDocument;

typedef enum {
    SCENE_LOAD_OK = 0,
    SCENE_LOAD_FILE_NOT_FOUND,
    SCENE_LOAD_PARSE_ERROR,
    SCENE_LOAD_VALIDATION_FAILED,
    SCENE_LOAD_OUT_OF_MEMORY
} SceneLoadResult;

typedef enum {
    SCENE_SAVE_OK = 0,
    SCENE_SAVE_NO_PATH,
    SCENE_SAVE_INVALID_DOCUMENT,
    SCENE_SAVE_UNREPRESENTABLE_MATERIAL,
    SCENE_SAVE_TEMP_CREATE_FAILED,
    SCENE_SAVE_WRITE_FAILED,
    SCENE_SAVE_FLUSH_FAILED,
    SCENE_SAVE_CLOSE_FAILED,
    SCENE_SAVE_REPLACE_FAILED
} SceneSaveResult;

void scene_document_init(SceneDocument *document);
void scene_document_destroy(SceneDocument *document);

SceneLoadResult scene_document_load(
    SceneDocument *document,
    const char *path
);

SceneSaveResult scene_document_save(SceneDocument *document);

const Map *scene_document_get_map(const SceneDocument *document);
Map *scene_document_get_map_for_runtime(SceneDocument *document);

bool scene_document_get_wall_material(
    const SceneDocument *document,
    WallMaterialRef ref,
    MaterialId *out_material
);

bool scene_document_is_dirty(const SceneDocument *document);
SceneSaveResult scene_document_validate_for_save(
    const SceneDocument *document
);

#endif
```

`scene_document_get_map_for_runtime()` permits existing camera, collision, and rendering functions to receive the authoritative map. It must not be used by editor UI code to mutate authored fields.

Create `src/scene_document_internal.h` for command-system-only operations:

```c
#ifndef SCENE_DOCUMENT_INTERNAL_H
#define SCENE_DOCUMENT_INTERNAL_H

#include "scene_document.h"

bool scene_document_internal_set_wall_material(
    SceneDocument *document,
    WallMaterialRef ref,
    MaterialId material
);

void scene_document_internal_set_current_state(
    SceneDocument *document,
    DocumentStateId state
);

#endif
```

### SceneDocument Load Transaction

`scene_document_load()` performs all fallible work before replacing the active document:

1. Initialize a temporary `Map` using the repository's existing map lifecycle.
2. Load the requested file into the temporary map using the existing loader.
3. Validate map dimensions, cell allocation, and row semantics.
4. Duplicate the path.
5. If any step fails, destroy the temporary map and preserve the old document unchanged.
6. After all steps succeed, destroy the old map and path and move the temporary ownership into the document.
7. Set `current_state = 1` and `saved_state = 1`.

Ordinary shallow copying of an independently owned live `Map` is prohibited except as an explicit ownership move in which the source is immediately reset to an empty destructible state.

## Command System API

Create `src/command_system.h`:

```c
#ifndef COMMAND_SYSTEM_H
#define COMMAND_SYSTEM_H

#include "editor_types.h"
#include "scene_document.h"

#include <stddef.h>

typedef enum {
    CMD_SET_WALL_MATERIAL = 0
} EditorCommandType;

typedef enum {
    CMD_RESULT_OK = 0,
    CMD_RESULT_NO_CHANGE,
    CMD_RESULT_NOTHING_TO_UNDO,
    CMD_RESULT_NOTHING_TO_REDO,
    CMD_RESULT_INVALID_TARGET,
    CMD_RESULT_OUT_OF_MEMORY,
    CMD_RESULT_STATE_ID_EXHAUSTED
} CommandResult;

typedef struct {
    EditorCommandType type;
    DocumentStateId before_state;
    DocumentStateId after_state;
    union {
        struct {
            WallMaterialRef wall;
            MaterialId old_material;
            MaterialId new_material;
        } set_wall_material;
    } data;
} EditorCommand;

typedef struct {
    EditorCommand *commands;
    size_t count;
    size_t cursor;
    size_t capacity;
    DocumentStateId next_state_id;
} CommandHistory;

void command_history_init(
    CommandHistory *history,
    DocumentStateId initial_state
);

void command_history_destroy(CommandHistory *history);

CommandResult command_history_set_wall_material(
    CommandHistory *history,
    SceneDocument *document,
    WallMaterialRef ref,
    MaterialId new_material
);

CommandResult command_history_undo(
    CommandHistory *history,
    SceneDocument *document
);

CommandResult command_history_redo(
    CommandHistory *history,
    SceneDocument *document
);

#endif
```

History allocation is lazy. `command_history_init()` performs no allocation and cannot fail. It sets `count`, `cursor`, and `capacity` to zero, `commands` to `NULL`, and `next_state_id` to `initial_state + 1`. If `initial_state == UINT64_MAX`, the next mutating command returns `CMD_RESULT_STATE_ID_EXHAUSTED`.

Before mutating the document, `command_history_set_wall_material()` performs this transaction:

1. Validate the target and read the authoritative old material.
2. Return `CMD_RESULT_NO_CHANGE` when old and new values match.
3. Check that a new state ID is available.
4. Reserve command capacity, with multiplication and allocation-overflow checks.
5. Discard the redo range `[cursor, count)` without changing `next_state_id`.
6. Create a command whose `before_state` is the document's current state and whose `after_state` is the next unused state ID.
7. Apply the internal SceneDocument mutation.
8. Append the command, advance `cursor` and `count`, advance `next_state_id`, and set the document current state to `after_state`.

If any step before mutation fails, the document and history remain unchanged. The internal mutation cannot fail after successful target validation; if the repository API makes that guarantee impossible, the implementation must roll back before recording the command.

Undo applies the command's old value and restores `before_state` before decrementing the cursor. Redo applies the new value and restores `after_state` before incrementing the cursor.

The UI and controller never construct completed `EditorCommand` values.

## Selection API

Create `src/editor_selection.h`:

```c
#ifndef EDITOR_SELECTION_H
#define EDITOR_SELECTION_H

#include "camera.h"
#include "editor_types.h"
#include "map.h"

EditorHit editor_raycast_selection(
    const Camera *camera,
    const Map *map
);

WallFace editor_calculate_wall_face(
    int side,
    double ray_dir_x,
    double ray_dir_y
);

WallMaterialRef editor_wall_face_to_material_ref(
    WallFaceRef face
);

bool editor_selection_is_valid_for_map(
    SelectionTarget selection,
    const Map *map
);

#endif
```

`editor_raycast_selection()` reuses the existing raycast implementation rather than duplicating DDA traversal.

## Unified Editor Controller

Create `src/unified_editor.h` after verifying and including the exact headers for `AssetRegistry`, `InputState`, `Camera`, and `Grid`.

```c
typedef enum {
    EDITOR_MODE_WALK = 0,
    EDITOR_MODE_EDIT
} EditorMode;

typedef enum {
    EDITOR_MODAL_NONE = 0,
    EDITOR_MODAL_EXIT_PROMPT,
    EDITOR_MODAL_RELOAD_PROMPT
} EditorModal;

typedef enum {
    EDITOR_STATUS_NONE = 0,
    EDITOR_STATUS_SAVED,
    EDITOR_STATUS_SAVE_FAILED,
    EDITOR_STATUS_INVALID_SELECTION,
    EDITOR_STATUS_INVALID_MATERIAL,
    EDITOR_STATUS_UNSAVABLE_MATERIAL_ID,
    EDITOR_STATUS_OUT_OF_MEMORY,
    EDITOR_STATUS_STATE_ID_EXHAUSTED
} EditorStatus;

typedef struct {
    bool keyboard_consumed;
    bool pointer_consumed;
} EditorInputConsumption;

typedef struct {
    EditorMode mode;
    EditorModal modal;
    EditorStatus status;

    SceneDocument document;
    CommandHistory history;

    SelectionTarget selection;
    EditorHit hover;

    AssetRegistry *assets;

    bool inspector_open;
    size_t material_picker_index;
    MaterialId highlighted_material;

    CommandResult last_command_result;
    SceneLoadResult last_load_result;
    SceneSaveResult last_save_result;

    bool request_exit_to_main_menu;
} UnifiedEditorState;
```

The exact public functions are:

```c
bool unified_editor_init(
    UnifiedEditorState *editor,
    AssetRegistry *assets
);

void unified_editor_destroy(UnifiedEditorState *editor);

SceneLoadResult unified_editor_load_scene(
    UnifiedEditorState *editor,
    const char *path
);

EditorInputConsumption unified_editor_update(
    UnifiedEditorState *editor,
    InputState *input,
    Camera *camera,
    double delta_seconds
);

void unified_editor_render_overlay(
    const UnifiedEditorState *editor,
    Grid *grid
);

CommandResult unified_editor_set_wall_material(
    UnifiedEditorState *editor,
    MaterialId material
);

CommandResult unified_editor_undo(UnifiedEditorState *editor);
CommandResult unified_editor_redo(UnifiedEditorState *editor);
SceneSaveResult unified_editor_save(UnifiedEditorState *editor);
```

`unified_editor_load_scene()` is the only application-facing load operation. On success it:

1. Loads the replacement document transactionally.
2. Destroys and reinitializes history at state `1`.
3. Clears hover and selection.
4. Closes the inspector and modals.
5. Rebuilds the picker from loaded materials.
6. Preserves the application camera unless the existing editor-entry contract explicitly initializes it to the level spawn.

On failure it preserves the current document, history, selection, camera, and dirty state.

The controller wrappers validate loaded material IDs, call command-history operations, record the precise command or save result, revalidate selection, and update the user-facing status. There is no public `unified_editor_sync_runtime()` function because the runtime reads the authoritative map directly.

## Inspector UI

The MVP inspector is rendered directly in `unified_editor_render_overlay()` using the verified grid text functions.

It displays:

1. Current editor mode.
2. Hovered wall coordinates, face, distance, and material ID.
3. Persistent selected wall coordinates and face.
4. A warning that material assignment affects the entire wall cell.
5. The current cell material.
6. A scrollable list of loaded material IDs and names.
7. A marker beside the highlighted material.
8. Clean or dirty state.
9. A persistent unsaveable warning when any cell has a material ID outside `0` through `9`.
10. The latest save or command error.
11. Keyboard shortcuts relevant to the current mode.

The picker enumerates actual loaded materials from the repository asset collection. It does not synthesize an integer range.

If the current wall references an unloaded material, the inspector displays the numeric ID as missing and still allows replacement with a loaded material.

When no materials are loaded, the picker displays an empty-state message and disables application of a material.

## Application Integration

`src/app.c` replaces the placeholder `APP_STATE_EDITOR` branch with this sequence:

1. On entry, initialize `UnifiedEditorState` and load the selected level.
2. During update, call `unified_editor_update()`.
3. In walk mode, the controller invokes or permits the existing camera-update path using `SceneDocument.map`.
4. Render the world through the existing raycast renderer using `SceneDocument.map`, the application camera, application assets, and existing runtime lights and decals.
5. Call `unified_editor_render_overlay()` after the world has rendered.
6. When the controller requests exit, destroy editor-owned resources and transition to `APP_STATE_MAIN_MENU`.

`APP_STATE_LIVE_EDITOR`, `APP_STATE_ASSET_DESIGNER`, and `APP_STATE_MATERIAL_DESIGNER` remain functional until Phase 7.

## Input Changes

Update the existing input module in the phase that introduces `UnifiedEditorState`.

Add edge-triggered editor actions to `InputState`, using names consistent with the repository's existing naming conventions. Required semantic actions are:

```text
editor_toggle_mode_pressed
editor_select_pressed
editor_confirm_pressed
editor_cancel_pressed
editor_undo_pressed
editor_redo_pressed
editor_save_pressed
editor_reload_pressed
editor_previous_pressed
editor_next_pressed
```

The input implementation resets edge-triggered actions every frame and sets them from SDL key-down events. It must not derive editor actions from continuously held movement fields.

## Implementation Phases

### Phase P: Commit and Symbol Confirmation

Record the current `dev` commit hash in this document or the implementation work log. Confirm the exact typedef name of the material collection and the exact signatures of camera update, raycast fire, raycast render, map load, input update, and grid text output.

If a verified name in this plan differs from source, update the plan to the source name without changing the selected architecture.

Exit gate:

```text
Repository commit recorded.
Exact referenced declarations copied into implementation notes.
No unresolved ownership or API option remains.
No source file modified yet.
```

### Phase 0: Shared Types and Test-Linkage Foundation

Create `src/editor_types.h`.

Modify the Makefile test structure so each test target links only the production modules it requires. Do not continue linking every new `src/*.c` file into every test executable.

Exit gate:

```text
Default build succeeds.
Existing tests succeed.
A translation unit including editor_types.h compiles with project flags.
Per-test source groups are available for subsequent phases.
```

### Phase 1: SceneDocument and Serialization

Create:

```text
src/scene_document.h
src/scene_document_internal.h
src/scene_document.c
tests/test_scene_document.c
```

Implement lifecycle, transactional load, semantic map validation, atomic save, path ownership, read-only accessors, internal mutation, and dirty-state queries.

Required tests:

1. Initialize and destroy an empty document.
2. Load a valid fixture.
3. Repeated load does not leak or double-free.
4. Failed load preserves the existing document and path.
5. Semantic save and reload round trip.
6. Save rejects a material ID below `0` or above `9`.
7. Save failure leaves `saved_state` unchanged.
8. Successful save updates `saved_state`.
9. Failed replacement preserves the old destination.
10. Temporary files are removed after failure.
11. `light_map` is not serialized.
12. Tests operate only inside a test-created temporary directory.

Exit gate:

```text
Default build succeeds.
Existing tests succeed.
test_scene_document succeeds.
No checked-in asset is modified.
```

### Phase 2: Command History

Create:

```text
src/command_system.h
src/command_system.c
tests/test_command_system.c
```

Implement lazy history allocation, capacity growth with overflow checks, material assignment, undo, redo, branch truncation, and state-ID updates.

Required tests:

1. First command stores correct before and after states.
2. Undo of first command restores state `1` and the old material.
3. Redo restores the new material and command after-state.
4. No-change assignment does not change history or state.
5. Invalid coordinates do not change history or state.
6. Undo at cursor zero returns `CMD_RESULT_NOTHING_TO_UNDO`.
7. Redo at cursor count returns `CMD_RESULT_NOTHING_TO_REDO`.
8. Executing after undo discards the redo branch.
9. Discarded state IDs are never reused.
10. Returning to `saved_state` by undo reports clean.
11. Redo away from `saved_state` reports dirty.
12. Saving after undo marks that historical state clean.
13. Allocation failure leaves history and document unchanged, using an injectable allocator or test hook if supported.
14. State-ID exhaustion leaves history and document unchanged.

Exit gate:

```text
Default build succeeds.
Existing tests succeed.
test_scene_document succeeds.
test_command_system succeeds.
```

### Phase 3: Wall Selection

Create:

```text
src/editor_selection.h
src/editor_selection.c
tests/test_editor_selection.c
```

Reuse the existing raycast path and convert its center-ray hit into `EditorHit` and `WallFaceRef`.

Required tests:

1. No-hit result is invalid.
2. X-positive ray produces west face.
3. X-negative ray produces east face.
4. Y-positive ray produces north face.
5. Y-negative ray produces south face.
6. Boundary coordinates are handled safely.
7. Conversion from face reference to material reference drops only the face.
8. Selection validation rejects an out-of-bounds or no-longer-wall cell.

Exit gate:

```text
Default build succeeds.
Existing and prior new tests succeed.
test_editor_selection succeeds.
```

### Phase 4: Unified Editor Shell and Input

Create:

```text
src/unified_editor.h
src/unified_editor.c
tests/test_unified_editor.c
```

Modify:

```text
src/input.h
src/input.c
src/app.c
Makefile
```

Replace the unused `APP_STATE_EDITOR` placeholder with editor initialization, level loading, update, world rendering through the authoritative map, and overlay rendering.

At this phase the inspector may display selection and status text without applying a material yet. The public API must not contain stubs that claim to apply changes.

Required tests:

1. Initialization and destruction.
2. Successful load resets history and selection.
3. Failed load preserves existing editor state.
4. Tab toggles mode without moving the camera.
5. Hover is invalidated each frame.
6. Select copies a valid hover.
7. Invalid Select leaves prior selection unchanged and sets status.
8. Input consumption prevents one key event from triggering multiple layers.
9. Escape hierarchy closes inspector before opening the exit prompt.

Exit gate:

```text
Default build succeeds.
All existing and new tests succeed.
APP_STATE_EDITOR loads and renders a real level.
Walk/edit switching preserves camera state.
Legacy editor states remain functional.
```

### Phase 5: Existing-Material Assignment Inspector

Implement the keyboard-driven inspector and controller wrappers in `src/unified_editor.c`.

Required behavior:

1. Enumerate loaded material IDs and names.
2. Apply only loaded material IDs.
3. Route assignment through `command_history_set_wall_material()`.
4. Update world appearance on the next render without reconstruction.
5. Provide undo and redo through controller wrappers.
6. Show dirty status.
7. Allow loaded material IDs above `9` for live preview.
8. Immediately show that IDs above `9` make the document unsaveable.
9. Report save errors without discarding history or edits.

Add controller-focused tests for material validation, status mapping, undo, redo, selection revalidation, and unsaveable-state reporting.

Exit gate:

```text
Default build succeeds.
All tests succeed.
Material assignment is visible immediately.
No full-world rebuild occurs.
```

### Phase 6: Vertical-Slice Acceptance

Validate this complete workflow:

1. Open the unified Editor entry from the main menu.
2. Load a real level.
3. Walk using the existing movement and mouse-look controls.
4. Aim at a wall.
5. Press `Tab` to enter edit mode without moving the camera.
6. Press `E` to select the hovered wall face.
7. Navigate the loaded material list.
8. Press `Enter` to assign a material to the selected wall cell.
9. Observe the material change immediately.
10. Press `Ctrl+Z` and observe the old material.
11. Press `Ctrl+Y` and observe the new material.
12. Press `Ctrl+S` and save a serializable document.
13. Reload and confirm persistence.
14. Assign a loaded ID above `9` and confirm immediate unsaveable warning.
15. Attempt Save and confirm the existing map file remains unchanged.
16. Undo or replace the unrepresentable material and save successfully.
17. Confirm dirty state is correct throughout undo, redo, save, and reload.
18. Exit through the dirty-document prompt without silent data loss.

Exit gate:

```text
All automated tests succeed.
All manual workflow steps succeed.
No renderer, camera, light, decal, or asset state is reset by material edits.
```

### Phase 7: Legacy Editor Migration and State Removal

The requirement is to remove the separate editor products and application states, not to discard reusable authoring logic.

Create `docs/EDITOR_LEGACY_SYMBOL_DISPOSITION.md`. For every externally visible function, significant static helper, state structure, and relevant test in these modules, record its current role and one disposition:

```text
remove as obsolete state/UI glue
move unchanged
move with adaptation
retain temporarily as reusable domain code
replace with tested unified-editor behavior
```

Review:

```text
src/asset_designer.c
src/asset_designer.h
src/live_editor.c
src/live_editor.h
src/material_designer.c
src/material_designer.h
related test files
related UI layouts and UI elements
all app-state transitions and menu actions
```

Extract reusable painter, asset I/O, decal, and material-domain behavior before deleting its owning legacy state. Preserve or migrate its tests.

Then:

1. Remove `APP_STATE_ASSET_DESIGNER`, `APP_STATE_LIVE_EDITOR`, and `APP_STATE_MATERIAL_DESIGNER` from the application enum.
2. Remove their dispatch and transition code from `src/app.c`.
3. Replace separate legacy menu actions with one `EDITOR` action that enters `APP_STATE_EDITOR`.
4. Keep `Start Game` and `Quit` entries.
5. Delete a legacy source file only when its disposition table shows no remaining reusable behavior.
6. Remove a legacy test only when it tests intentionally removed behavior and no migrated replacement is required.
7. Update Makefile targets and source groups after each deletion or extraction.

Exit gate:

```text
Main menu contains Start Game, Editor, and Quit.
No application transition references a removed editor state.
Reusable painter and asset-domain code remains available and tested.
All builds and tests succeed.
```

### Phase 8: Documentation and Regression Validation

Update:

```text
README.md
assets/README.md only if controls or format notes belong there
docs/EDITOR_UNIFICATION_PLAN.md status and completed-phase markers
```

Document current controls, the one-material-per-cell limitation, the `0` through `9` save limitation, and the deferred feature scope.

Run the supported build matrix, one dirty-tracking mode at a time:

```bash
make clean && make
make test

make clean && make USE_SMC_STREAM_STATE_TRACKER=1
make test USE_SMC_STREAM_STATE_TRACKER=1

make clean && make USE_DIRTY_CELLS=1
make test USE_DIRTY_CELLS=1

make clean && make USE_LIGHTING_CACHE=1
make test USE_LIGHTING_CACHE=1

make benchmark
make stability
```

If the Makefile does not propagate configuration variables to test targets, correct that behavior or document and use the actual supported invocation.

Exit gate:

```text
Default build and tests succeed.
Preferred SMC stream build and tests succeed.
Dirty-cell reference build and tests succeed.
Lighting-cache build and tests succeed when supported.
Benchmark and stability targets complete without editor regressions.
README describes the unified editor rather than the removed separate editor states.
```

## File Modification Matrix

| File | Phase | Action |
|---|---:|---|
| `docs/EDITOR_UNIFICATION_PLAN.md` | P, 8 | Replace with this resolved plan; record implementation progress |
| `src/editor_types.h` | 0 | Create |
| `Makefile` | 0, 1-4, 7, 8 | Introduce per-test source groups; add and remove targets as modules change; verify flag propagation |
| `src/scene_document.h` | 1 | Create public SceneDocument API |
| `src/scene_document_internal.h` | 1 | Create command-only mutation API |
| `src/scene_document.c` | 1 | Create lifecycle, load, validation, serialization, and atomic save implementation |
| `tests/test_scene_document.c` | 1 | Create |
| `src/command_system.h` | 2 | Create |
| `src/command_system.c` | 2 | Create |
| `tests/test_command_system.c` | 2 | Create |
| `src/editor_selection.h` | 3 | Create |
| `src/editor_selection.c` | 3 | Create |
| `tests/test_editor_selection.c` | 3 | Create |
| `src/unified_editor.h` | 4 | Create after exact dependency headers are confirmed |
| `src/unified_editor.c` | 4-6 | Create controller, interaction, inspector, and overlay implementation |
| `tests/test_unified_editor.c` | 4-6 | Create controller tests; keep SDL/window interaction in manual acceptance where necessary |
| `src/input.h` | 4 | Add semantic edge-triggered editor actions |
| `src/input.c` | 4 | Map bindings and reset edge-triggered actions each frame |
| `src/app.c` | 4, 6, 7 | Replace editor placeholder, integrate world rendering, later remove legacy dispatch |
| `src/config.h` | 7 | Remove only the three legacy editor state constants after validation |
| `assets/ui_layouts/main_menu.txt` | 7 | Replace legacy editor entries with one Editor entry |
| `assets/ui_elements/main_menu_asset_editor.txt` | 7 | Remove or replace according to actual menu references |
| `docs/EDITOR_LEGACY_SYMBOL_DISPOSITION.md` | 7 | Create symbol-level migration record |
| `src/asset_designer.c/.h` | 7 | Extract reusable code, then delete only if obsolete |
| `src/live_editor.c/.h` | 7 | Extract reusable code, then delete only if obsolete |
| `src/material_designer.c/.h` | 7 | Extract reusable code, then delete only if obsolete |
| Legacy editor tests | 7 | Migrate reusable coverage; delete only obsolete state-level tests |
| `README.md` | 8 | Replace legacy editor description and document MVP controls and limits |

## Definition of Done for This Plan

The plan is complete when all of the following are true:

1. `APP_STATE_EDITOR` runs a real loaded level.
2. Walk and edit modes share one camera and one authoritative map.
3. A wall face can be selected from the first-person view.
4. An existing loaded material can be assigned to the selected wall cell.
5. The change appears immediately without rebuilding the world.
6. Undo and redo restore both map values and document-state identities.
7. Save is atomic and persists IDs `0` through `9`.
8. IDs outside the format range are allowed only as visibly unsaveable live edits and are rejected without replacing the destination file.
9. Dirty state remains correct across execute, undo, redo, save, and reload.
10. The separate legacy editor application states and menu entries are removed only after reusable behavior and tests are migrated.
11. The main menu exposes one unified Editor entry.
12. Default, stream-tracker, dirty-cell, and applicable lighting-cache builds pass their regression checks.

Completion of this plan establishes the foundation for later map construction, material authoring, integrated painting, sprites, animation, objects, lights, and the broader in-world asset-editor vision.
