/**
 * command_system.c — CommandHistory for undoable SceneDocument mutations
 *
 * Transaction order for set_wall_material (plan Phase 2):
 *   1. Validate target and read old material
 *   2. NO_CHANGE if values match
 *   3. Check state-ID availability
 *   4. Reserve capacity (overflow-checked)
 *   5. Discard redo range [cursor, count) without changing next_state_id
 *   6. Build command (before/after states)
 *   7. Apply internal document mutation
 *   8. Append, advance cursor/count/next_state_id, set document current_state
 *
 * Failures before mutation leave history and document unchanged.
 */

#include "command_system.h"
#include "scene_document_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ===================================================================
 *  Allocator hook (tests may override to force OOM)
 * =================================================================== */

typedef void *(*CmdAllocFn)(size_t size);
typedef void *(*CmdReallocFn)(void *ptr, size_t size);
typedef void (*CmdFreeFn)(void *ptr);

static void *cmd_default_alloc(size_t size) { return malloc(size); }
static void *cmd_default_realloc(void *ptr, size_t size) { return realloc(ptr, size); }
static void cmd_default_free(void *ptr) { free(ptr); }

static CmdAllocFn g_cmd_alloc = cmd_default_alloc;
static CmdReallocFn g_cmd_realloc = cmd_default_realloc;
static CmdFreeFn g_cmd_free = cmd_default_free;

/* Test-only: declared in command_system.c; tests extern these. */
void command_history_set_allocator_for_test(
    void *(*alloc_fn)(size_t),
    void *(*realloc_fn)(void *, size_t),
    void (*free_fn)(void *)
) {
    g_cmd_alloc = alloc_fn ? alloc_fn : cmd_default_alloc;
    g_cmd_realloc = realloc_fn ? realloc_fn : cmd_default_realloc;
    g_cmd_free = free_fn ? free_fn : cmd_default_free;
}

void command_history_reset_allocator_for_test(void) {
    g_cmd_alloc = cmd_default_alloc;
    g_cmd_realloc = cmd_default_realloc;
    g_cmd_free = cmd_default_free;
}

/* ===================================================================
 *  Capacity
 * =================================================================== */

static const size_t CMD_INITIAL_CAPACITY = 8;

/*
 * Ensure capacity for at least one more command at cursor (after discard,
 * count will become cursor + 1). Returns false on OOM or size overflow.
 */
static bool history_reserve_one(CommandHistory *history) {
    if (history->cursor < history->capacity) {
        return true;
    }

    size_t new_cap;
    if (history->capacity == 0) {
        new_cap = CMD_INITIAL_CAPACITY;
    } else {
        if (history->capacity > SIZE_MAX / 2) {
            return false;
        }
        new_cap = history->capacity * 2;
    }

    if (new_cap > SIZE_MAX / sizeof(EditorCommand)) {
        return false;
    }

    size_t bytes = new_cap * sizeof(EditorCommand);
    EditorCommand *grown = (EditorCommand *)g_cmd_realloc(history->commands, bytes);
    if (!grown) {
        return false;
    }

    history->commands = grown;
    history->capacity = new_cap;
    return true;
}

/* ===================================================================
 *  Lifecycle
 * =================================================================== */

void command_history_init(
    CommandHistory *history,
    DocumentStateId initial_state
) {
    if (!history) return;
    history->commands = NULL;
    history->count = 0;
    history->cursor = 0;
    history->capacity = 0;
    /* next_state_id = initial_state + 1; if initial is UINT64_MAX, stay max
     * so the next mutating command reports STATE_ID_EXHAUSTED. */
    if (initial_state == UINT64_MAX) {
        history->next_state_id = UINT64_MAX;
    } else {
        history->next_state_id = initial_state + 1;
    }
}

void command_history_destroy(CommandHistory *history) {
    if (!history) return;
    g_cmd_free(history->commands);
    history->commands = NULL;
    history->count = 0;
    history->cursor = 0;
    history->capacity = 0;
    history->next_state_id = 0;
}

