/** command_system.c — bounded grouped authored-document transactions */
#include "command_system.h"
#include "scene_document_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef void *(*CmdReallocFn)(void *ptr, size_t size);
typedef void (*CmdFreeFn)(void *ptr);
static void *cmd_default_realloc(void *ptr, size_t size) { return realloc(ptr, size); }
static void cmd_default_free(void *ptr) { free(ptr); }
static CmdReallocFn g_cmd_realloc = cmd_default_realloc;
static CmdFreeFn g_cmd_free = cmd_default_free;

void command_history_set_allocator_for_test(
    void *(*alloc_fn)(size_t), void *(*realloc_fn)(void *, size_t),
    void (*free_fn)(void *)) {
    (void)alloc_fn;
    g_cmd_realloc = realloc_fn ? realloc_fn : cmd_default_realloc;
    g_cmd_free = free_fn ? free_fn : cmd_default_free;
}
void command_history_reset_allocator_for_test(void) {
    g_cmd_realloc = cmd_default_realloc;
    g_cmd_free = cmd_default_free;
}

static bool history_reserve_one(CommandHistory *history) {
    size_t new_capacity;
    EditorCommand *grown;
    if (history->cursor < history->capacity) return true;
    new_capacity = history->capacity ? history->capacity * 2U : 8U;
    if (new_capacity < history->capacity ||
        new_capacity > SIZE_MAX / sizeof(*history->commands)) return false;
    grown = g_cmd_realloc(history->commands,
                          new_capacity * sizeof(*history->commands));
    if (!grown) return false;
    history->commands = grown;
    history->capacity = new_capacity;
    return true;
}

void command_history_init(CommandHistory *history, DocumentStateId initial_state) {
    if (!history) return;
    memset(history, 0, sizeof(*history));
    history->next_state_id = initial_state == UINT64_MAX
        ? UINT64_MAX : initial_state + 1U;
}
void command_history_destroy(CommandHistory *history) {
    if (!history) return;
    g_cmd_free(history->commands);
    memset(history, 0, sizeof(*history));
}

static bool lights_equal(const SceneLight *a, const SceneLight *b) {
    return a->id == b->id && a->x == b->x && a->y == b->y &&
        a->red == b->red && a->green == b->green && a->blue == b->blue &&
        a->alpha == b->alpha && a->intensity == b->intensity &&
        a->radius == b->radius;
}

static bool mutations_target_same_object(
    const EditorMutation *a,
    const EditorMutation *b
) {
    if (a->type != b->type) return false;
    if (a->type == EDITOR_MUTATION_SET_WALL_MATERIAL) {
        return a->data.wall_material.wall.map_x == b->data.wall_material.wall.map_x &&
            a->data.wall_material.wall.map_y == b->data.wall_material.wall.map_y;
    }
    if (a->type == EDITOR_MUTATION_SET_LIGHT) {
        return a->data.light.id == b->data.light.id;
    }
    return false;
}

static bool prepare_mutation(SceneDocument *document,
                             const EditorMutationRequest *request,
                             EditorMutation *mutation, bool *changed) {
    memset(mutation, 0, sizeof(*mutation));
    mutation->type = request->type;
    if (request->type == EDITOR_MUTATION_SET_WALL_MATERIAL) {
        MaterialId before;
        if (!scene_document_get_wall_material(document,
                request->data.wall_material.wall, &before) || before <= 0)
            return false;
        mutation->data.wall_material.wall = request->data.wall_material.wall;
        mutation->data.wall_material.before = before;
        mutation->data.wall_material.after = request->data.wall_material.material;
        *changed = before != request->data.wall_material.material;
        return true;
    }
    if (request->type == EDITOR_MUTATION_SET_LIGHT) {
        const SceneLight *before = scene_document_find_light(
            document, request->data.light.id);
        if (!before || !scene_document_internal_light_value_is_valid(
                document, request->data.light.id, &request->data.light.value))
            return false;
        mutation->data.light.id = request->data.light.id;
        mutation->data.light.before = *before;
        mutation->data.light.after = request->data.light.value;
        *changed = !lights_equal(before, &request->data.light.value);
        return true;
    }
    return false;
}

static bool apply_mutation(SceneDocument *document,
                           const EditorMutation *mutation, bool after) {
    if (mutation->type == EDITOR_MUTATION_SET_WALL_MATERIAL) {
        return scene_document_internal_set_wall_material(
            document, mutation->data.wall_material.wall,
            after ? mutation->data.wall_material.after
                  : mutation->data.wall_material.before);
    }
    if (mutation->type == EDITOR_MUTATION_SET_LIGHT) {
        return scene_document_internal_set_light(
            document, mutation->data.light.id,
            after ? &mutation->data.light.after : &mutation->data.light.before);
    }
    return false;
}

