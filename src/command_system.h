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

typedef enum {
    EDITOR_MUTATION_SET_WALL_MATERIAL = 0,
    EDITOR_MUTATION_SET_FLOOR_MATERIAL,
    EDITOR_MUTATION_SET_CEILING_MATERIAL,
    EDITOR_MUTATION_SET_AMBIENT_INTENSITY,
    EDITOR_MUTATION_PLACE_WALL,
    EDITOR_MUTATION_REMOVE_WALL,
    EDITOR_MUTATION_SET_LIGHT
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
    } data;
} EditorMutationRequest;

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
        } occupancy;
        struct {
            SceneInstanceId id;
            SceneLight before;
            SceneLight after;
        } light;
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
} CommandResult;

typedef struct {
    DocumentStateId before_state;
    DocumentStateId after_state;
    size_t mutation_count;
    EditorMutation mutations[EDITOR_COMMAND_MAX_MUTATIONS];
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
