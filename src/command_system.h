/**
 * command_system.h — Undoable authored-document command history
 *
 * Sole caller of scene_document_internal_* mutations. UI and controller submit
 * typed mutation requests; one bounded group commits as one undo/redo state.
 */
#ifndef COMMAND_SYSTEM_H
#define COMMAND_SYSTEM_H

#include "editor_types.h"
#include "scene_document.h"

#include <stddef.h>

#define EDITOR_COMMAND_MAX_MUTATIONS 8U
#define COMMAND_HISTORY_MAX_RETAINED_BYTES (16U * 1024U * 1024U)

typedef enum {
    EDITOR_MUTATION_SET_WALL_MATERIAL = 0,
    EDITOR_MUTATION_SET_FLOOR_MATERIAL,
    EDITOR_MUTATION_SET_CEILING_MATERIAL,
    EDITOR_MUTATION_SET_AMBIENT_INTENSITY,
    EDITOR_MUTATION_PLACE_WALL,
    EDITOR_MUTATION_REMOVE_WALL,
    EDITOR_MUTATION_SET_LIGHT,
    EDITOR_MUTATION_INSERT_LIGHT,
    EDITOR_MUTATION_REMOVE_LIGHT,
    EDITOR_MUTATION_SET_DECAL,
    EDITOR_MUTATION_INSERT_DECAL,
    EDITOR_MUTATION_REMOVE_DECAL,
    EDITOR_MUTATION_SET_SPRITE,
    EDITOR_MUTATION_INSERT_SPRITE,
    EDITOR_MUTATION_REMOVE_SPRITE,
    EDITOR_MUTATION_SET_TRIGGER,
    EDITOR_MUTATION_INSERT_TRIGGER,
    EDITOR_MUTATION_REMOVE_TRIGGER,
    EDITOR_MUTATION_GROW_EAST,
    EDITOR_MUTATION_GROW_SOUTH,
    EDITOR_MUTATION_SHRINK_EAST,
    EDITOR_MUTATION_SHRINK_SOUTH,
    EDITOR_MUTATION_SET_CELL_VERTICAL,
    EDITOR_MUTATION_SET_MOVEMENT_PARAMETERS,
    EDITOR_MUTATION_SET_OPTICAL_MATERIAL,
    EDITOR_MUTATION_SET_OPTICAL_CELL
} EditorMutationType;

typedef struct {
    const AssetRegistry *assets;
    bool has_player_cell;
    int player_map_x;
    int player_map_y;
} CommandExecutionContext;

typedef struct {
    EditorMutationType type;
    union {
        struct {
            WallMaterialRef wall;
            MaterialId material;
        } wall_material;
        struct {
            int map_x;
            int map_y;
            MaterialId material;
        } surface_material;
        struct {
            double intensity;
        } ambient;
        struct {
            int map_x;
            int map_y;
        } occupancy;
        struct {
            SceneInstanceId id;
            SceneLight value;
        } light;
        struct {
            SceneLight value;
        } insert_light;
        struct {
            SceneInstanceId id;
        } remove_light;
        struct {
            SceneInstanceId id;
            SceneDecalInstance value;
        } decal;
        struct {
            SceneDecalInstance value;
        } insert_decal;
        struct {
            SceneInstanceId id;
        } remove_decal;
        struct { SceneInstanceId id; SceneSpriteInstance value; } sprite;
        struct { SceneSpriteInstance value; } insert_sprite;
        struct { SceneInstanceId id; } remove_sprite;
        struct { SceneInstanceId id; SceneTrigger value; } trigger;
        struct { SceneTrigger value; } insert_trigger;
        struct { SceneInstanceId id; } remove_trigger;
        struct { int trigger; } resize;
        struct {
            int map_x;
            int map_y;
            SceneCellVertical value;
        } cell_vertical;
        struct { SceneMovementParameters value; } movement;
        struct {
            uint16_t material_id;
            OpticalExtension value;
        } optical_material;
        struct {
            size_t cell_index;
            OpticalExtension value;
        } optical_cell;
    } data;
} EditorMutationRequest;

