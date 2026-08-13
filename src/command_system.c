/** command_system.c — bounded grouped authored-document transactions */
#include "command_system.h"
#include "scene_document_internal.h"

#include <math.h>
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
    size_t i;
    if (!history) return;
    for (i = 0U; i < history->count; i++)
        g_cmd_free(history->commands[i].removed_decals);
    g_cmd_free(history->commands);
    memset(history, 0, sizeof(*history));
}

static bool lights_equal(const SceneLight *a, const SceneLight *b) {
    return a->id == b->id && a->x == b->x && a->y == b->y &&
        a->red == b->red && a->green == b->green && a->blue == b->blue &&
        a->alpha == b->alpha && a->intensity == b->intensity &&
        a->radius == b->radius;
}

static bool decals_equal(
    const SceneDecalInstance *a, const SceneDecalInstance *b
) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

static bool decal_value_common_is_valid(
    const SceneDocument *document, const SceneDecalInstance *value
) {
    if (!document || !value || value->id == SCENE_INSTANCE_ID_INVALID ||
        value->asset.kind != SCENE_ASSET_KIND_DECAL_PATTERN || value->asset.id == 0U ||
        value->surface < SCENE_DECAL_SURFACE_WALL ||
        value->surface > SCENE_DECAL_SURFACE_CEILING ||
        !isfinite(value->width) || value->width <= 0.0 ||
        !isfinite(value->height) || value->height <= 0.0 ||
        !isfinite(value->glyph_step_u) || value->glyph_step_u < 0.0 ||
        !isfinite(value->glyph_step_v) || value->glyph_step_v < 0.0 ||
        !isfinite(value->depth) || value->depth < 0.0 ||
        !isfinite(value->rotation)) return false;
    if (value->surface == SCENE_DECAL_SURFACE_WALL) {
        return map_in_bounds(&document->map, value->map_x, value->map_y) &&
            (value->side == 0 || value->side == 1) &&
            isfinite(value->u) && value->u >= 0.0 && value->u <= 1.0 &&
            isfinite(value->v) && value->v >= 0.0 && value->v <= 1.0;
    }
    return isfinite(value->x) && isfinite(value->y) && isfinite(value->z) &&
        value->x >= 0.0 && value->x < document->map.width &&
        value->y >= 0.0 && value->y < document->map.height;
}