CommandResult command_history_execute_group(
    CommandHistory *history, SceneDocument *document,
    const EditorMutationRequest *requests, size_t request_count) {
    EditorCommand command;
    size_t i;
    if (!history || !document || !requests || request_count == 0U ||
        request_count > EDITOR_COMMAND_MAX_MUTATIONS) return CMD_RESULT_INVALID_TARGET;
    memset(&command, 0, sizeof(command));
    for (i = 0U; i < request_count; i++) {
        EditorMutation mutation;
        bool changed = false;
        size_t j;
        if (!prepare_mutation(document, &requests[i], &mutation, &changed))
            return CMD_RESULT_INVALID_TARGET;
        if (!changed) continue;
        for (j = 0U; j < command.mutation_count; j++) {
            if (mutations_target_same_object(&command.mutations[j], &mutation))
                return CMD_RESULT_INVALID_TARGET;
        }
        command.mutations[command.mutation_count++] = mutation;
    }
    if (command.mutation_count == 0U) return CMD_RESULT_NO_CHANGE;
    if (history->next_state_id == UINT64_MAX) return CMD_RESULT_STATE_ID_EXHAUSTED;
    if (!history_reserve_one(history)) return CMD_RESULT_OUT_OF_MEMORY;

    command.before_state = document->current_state;
    command.after_state = history->next_state_id;
    for (i = 0U; i < command.mutation_count; i++) {
        if (!apply_mutation(document, &command.mutations[i], true)) {
            while (i > 0U) {
                i--;
                (void)apply_mutation(document, &command.mutations[i], false);
            }
            return CMD_RESULT_INVALID_TARGET;
        }
    }
    history->count = history->cursor;
    history->commands[history->cursor++] = command;
    history->count = history->cursor;
    history->next_state_id = command.after_state + 1U;
    scene_document_internal_set_current_state(document, command.after_state);
    return CMD_RESULT_OK;
}

CommandResult command_history_set_wall_material(
    CommandHistory *history, SceneDocument *document,
    WallMaterialRef ref, MaterialId material) {
    EditorMutationRequest request = {0};
    request.type = EDITOR_MUTATION_SET_WALL_MATERIAL;
    request.data.wall_material.wall = ref;
    request.data.wall_material.material = material;
    return command_history_execute_group(history, document, &request, 1U);
}

CommandResult command_history_set_light(
    CommandHistory *history, SceneDocument *document,
    SceneInstanceId id, const SceneLight *value) {
    EditorMutationRequest request = {0};
    if (!value) return CMD_RESULT_INVALID_TARGET;
    request.type = EDITOR_MUTATION_SET_LIGHT;
    request.data.light.id = id;
    request.data.light.value = *value;
    return command_history_execute_group(history, document, &request, 1U);
}

CommandResult command_history_undo(CommandHistory *history, SceneDocument *document) {
    EditorCommand *command;
    size_t i;
    if (!history || !document) return CMD_RESULT_INVALID_TARGET;
    if (history->cursor == 0U) return CMD_RESULT_NOTHING_TO_UNDO;
    command = &history->commands[history->cursor - 1U];
    for (i = command->mutation_count; i > 0U; i--) {
        if (!apply_mutation(document, &command->mutations[i - 1U], false)) {
            size_t rollback;
            for (rollback = i; rollback < command->mutation_count; rollback++) {
                (void)apply_mutation(document, &command->mutations[rollback], true);
            }
            return CMD_RESULT_INVALID_TARGET;
        }
    }
    scene_document_internal_set_current_state(document, command->before_state);
    history->cursor--;
    return CMD_RESULT_OK;
}

CommandResult command_history_redo(CommandHistory *history, SceneDocument *document) {
    EditorCommand *command;
    size_t i;
    if (!history || !document) return CMD_RESULT_INVALID_TARGET;
    if (history->cursor >= history->count) return CMD_RESULT_NOTHING_TO_REDO;
    command = &history->commands[history->cursor];
    for (i = 0U; i < command->mutation_count; i++) {
        if (!apply_mutation(document, &command->mutations[i], true)) {
            while (i > 0U) {
                i--;
                (void)apply_mutation(document, &command->mutations[i], false);
            }
            return CMD_RESULT_INVALID_TARGET;
        }
    }
    scene_document_internal_set_current_state(document, command->after_state);
    history->cursor++;
    return CMD_RESULT_OK;
}