typedef struct {
    size_t index;
    SceneDecalInstance value;
} EditorRemovedDecal;

typedef struct {
    EditorMutationType type;
    union {
        struct {
            WallMaterialRef wall;
            MaterialId before;
            MaterialId after;
        } wall_material;
        struct {
            int map_x;
            int map_y;
            MaterialId before;
            MaterialId after;
        } surface_material;
        struct {
            double before;
            double after;
        } ambient;
        struct {
            int map_x;
            int map_y;
            SceneCellOccupancy before;
            SceneCellOccupancy after;
            size_t removed_decal_start;
            size_t removed_decal_count;
        } occupancy;
        struct {
            SceneInstanceId id;
            SceneLight before;
            SceneLight after;
        } light;
        struct {
            SceneLight value;
        } insert_light;
        struct {
            SceneInstanceId id;
            size_t index;
            SceneLight removed_value;
        } remove_light;
        struct {
            SceneInstanceId id;
            SceneDecalInstance before;
            SceneDecalInstance after;
        } decal;
        struct {
            SceneDecalInstance value;
        } insert_decal;
        struct {
            SceneInstanceId id;
            size_t index;
            SceneDecalInstance removed_value;
        } remove_decal;
        struct {
            SceneInstanceId id;
            SceneSpriteInstance before;
            SceneSpriteInstance after;
        } sprite;
        struct { SceneSpriteInstance value; } insert_sprite;
        struct {
            SceneInstanceId id;
            size_t index;
            SceneSpriteInstance removed_value;
        } remove_sprite;
        struct { SceneInstanceId id; SceneTrigger before; SceneTrigger after; } trigger;
        struct { SceneTrigger value; } insert_trigger;
        struct { SceneInstanceId id; size_t index; SceneTrigger removed_value; } remove_trigger;
        struct { int trigger; } resize;
        struct {
            int map_x;
            int map_y;
            SceneCellVertical before;
            SceneCellVertical after;
        } cell_vertical;
        struct {
            SceneMovementParameters before;
            SceneMovementParameters after;
        } movement;
        struct {
            uint16_t material_id;
            OpticalExtension before;
            OpticalExtension after;
        } optical_material;
        struct {
            size_t cell_index;
            OpticalExtension before;
            OpticalExtension after;
        } optical_cell;
    } data;
} EditorMutation;

typedef enum {
    CMD_RESULT_OK = 0,
    CMD_RESULT_NO_CHANGE,
    CMD_RESULT_NOTHING_TO_UNDO,
    CMD_RESULT_NOTHING_TO_REDO,
    CMD_RESULT_INVALID_TARGET,
    CMD_RESULT_MATERIAL_NOT_LOADED,
    CMD_RESULT_WALL_ATTACHMENT_BLOCKED,
    CMD_RESULT_SPAWN_BLOCKED,
    CMD_RESULT_PLAYER_BLOCKED,
    CMD_RESULT_OUT_OF_MEMORY,
    CMD_RESULT_STATE_ID_EXHAUSTED
    ,CMD_RESULT_MAP_LIMIT
    ,CMD_RESULT_RESIZE_BLOCKED
    ,CMD_RESULT_HISTORY_LIMIT
    ,CMD_RESULT_TRIGGER_REFERENCE_BLOCKED
} CommandResult;

typedef struct {
    DocumentStateId before_state;
    DocumentStateId after_state;
    size_t mutation_count;
    EditorMutation mutations[EDITOR_COMMAND_MAX_MUTATIONS];
    EditorRemovedDecal *removed_decals;
    size_t removed_decal_count;
} EditorCommand;

typedef struct {
    EditorCommand *commands;
    size_t count;
    size_t cursor;
    size_t capacity;
    DocumentStateId next_state_id;
} CommandHistory;

void command_history_init(CommandHistory *history, DocumentStateId initial_state);
void command_history_destroy(CommandHistory *history);
size_t command_history_retained_bytes(const CommandHistory *history);

CommandResult command_history_execute_group(
    CommandHistory *history,
    SceneDocument *document,
    const EditorMutationRequest *requests,
    size_t request_count
);

