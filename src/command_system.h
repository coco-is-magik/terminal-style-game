/**
 * command_system.h — Undoable authored-document command history
 *
 * Sole caller of scene_document_internal_* mutations. UI and controller
 * never construct completed EditorCommand values; they call the history
 * wrappers below.
 */

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

#endif /* COMMAND_SYSTEM_H */