/* ===================================================================
 *  Mutating command
 * =================================================================== */

CommandResult command_history_set_wall_material(
    CommandHistory *history,
    SceneDocument *document,
    WallMaterialRef ref,
    MaterialId new_material
) {
    if (!history || !document) {
        return CMD_RESULT_INVALID_TARGET;
    }

    MaterialId old_material = 0;
    if (!scene_document_get_wall_material(document, ref, &old_material)) {
        return CMD_RESULT_INVALID_TARGET;
    }

    if (old_material == new_material) {
        return CMD_RESULT_NO_CHANGE;
    }

    if (history->next_state_id == UINT64_MAX) {
        return CMD_RESULT_STATE_ID_EXHAUSTED;
    }

    /* Reserve before discarding redo so a failed grow leaves redo intact. */
    if (!history_reserve_one(history)) {
        return CMD_RESULT_OUT_OF_MEMORY;
    }

    /* Discard redo branch [cursor, count). State IDs are not reused. */
    history->count = history->cursor;

    DocumentStateId before = document->current_state;
    DocumentStateId after = history->next_state_id;

    EditorCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CMD_SET_WALL_MATERIAL;
    cmd.before_state = before;
    cmd.after_state = after;
    cmd.data.set_wall_material.wall = ref;
    cmd.data.set_wall_material.old_material = old_material;
    cmd.data.set_wall_material.new_material = new_material;

    if (!scene_document_internal_set_wall_material(document, ref, new_material)) {
        /* Target was valid at read time; map_set should not fail. Treat as
         * invalid and leave history unchanged (redo already discarded only
         * after reserve succeeded — document still has old material). */
        return CMD_RESULT_INVALID_TARGET;
    }

    history->commands[history->cursor] = cmd;
    history->cursor += 1;
    history->count = history->cursor;
    history->next_state_id = after + 1;
    scene_document_internal_set_current_state(document, after);

    return CMD_RESULT_OK;
}

/* ===================================================================
 *  Undo / Redo
 * =================================================================== */

CommandResult command_history_undo(
    CommandHistory *history,
    SceneDocument *document
) {
    if (!history || !document) {
        return CMD_RESULT_INVALID_TARGET;
    }
    if (history->cursor == 0) {
        return CMD_RESULT_NOTHING_TO_UNDO;
    }

    EditorCommand *cmd = &history->commands[history->cursor - 1];
    switch (cmd->type) {
        case CMD_SET_WALL_MATERIAL:
            if (!scene_document_internal_set_wall_material(
                    document,
                    cmd->data.set_wall_material.wall,
                    cmd->data.set_wall_material.old_material)) {
                return CMD_RESULT_INVALID_TARGET;
            }
            scene_document_internal_set_current_state(document, cmd->before_state);
            history->cursor -= 1;
            return CMD_RESULT_OK;
        default:
            return CMD_RESULT_INVALID_TARGET;
    }
}

CommandResult command_history_redo(
    CommandHistory *history,
    SceneDocument *document
) {
    if (!history || !document) {
        return CMD_RESULT_INVALID_TARGET;
    }
    if (history->cursor >= history->count) {
        return CMD_RESULT_NOTHING_TO_REDO;
    }

    EditorCommand *cmd = &history->commands[history->cursor];
    switch (cmd->type) {
        case CMD_SET_WALL_MATERIAL:
            if (!scene_document_internal_set_wall_material(
                    document,
                    cmd->data.set_wall_material.wall,
                    cmd->data.set_wall_material.new_material)) {
                return CMD_RESULT_INVALID_TARGET;
            }
            scene_document_internal_set_current_state(document, cmd->after_state);
            history->cursor += 1;
            return CMD_RESULT_OK;
        default:
            return CMD_RESULT_INVALID_TARGET;
    }
}