static bool find_decal_index(
    const SceneDocument *document, SceneInstanceId id, size_t *out_index
) {
    size_t i;
    if (!document || id == SCENE_INSTANCE_ID_INVALID || !out_index) return false;
    for (i = 0U; i < document->decal_count; i++) {
        if (document->decals[i].id == id) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static bool wall_decal_supports_cell(
    const SceneDecalInstance *decal, int map_x, int map_y
) {
    int support_x;
    int support_y;
    if (!decal || decal->surface != SCENE_DECAL_SURFACE_WALL) return false;
    support_x = decal->map_x;
    support_y = decal->map_y;
    if (decal->side == 0 && cos(decal->rotation) < 0.0) support_x++;
    if (decal->side == 1 && sin(decal->rotation) < 0.0) support_y++;
    return support_x == map_x && support_y == map_y;
}

static bool light_value_insert_is_valid(
    const SceneDocument *document,
    const SceneLight *value
) {
    if (!document || !value || value->id == SCENE_INSTANCE_ID_INVALID ||
        !isfinite(value->x) || !isfinite(value->y) ||
        !isfinite(value->intensity) || !isfinite(value->radius) ||
        value->x < 0.0 || value->y < 0.0 ||
        value->x >= document->map.width || value->y >= document->map.height ||
        value->radius <= 0.0 ||
        document->light_count >= SCENE_MAX_LIGHTS ||
        scene_document_find_light(document, value->id) != NULL) {
        return false;
    }
    return true;
}

static bool find_light_index(
    const SceneDocument *document,
    SceneInstanceId id,
    size_t *out_index
) {
    size_t i;
    if (!document || id == SCENE_INSTANCE_ID_INVALID || !out_index) return false;
    for (i = 0U; i < document->light_count; i++) {
        if (document->lights[i].id == id) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static bool mutation_is_surface(EditorMutationType type) {
    return type == EDITOR_MUTATION_SET_WALL_MATERIAL ||
        type == EDITOR_MUTATION_SET_FLOOR_MATERIAL ||
        type == EDITOR_MUTATION_SET_CEILING_MATERIAL;
}
static SceneSurfaceKind mutation_surface(EditorMutationType type) {
    if (type == EDITOR_MUTATION_SET_FLOOR_MATERIAL) return SCENE_SURFACE_FLOOR;
    if (type == EDITOR_MUTATION_SET_CEILING_MATERIAL) return SCENE_SURFACE_CEILING;
    return SCENE_SURFACE_WALL;
}
static bool mutation_is_occupancy(EditorMutationType type) {
    return type == EDITOR_MUTATION_PLACE_WALL ||
        type == EDITOR_MUTATION_REMOVE_WALL;
}
static bool mutation_is_resize(EditorMutationType type) {
    return type == EDITOR_MUTATION_GROW_EAST ||
        type == EDITOR_MUTATION_GROW_SOUTH ||
        type == EDITOR_MUTATION_SHRINK_EAST ||
        type == EDITOR_MUTATION_SHRINK_SOUTH;
}

static bool shrink_ring_matches_source(const SceneDocument *document, bool east,
                                       int trigger) {
    int limit = east ? document->map.height : document->map.width;
    int outer = (east ? document->map.width : document->map.height) - 1;
    int inner = outer - 1;
    int i;
    for (i = 0; i < limit; i++) {
        int inner_x = east ? inner : i;
        int inner_y = east ? i : inner;
        int outer_x = east ? outer : i;
        int outer_y = east ? i : outer;
        size_t inner_index = (size_t)inner_y * (size_t)document->map.width +
                             (size_t)inner_x;
        size_t outer_index = (size_t)outer_y * (size_t)document->map.width +
                             (size_t)outer_x;
        SceneAuthoredCell expected = document->authored_cells[inner_index];
        if (i == trigger) expected.occupancy = SCENE_CELL_OCCUPANCY_WALL;
        {
            const SceneAuthoredCell *actual = &document->authored_cells[outer_index];
            if (expected.occupancy != actual->occupancy ||
                expected.wall_material != actual->wall_material ||
                expected.floor_material != actual->floor_material ||
                expected.ceiling_material != actual->ceiling_material) return false;
        }
    }
    return true;
}

static bool mutations_target_same_field(
    const EditorMutation *a, const EditorMutation *b
) {
    if (mutation_is_surface(a->type) && mutation_is_surface(b->type)) {
        return a->type == b->type &&
            a->data.surface_material.map_x == b->data.surface_material.map_x &&
            a->data.surface_material.map_y == b->data.surface_material.map_y;
    }
    if (mutation_is_occupancy(a->type) && mutation_is_occupancy(b->type)) {
        return a->data.occupancy.map_x == b->data.occupancy.map_x &&
            a->data.occupancy.map_y == b->data.occupancy.map_y;
    }
    if (a->type == EDITOR_MUTATION_SET_AMBIENT_INTENSITY &&
        b->type == EDITOR_MUTATION_SET_AMBIENT_INTENSITY) return true;
    if ((a->type == EDITOR_MUTATION_SET_LIGHT ||
         a->type == EDITOR_MUTATION_REMOVE_LIGHT) &&
        (b->type == EDITOR_MUTATION_SET_LIGHT ||
         b->type == EDITOR_MUTATION_REMOVE_LIGHT)) {
        SceneInstanceId a_id = a->type == EDITOR_MUTATION_SET_LIGHT
            ? a->data.light.id : a->data.remove_light.id;
        SceneInstanceId b_id = b->type == EDITOR_MUTATION_SET_LIGHT
            ? b->data.light.id : b->data.remove_light.id;
        return a_id == b_id;
    }
    if (a->type == EDITOR_MUTATION_INSERT_LIGHT &&
        b->type == EDITOR_MUTATION_INSERT_LIGHT)
        return true;
    if ((a->type == EDITOR_MUTATION_SET_DECAL ||
         a->type == EDITOR_MUTATION_REMOVE_DECAL) &&
        (b->type == EDITOR_MUTATION_SET_DECAL ||
         b->type == EDITOR_MUTATION_REMOVE_DECAL)) {
        SceneInstanceId a_id = a->type == EDITOR_MUTATION_SET_DECAL
            ? a->data.decal.id : a->data.remove_decal.id;
        SceneInstanceId b_id = b->type == EDITOR_MUTATION_SET_DECAL
            ? b->data.decal.id : b->data.remove_decal.id;
        return a_id == b_id;
    }
    if (a->type == EDITOR_MUTATION_INSERT_DECAL &&
        b->type == EDITOR_MUTATION_INSERT_DECAL) return true;
    if (mutation_is_resize(a->type) && mutation_is_resize(b->type)) return true;
    return false;
}

static bool prepare_surface_mutation(
    SceneDocument *document, const EditorMutationRequest *request,
    EditorMutation *mutation, bool *changed
) {
    int map_x;
    int map_y;
    MaterialId material;
    MaterialId before;
    SceneSurfaceKind surface = mutation_surface(request->type);
    SceneCellOccupancy occupancy;
    if (request->type == EDITOR_MUTATION_SET_WALL_MATERIAL) {
        map_x = request->data.wall_material.wall.map_x;
        map_y = request->data.wall_material.wall.map_y;
        material = request->data.wall_material.material;
    } else {
        map_x = request->data.surface_material.map_x;
        map_y = request->data.surface_material.map_y;
        material = request->data.surface_material.material;
    }
    if (material < 1 || material > ASSET_ID_MAX ||
        !scene_document_get_surface_material(
            document, map_x, map_y, surface, &before)) return false;
    if (surface == SCENE_SURFACE_WALL &&
        (!scene_document_get_cell_occupancy(document, map_x, map_y, &occupancy) ||
         occupancy != SCENE_CELL_OCCUPANCY_WALL)) return false;
    mutation->data.surface_material.map_x = map_x;
    mutation->data.surface_material.map_y = map_y;
    mutation->data.surface_material.before = before;
    mutation->data.surface_material.after = material;
    *changed = before != material;
    return true;
}

static bool prepare_mutation(
    SceneDocument *document, const EditorMutationRequest *request,
    EditorMutation *mutation, bool *changed
) {
    memset(mutation, 0, sizeof(*mutation));
    mutation->type = request->type;
    if (mutation_is_surface(request->type))
        return prepare_surface_mutation(document, request, mutation, changed);
    if (request->type == EDITOR_MUTATION_SET_AMBIENT_INTENSITY) {
        double after = request->data.ambient.intensity;
        if (!isfinite(after) || after < 0.0 || after > 1.0) return false;
        mutation->data.ambient.before = scene_document_get_ambient_intensity(document);
        mutation->data.ambient.after = after;
        *changed = mutation->data.ambient.before != after;
        return true;
    }
    if (mutation_is_occupancy(request->type)) {
        SceneCellOccupancy before;
        SceneCellOccupancy after = request->type == EDITOR_MUTATION_PLACE_WALL
            ? SCENE_CELL_OCCUPANCY_WALL : SCENE_CELL_OCCUPANCY_EMPTY;
        if (!scene_document_get_cell_occupancy(
                document, request->data.occupancy.map_x,
                request->data.occupancy.map_y, &before)) return false;
        mutation->data.occupancy.map_x = request->data.occupancy.map_x;
        mutation->data.occupancy.map_y = request->data.occupancy.map_y;
        mutation->data.occupancy.before = before;
        mutation->data.occupancy.after = after;
        mutation->data.occupancy.removed_decal_start = 0U;
        mutation->data.occupancy.removed_decal_count = 0U;
        *changed = before != after;
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
    if (request->type == EDITOR_MUTATION_INSERT_LIGHT) {
        if (!light_value_insert_is_valid(document, &request->data.insert_light.value))
            return false;
        mutation->data.insert_light.value = request->data.insert_light.value;
        *changed = true;
        return true;
    }
    if (request->type == EDITOR_MUTATION_REMOVE_LIGHT) {
        size_t index;
        if (!find_light_index(document, request->data.remove_light.id, &index))
            return false;
        mutation->data.remove_light.id = request->data.remove_light.id;
        mutation->data.remove_light.index = index;
        mutation->data.remove_light.removed_value = document->lights[index];
        *changed = true;
        return true;
    }
    if (request->type == EDITOR_MUTATION_SET_DECAL) {
        const SceneDecalInstance *before = scene_document_find_decal(
            document, request->data.decal.id);
        if (!before || !scene_document_internal_decal_value_is_valid(
                document, request->data.decal.id, &request->data.decal.value))
            return false;
        mutation->data.decal.id = request->data.decal.id;
        mutation->data.decal.before = *before;
        mutation->data.decal.after = request->data.decal.value;
        *changed = !decals_equal(before, &request->data.decal.value);
        return true;
    }
    if (request->type == EDITOR_MUTATION_INSERT_DECAL) {
        if (document->decal_count >= SCENE_MAX_DECALS ||
            !decal_value_common_is_valid(document, &request->data.insert_decal.value) ||
            scene_document_find_decal(
                document, request->data.insert_decal.value.id) != NULL ||
            scene_document_find_light(
                document, request->data.insert_decal.value.id) != NULL) return false;
        mutation->data.insert_decal.value = request->data.insert_decal.value;
        *changed = true;
        return true;
    }
    if (request->type == EDITOR_MUTATION_REMOVE_DECAL) {
        size_t index;
        if (!find_decal_index(document, request->data.remove_decal.id, &index))
            return false;
        mutation->data.remove_decal.id = request->data.remove_decal.id;
        mutation->data.remove_decal.index = index;
        mutation->data.remove_decal.removed_value = document->decals[index];
        *changed = true;
        return true;
    }
    if (mutation_is_resize(request->type)) {
        mutation->data.resize.trigger = request->data.resize.trigger;
        *changed = true;
        return request->data.resize.trigger >= 0;
    }
    return false;
}

static CommandResult validate_transition(
    const SceneDocument *document, const EditorMutation *mutation, bool after,
    const CommandExecutionContext *context
) {
    if (mutation_is_surface(mutation->type)) {
        MaterialId material = after ? mutation->data.surface_material.after
                                    : mutation->data.surface_material.before;
        if (after && context && context->assets &&
            !material_id_is_loaded(context->assets, material))
            return CMD_RESULT_MATERIAL_NOT_LOADED;
        return CMD_RESULT_OK;
    }
    if (mutation_is_occupancy(mutation->type)) {
        int map_x = mutation->data.occupancy.map_x;
        int map_y = mutation->data.occupancy.map_y;
        SceneCellOccupancy occupancy = after ? mutation->data.occupancy.after
                                             : mutation->data.occupancy.before;
        if (occupancy == SCENE_CELL_OCCUPANCY_WALL) {
            if (map_x == (int)floor(document->spawn_x) &&
                map_y == (int)floor(document->spawn_y))
                return CMD_RESULT_SPAWN_BLOCKED;
            if (context && context->has_player_cell &&
                map_x == context->player_map_x && map_y == context->player_map_y)
                return CMD_RESULT_PLAYER_BLOCKED;
        }
    }
    return CMD_RESULT_OK;
}

static bool apply_mutation(
    SceneDocument *document, const EditorMutation *mutation, bool after,
    const CommandExecutionContext *context,
    const EditorRemovedDecal *removed_decals
) {
    if (mutation_is_surface(mutation->type))
        return scene_document_internal_set_surface_material(
            document, mutation->data.surface_material.map_x,
            mutation->data.surface_material.map_y, mutation_surface(mutation->type),
            after ? mutation->data.surface_material.after
                  : mutation->data.surface_material.before,
            context ? context->assets : NULL);
    if (mutation->type == EDITOR_MUTATION_SET_AMBIENT_INTENSITY)
        return scene_document_internal_set_ambient_intensity(
            document, after ? mutation->data.ambient.after
                            : mutation->data.ambient.before);
    if (mutation_is_occupancy(mutation->type)) {
        SceneCellOccupancy occupancy = after ? mutation->data.occupancy.after
                                             : mutation->data.occupancy.before;
        size_t count = mutation->data.occupancy.removed_decal_count;
        size_t i;
        if (occupancy == SCENE_CELL_OCCUPANCY_EMPTY) {
            for (i = count; i > 0U; i--) {
                const EditorRemovedDecal *removed = &removed_decals[
                    mutation->data.occupancy.removed_decal_start + i - 1U];
                if (!scene_document_internal_remove_decal(
                        document, removed->index, removed->value.id)) return false;
            }
        }
        if (!scene_document_internal_set_cell_occupancy(
                document, mutation->data.occupancy.map_x,
                mutation->data.occupancy.map_y, occupancy)) return false;
        if (occupancy == SCENE_CELL_OCCUPANCY_WALL) {
            for (i = 0U; i < count; i++) {
                const EditorRemovedDecal *removed = &removed_decals[
                    mutation->data.occupancy.removed_decal_start + i];
                if (!scene_document_internal_insert_decal(
                        document, removed->index, &removed->value)) return false;
            }
        }
        return true;
    }
    if (mutation_is_resize(mutation->type)) {
        bool east = mutation->type == EDITOR_MUTATION_GROW_EAST ||
                    mutation->type == EDITOR_MUTATION_SHRINK_EAST;
        bool grow = mutation->type == EDITOR_MUTATION_GROW_EAST ||
                    mutation->type == EDITOR_MUTATION_GROW_SOUTH;
        int trigger = mutation->data.resize.trigger;
        if (!after) grow = !grow;
        return (east
            ? scene_document_internal_resize_east(document, grow, trigger)
            : scene_document_internal_resize_south(document, grow, trigger)) ==
            SCENE_RESIZE_OK;
    }
    if (mutation->type == EDITOR_MUTATION_SET_LIGHT)
        return scene_document_internal_set_light(
            document, mutation->data.light.id,
            after ? &mutation->data.light.after : &mutation->data.light.before);
    if (mutation->type == EDITOR_MUTATION_INSERT_LIGHT) {
        if (after) {
            return scene_document_internal_insert_light(
                document, document->light_count,
                &mutation->data.insert_light.value);
        }
        {
            size_t index;
            if (!find_light_index(document, mutation->data.insert_light.value.id,
                                  &index)) return false;
            return scene_document_internal_remove_light(
                document, index, mutation->data.insert_light.value.id);
        }
    }
    if (mutation->type == EDITOR_MUTATION_REMOVE_LIGHT) {
        if (after) {
            return scene_document_internal_remove_light(
                document, mutation->data.remove_light.index,
                mutation->data.remove_light.id);
        }
        return scene_document_internal_insert_light(
            document, mutation->data.remove_light.index,
            &mutation->data.remove_light.removed_value);
    }
    if (mutation->type == EDITOR_MUTATION_SET_DECAL)
        return scene_document_internal_set_decal(
            document, mutation->data.decal.id,
            after ? &mutation->data.decal.after : &mutation->data.decal.before);
    if (mutation->type == EDITOR_MUTATION_INSERT_DECAL) {
        if (after) return scene_document_internal_insert_decal(
            document, document->decal_count, &mutation->data.insert_decal.value);
        {
            size_t index;
            if (!find_decal_index(document, mutation->data.insert_decal.value.id,
                                  &index)) return false;
            return scene_document_internal_remove_decal(
                document, index, mutation->data.insert_decal.value.id);
        }
    }
    if (mutation->type == EDITOR_MUTATION_REMOVE_DECAL) {
        if (after) return scene_document_internal_remove_decal(
            document, mutation->data.remove_decal.index,
            mutation->data.remove_decal.id);
        return scene_document_internal_insert_decal(
            document, mutation->data.remove_decal.index,
            &mutation->data.remove_decal.removed_value);
    }
    return false;
}

static bool collect_removed_decals(SceneDocument *document, EditorCommand *command) {
    size_t total = 0U;
    size_t mutation_index;
    size_t decal_index;
    for (mutation_index = 0U; mutation_index < command->mutation_count; mutation_index++) {
        EditorMutation *mutation = &command->mutations[mutation_index];
        if (!mutation_is_occupancy(mutation->type) ||
            mutation->data.occupancy.after != SCENE_CELL_OCCUPANCY_EMPTY) continue;
        for (decal_index = 0U; decal_index < document->decal_count; decal_index++) {
            const SceneDecalInstance *decal = &document->decals[decal_index];
            if (wall_decal_supports_cell(
                    decal, mutation->data.occupancy.map_x,
                    mutation->data.occupancy.map_y)) total++;
        }
    }
    if (total == 0U) return true;
    command->removed_decals = g_cmd_realloc(NULL, total * sizeof(*command->removed_decals));
    if (!command->removed_decals) return false;
    command->removed_decal_count = total;
    total = 0U;
    for (mutation_index = 0U; mutation_index < command->mutation_count; mutation_index++) {
        EditorMutation *mutation = &command->mutations[mutation_index];
        if (!mutation_is_occupancy(mutation->type) ||
            mutation->data.occupancy.after != SCENE_CELL_OCCUPANCY_EMPTY) continue;
        mutation->data.occupancy.removed_decal_start = total;
        for (decal_index = 0U; decal_index < document->decal_count; decal_index++) {
            const SceneDecalInstance *decal = &document->decals[decal_index];
            if (wall_decal_supports_cell(
                    decal, mutation->data.occupancy.map_x,
                    mutation->data.occupancy.map_y)) {
                command->removed_decals[total++] =
                    (EditorRemovedDecal){decal_index, *decal};
                mutation->data.occupancy.removed_decal_count++;
            }
        }
    }
    return true;
}

static CommandResult preflight_command(
    const SceneDocument *document, const EditorCommand *command, bool after,
    const CommandExecutionContext *context
) {
    size_t i;
    for (i = 0U; i < command->mutation_count; i++) {
        CommandResult result = validate_transition(
            document, &command->mutations[i], after, context);
        if (result != CMD_RESULT_OK) return result;
    }
    return CMD_RESULT_OK;
}

CommandResult command_history_execute_group_checked(
    CommandHistory *history, SceneDocument *document,
    const EditorMutationRequest *requests, size_t request_count,
    const CommandExecutionContext *context
) {
    EditorCommand command;
    EditorMutation prepared[EDITOR_COMMAND_MAX_MUTATIONS];
    size_t prepared_count = 0U;
    size_t i;
    CommandResult validation;
    if (!history || !document || !requests || request_count == 0U ||
        request_count > EDITOR_COMMAND_MAX_MUTATIONS) return CMD_RESULT_INVALID_TARGET;
    memset(&command, 0, sizeof(command));
    for (i = 0U; i < request_count; i++) {
        EditorMutation mutation;
        bool changed = false;
        size_t j;
        if (!prepare_mutation(document, &requests[i], &mutation, &changed))
            return CMD_RESULT_INVALID_TARGET;
        for (j = 0U; j < prepared_count; j++)
            if (mutations_target_same_field(&prepared[j], &mutation))
                return CMD_RESULT_INVALID_TARGET;
        prepared[prepared_count++] = mutation;
        if (!changed) continue;
        command.mutations[command.mutation_count++] = mutation;
    }
    if (command.mutation_count == 0U) return CMD_RESULT_NO_CHANGE;
    if (!collect_removed_decals(document, &command)) return CMD_RESULT_OUT_OF_MEMORY;
    validation = preflight_command(document, &command, true, context);
    if (validation != CMD_RESULT_OK) { g_cmd_free(command.removed_decals); return validation; }
    if (history->next_state_id == UINT64_MAX) {
        g_cmd_free(command.removed_decals); return CMD_RESULT_STATE_ID_EXHAUSTED;
    }
    if (!history_reserve_one(history)) {
        g_cmd_free(command.removed_decals); return CMD_RESULT_OUT_OF_MEMORY;
    }
    command.before_state = document->current_state;
    command.after_state = history->next_state_id;
    for (i = 0U; i < command.mutation_count; i++) {
        if (!apply_mutation(document, &command.mutations[i], true, context,
                            command.removed_decals)) {
            while (i > 0U) {
                i--;
                (void)apply_mutation(document, &command.mutations[i], false, context,
                                     command.removed_decals);
            }
            g_cmd_free(command.removed_decals);
            return CMD_RESULT_INVALID_TARGET;
        }
    }
    for (i = history->cursor; i < history->count; i++)
        g_cmd_free(history->commands[i].removed_decals);
    history->count = history->cursor;
    history->commands[history->cursor++] = command;
    history->count = history->cursor;
    history->next_state_id = command.after_state + 1U;
    scene_document_internal_set_current_state(document, command.after_state);
    return CMD_RESULT_OK;
}

CommandResult command_history_execute_group(
    CommandHistory *history, SceneDocument *document,
    const EditorMutationRequest *requests, size_t request_count
) {
    return command_history_execute_group_checked(
        history, document, requests, request_count, NULL);
}

CommandResult command_history_set_surface_material(
    CommandHistory *history, SceneDocument *document, int map_x, int map_y,
    SceneSurfaceKind surface, MaterialId material,
    const CommandExecutionContext *context
) {
    EditorMutationRequest request = {0};
    if (surface < SCENE_SURFACE_WALL || surface > SCENE_SURFACE_CEILING)
        return CMD_RESULT_INVALID_TARGET;
    request.type = surface == SCENE_SURFACE_WALL
        ? EDITOR_MUTATION_SET_WALL_MATERIAL
        : (surface == SCENE_SURFACE_FLOOR
            ? EDITOR_MUTATION_SET_FLOOR_MATERIAL
            : EDITOR_MUTATION_SET_CEILING_MATERIAL);
    if (surface == SCENE_SURFACE_WALL) {
        request.data.wall_material.wall = (WallMaterialRef){map_x, map_y};
        request.data.wall_material.material = material;
    } else {
        request.data.surface_material.map_x = map_x;
        request.data.surface_material.map_y = map_y;
        request.data.surface_material.material = material;
    }
    return command_history_execute_group_checked(
        history, document, &request, 1U, context);
}

CommandResult command_history_set_wall_material(
    CommandHistory *history, SceneDocument *document,
    WallMaterialRef ref, MaterialId material
) {
    return command_history_set_surface_material(
        history, document, ref.map_x, ref.map_y,
        SCENE_SURFACE_WALL, material, NULL);
}

CommandResult command_history_set_ambient_intensity(
    CommandHistory *history, SceneDocument *document, double intensity
) {
    EditorMutationRequest request = {0};
    request.type = EDITOR_MUTATION_SET_AMBIENT_INTENSITY;
    request.data.ambient.intensity = intensity;
    return command_history_execute_group(history, document, &request, 1U);
}

CommandResult command_history_place_wall(
    CommandHistory *history, SceneDocument *document, int map_x, int map_y,
    const CommandExecutionContext *context
) {
    EditorMutationRequest requests[2] = {0};
    bool east_match;
    bool south_match;
    SceneCellOccupancy current;
    size_t count = 1U;
    size_t i;
    if (!document) return CMD_RESULT_INVALID_TARGET;
    if (!scene_document_get_cell_occupancy(document, map_x, map_y, &current))
        return CMD_RESULT_INVALID_TARGET;
    if (current == SCENE_CELL_OCCUPANCY_WALL) return CMD_RESULT_NO_CHANGE;
    east_match = document->east_growth_count > 0U &&
        map_x == document->map.width - 2 &&
        map_y == document->east_growth[document->east_growth_count - 1U];
    south_match = document->south_growth_count > 0U &&
        map_y == document->map.height - 2 &&
        map_x == document->south_growth[document->south_growth_count - 1U];
    if (east_match && south_match) return CMD_RESULT_RESIZE_BLOCKED;
    if (east_match || south_match) {
        double boundary = (east_match ? document->map.width : document->map.height) - 1;
        for (i = 0U; i < document->light_count; i++)
            if ((east_match ? document->lights[i].x : document->lights[i].y) >= boundary)
                return CMD_RESULT_RESIZE_BLOCKED;
        for (i = 0U; i < document->decal_count; i++) {
            const SceneDecalInstance *decal = &document->decals[i];
            double coordinate = decal->surface == SCENE_DECAL_SURFACE_WALL
                ? (east_match ? decal->map_x : decal->map_y)
                : (east_match ? decal->x : decal->y);
            if (coordinate >= boundary) return CMD_RESULT_RESIZE_BLOCKED;
        }
        if (!shrink_ring_matches_source(document, east_match,
                east_match ? map_y : map_x)) return CMD_RESULT_RESIZE_BLOCKED;
        count = 2U;
    }
    requests[0].type = EDITOR_MUTATION_PLACE_WALL;
    requests[0].data.occupancy.map_x = map_x;
    requests[0].data.occupancy.map_y = map_y;
    if (count == 2U) {
        requests[1].type = east_match
            ? EDITOR_MUTATION_SHRINK_EAST : EDITOR_MUTATION_SHRINK_SOUTH;
        requests[1].data.resize.trigger = east_match ? map_y : map_x;
    }
    return command_history_execute_group_checked(
        history, document, requests, count, context);
}
CommandResult command_history_remove_wall(
    CommandHistory *history, SceneDocument *document, int map_x, int map_y,
    const CommandExecutionContext *context
) {
    EditorMutationRequest requests[2] = {0};
    SceneCellOccupancy current;
    size_t count = 1U;
    if (!document) return CMD_RESULT_INVALID_TARGET;
    if (!scene_document_get_cell_occupancy(document, map_x, map_y, &current))
        return CMD_RESULT_INVALID_TARGET;
    if (current == SCENE_CELL_OCCUPANCY_EMPTY) return CMD_RESULT_NO_CHANGE;
    if (map_x == document->map.width - 1) {
        if (document->map.width >= SCENE_MAX_WIDTH) return CMD_RESULT_MAP_LIMIT;
        requests[0].type = EDITOR_MUTATION_GROW_EAST;
        requests[0].data.resize.trigger = map_y;
        count = 2U;
    } else if (map_y == document->map.height - 1) {
        if (document->map.height >= SCENE_MAX_HEIGHT) return CMD_RESULT_MAP_LIMIT;
        requests[0].type = EDITOR_MUTATION_GROW_SOUTH;
        requests[0].data.resize.trigger = map_x;
        count = 2U;
    }
    requests[count - 1U].type = EDITOR_MUTATION_REMOVE_WALL;
    requests[count - 1U].data.occupancy.map_x = map_x;
    requests[count - 1U].data.occupancy.map_y = map_y;
    return command_history_execute_group_checked(
        history, document, requests, count, context);
}

CommandResult command_history_set_light(
    CommandHistory *history, SceneDocument *document,
    SceneInstanceId id, const SceneLight *value
) {
    EditorMutationRequest request = {0};
    if (!value) return CMD_RESULT_INVALID_TARGET;
    request.type = EDITOR_MUTATION_SET_LIGHT;
    request.data.light.id = id;
    request.data.light.value = *value;
    return command_history_execute_group(history, document, &request, 1U);
}

CommandResult command_history_insert_light(
    CommandHistory *history, SceneDocument *document,
    const SceneLight *prototype, SceneInstanceId *out_id
) {
    EditorMutationRequest request = {0};
    SceneInstanceId allocated;
    SceneIdAllocateResult alloc_result;
    SceneInstanceId next_before;
    CommandResult result;
    if (!history || !document || !prototype) return CMD_RESULT_INVALID_TARGET;
    if (document->light_count >= SCENE_MAX_LIGHTS ||
        !isfinite(prototype->x) || !isfinite(prototype->y) ||
        !isfinite(prototype->intensity) || !isfinite(prototype->radius) ||
        prototype->x < 0.0 || prototype->y < 0.0 ||
        prototype->x >= document->map.width ||
        prototype->y >= document->map.height || prototype->radius <= 0.0) {
        return CMD_RESULT_INVALID_TARGET;
    }
    next_before = document->next_instance_id;
    alloc_result = scene_document_internal_allocate_instance_id(document, &allocated);
    if (alloc_result == SCENE_ID_ALLOCATE_EXHAUSTED)
        return CMD_RESULT_STATE_ID_EXHAUSTED;
    if (alloc_result != SCENE_ID_ALLOCATE_OK) return CMD_RESULT_INVALID_TARGET;
    request.type = EDITOR_MUTATION_INSERT_LIGHT;
    request.data.insert_light.value = *prototype;
    request.data.insert_light.value.id = allocated;
    result = command_history_execute_group(history, document, &request, 1U);
    if (result != CMD_RESULT_OK) {
        document->next_instance_id = next_before;
        return result;
    }
    if (out_id) *out_id = allocated;
    return CMD_RESULT_OK;
}

CommandResult command_history_remove_light(
    CommandHistory *history, SceneDocument *document,
    SceneInstanceId id
) {
    EditorMutationRequest request = {0};
    request.type = EDITOR_MUTATION_REMOVE_LIGHT;
    request.data.remove_light.id = id;
    return command_history_execute_group(history, document, &request, 1U);
}

CommandResult command_history_set_decal(
    CommandHistory *history, SceneDocument *document,
    SceneInstanceId id, const SceneDecalInstance *value
) {
    EditorMutationRequest request = {0};
    if (!value) return CMD_RESULT_INVALID_TARGET;
    request.type = EDITOR_MUTATION_SET_DECAL;
    request.data.decal.id = id;
    request.data.decal.value = *value;
    return command_history_execute_group(history, document, &request, 1U);
}

CommandResult command_history_insert_decal(
    CommandHistory *history, SceneDocument *document,
    const SceneDecalInstance *prototype, SceneInstanceId *out_id
) {
    EditorMutationRequest request = {0};
    SceneInstanceId allocated;
    SceneInstanceId next_before;
    SceneIdAllocateResult alloc_result;
    CommandResult result;
    if (!history || !document || !prototype ||
        document->decal_count >= SCENE_MAX_DECALS) return CMD_RESULT_INVALID_TARGET;
    next_before = document->next_instance_id;
    alloc_result = scene_document_internal_allocate_instance_id(document, &allocated);
    if (alloc_result == SCENE_ID_ALLOCATE_EXHAUSTED)
        return CMD_RESULT_STATE_ID_EXHAUSTED;
    if (alloc_result != SCENE_ID_ALLOCATE_OK) return CMD_RESULT_INVALID_TARGET;
    request.type = EDITOR_MUTATION_INSERT_DECAL;
    request.data.insert_decal.value = *prototype;
    request.data.insert_decal.value.id = allocated;
    result = command_history_execute_group(history, document, &request, 1U);
    if (result != CMD_RESULT_OK) {
        document->next_instance_id = next_before;
        return result;
    }
    if (out_id) *out_id = allocated;
    return CMD_RESULT_OK;
}

CommandResult command_history_remove_decal(
    CommandHistory *history, SceneDocument *document, SceneInstanceId id
) {
    EditorMutationRequest request = {0};
    request.type = EDITOR_MUTATION_REMOVE_DECAL;
    request.data.remove_decal.id = id;
    return command_history_execute_group(history, document, &request, 1U);
}

CommandResult command_history_undo_checked(
    CommandHistory *history, SceneDocument *document,
    const CommandExecutionContext *context
) {
    EditorCommand *command;
    size_t i;
    CommandResult validation;
    if (!history || !document) return CMD_RESULT_INVALID_TARGET;
    if (history->cursor == 0U) return CMD_RESULT_NOTHING_TO_UNDO;
    command = &history->commands[history->cursor - 1U];
    validation = preflight_command(document, command, false, context);
    if (validation != CMD_RESULT_OK) return validation;
    for (i = command->mutation_count; i > 0U; i--) {
        if (!apply_mutation(document, &command->mutations[i - 1U], false, context,
                            command->removed_decals)) {
            size_t rollback;
            for (rollback = i; rollback < command->mutation_count; rollback++)
                (void)apply_mutation(document, &command->mutations[rollback], true,
                                     context, command->removed_decals);
            return CMD_RESULT_INVALID_TARGET;
        }
    }
    scene_document_internal_set_current_state(document, command->before_state);
    history->cursor--;
    return CMD_RESULT_OK;
}
CommandResult command_history_undo(CommandHistory *history, SceneDocument *document) {
    return command_history_undo_checked(history, document, NULL);
}

CommandResult command_history_redo_checked(
    CommandHistory *history, SceneDocument *document,
    const CommandExecutionContext *context
) {
    EditorCommand *command;
    size_t i;
    CommandResult validation;
    if (!history || !document) return CMD_RESULT_INVALID_TARGET;
    if (history->cursor >= history->count) return CMD_RESULT_NOTHING_TO_REDO;
    command = &history->commands[history->cursor];
    validation = preflight_command(document, command, true, context);
    if (validation != CMD_RESULT_OK) return validation;
    for (i = 0U; i < command->mutation_count; i++) {
        if (!apply_mutation(document, &command->mutations[i], true, context,
                            command->removed_decals)) {
            while (i > 0U) {
                i--;
                (void)apply_mutation(document, &command->mutations[i], false, context,
                                     command->removed_decals);
            }
            return CMD_RESULT_INVALID_TARGET;
        }
    }
    scene_document_internal_set_current_state(document, command->after_state);
    history->cursor++;
    return CMD_RESULT_OK;
}
CommandResult command_history_redo(CommandHistory *history, SceneDocument *document) {
    return command_history_redo_checked(history, document, NULL);
}