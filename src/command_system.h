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
    EDITOR_MUTATION_SET_LIGHT
} EditorMutationType;

typedef struct {
    EditorMutationType type;
    union {
        struct {
            WallMaterialRef wall;
            MaterialId material;
        } wall_material;
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

CommandResult command_history_set_wall_material(
    CommandHistory *history,
    SceneDocument *document,
    WallMaterialRef ref,
    MaterialId new_material
);

CommandResult command_history_set_light(
    CommandHistory *history,
    SceneDocument *document,
    SceneInstanceId id,
    const SceneLight *value
);

CommandResult command_history_undo(CommandHistory *history, SceneDocument *document);
CommandResult command_history_redo(CommandHistory *history, SceneDocument *document);

#endif /* COMMAND_SYSTEM_H */