CommandResult command_history_execute_group_checked(
    CommandHistory *history,
    SceneDocument *document,
    const EditorMutationRequest *requests,
    size_t request_count,
    const CommandExecutionContext *context
);

CommandResult command_history_set_wall_material(
    CommandHistory *history,
    SceneDocument *document,
    WallMaterialRef ref,
    MaterialId new_material
);

CommandResult command_history_set_surface_material(
    CommandHistory *history,
    SceneDocument *document,
    int map_x,
    int map_y,
    SceneSurfaceKind surface,
    MaterialId new_material,
    const CommandExecutionContext *context
);

CommandResult command_history_set_ambient_intensity(
    CommandHistory *history,
    SceneDocument *document,
    double intensity
);
CommandResult command_history_set_cell_vertical(
    CommandHistory *history, SceneDocument *document,
    int map_x, int map_y, const SceneCellVertical *value,
    const CommandExecutionContext *context
);
CommandResult command_history_set_movement_parameters(
    CommandHistory *history, SceneDocument *document,
    const SceneMovementParameters *value
);
CommandResult command_history_set_optical_material_extension(
    CommandHistory *history, SceneDocument *document, uint16_t material_id,
    const OpticalExtension *value
);
CommandResult command_history_set_optical_cell_extension(
    CommandHistory *history, SceneDocument *document, size_t cell_index,
    const OpticalExtension *value
);

CommandResult command_history_place_wall(
    CommandHistory *history,
    SceneDocument *document,
    int map_x,
    int map_y,
    const CommandExecutionContext *context
);

CommandResult command_history_remove_wall(
    CommandHistory *history,
    SceneDocument *document,
    int map_x,
    int map_y,
    const CommandExecutionContext *context
);

CommandResult command_history_set_light(
    CommandHistory *history,
    SceneDocument *document,
    SceneInstanceId id,
    const SceneLight *value
);

CommandResult command_history_insert_light(
    CommandHistory *history,
    SceneDocument *document,
    const SceneLight *prototype,
    SceneInstanceId *out_id
);

CommandResult command_history_remove_light(
    CommandHistory *history,
    SceneDocument *document,
    SceneInstanceId id
);

CommandResult command_history_set_decal(
    CommandHistory *history,
    SceneDocument *document,
    SceneInstanceId id,
    const SceneDecalInstance *value
);

CommandResult command_history_insert_decal(
    CommandHistory *history,
    SceneDocument *document,
    const SceneDecalInstance *prototype,
    SceneInstanceId *out_id
);
CommandResult command_history_insert_decals(
    CommandHistory *history,
    SceneDocument *document,
    const SceneDecalInstance *prototypes,
    size_t prototype_count
);

CommandResult command_history_remove_decal(
    CommandHistory *history,
    SceneDocument *document,
    SceneInstanceId id
);

CommandResult command_history_set_sprite(
    CommandHistory *history, SceneDocument *document, SceneInstanceId id,
    const SceneSpriteInstance *value
);
CommandResult command_history_insert_sprite(
    CommandHistory *history, SceneDocument *document,
    const SceneSpriteInstance *prototype, SceneInstanceId *out_id
);
CommandResult command_history_remove_sprite(
    CommandHistory *history, SceneDocument *document, SceneInstanceId id
);
CommandResult command_history_set_trigger(
    CommandHistory *history, SceneDocument *document, SceneInstanceId id,
    const SceneTrigger *value
);
CommandResult command_history_insert_trigger(
    CommandHistory *history, SceneDocument *document,
    const SceneTrigger *prototype, SceneInstanceId *out_id
);
CommandResult command_history_remove_trigger(
    CommandHistory *history, SceneDocument *document, SceneInstanceId id
);

CommandResult command_history_undo(CommandHistory *history, SceneDocument *document);
CommandResult command_history_redo(CommandHistory *history, SceneDocument *document);
CommandResult command_history_undo_checked(
    CommandHistory *history,
    SceneDocument *document,
    const CommandExecutionContext *context
);
CommandResult command_history_redo_checked(
    CommandHistory *history,
    SceneDocument *document,
    const CommandExecutionContext *context
);

#endif /* COMMAND_SYSTEM_H */
