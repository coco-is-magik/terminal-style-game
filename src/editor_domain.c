/** editor_domain.c — Pure typed inspector adapters */
#include "editor_domain.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#define EDITOR_LIGHT_POSITION_STEP 0.25
#define EDITOR_LIGHT_POSITION_EDGE 0.01
#define EDITOR_LIGHT_INTENSITY_MIN (-100.0)
#define EDITOR_LIGHT_INTENSITY_MAX 100.0
#define EDITOR_LIGHT_INTENSITY_STEP 0.25
#define EDITOR_LIGHT_RADIUS_MIN 0.25
#define EDITOR_LIGHT_RADIUS_MAX 1024.0
#define EDITOR_LIGHT_RADIUS_STEP 0.25
#define EDITOR_AMBIENT_STEP 0.05
#define EDITOR_DECAL_POSITION_STEP 0.05
#define EDITOR_DECAL_SIZE_STEP 0.05
#define EDITOR_DECAL_ROTATION_STEP 0.125
#define EDITOR_DECAL_DEPTH_STEP 0.01

EditorInspectorKind editor_domain_inspector_kind(SelectionTarget target) {
    if (target.type == SELECTION_WALL_FACE) {
        return EDITOR_INSPECTOR_WALL_MATERIAL;
    }
    if (target.type == SELECTION_LIGHT) return EDITOR_INSPECTOR_LIGHT;
    if (target.type == SELECTION_FLOOR) return EDITOR_INSPECTOR_FLOOR_SURFACE;
    if (target.type == SELECTION_CEILING) return EDITOR_INSPECTOR_CEILING_SURFACE;
    if (target.type == SELECTION_DECAL) return EDITOR_INSPECTOR_DECAL;
    if (target.type == SELECTION_SPRITE) return EDITOR_INSPECTOR_SPRITE;
    if (target.type == SELECTION_TRIGGER) return EDITOR_INSPECTOR_TRIGGER;
    if (target.type == SELECTION_OBJECT) return EDITOR_INSPECTOR_OBJECT;
    return EDITOR_INSPECTOR_NONE;
}

bool editor_domain_inspector_presentation(
    EditorInspectorKind kind,
    EditorInspectorPresentation *out_presentation
) {
    if (!out_presentation) return false;
    if (kind == EDITOR_INSPECTOR_LIGHT) {
        *out_presentation = (EditorInspectorPresentation){
            "light", "Up/Down  Left/Right=edit  Type+Enter=set",
            "RGBA weights colored surface illumination",
            EDITOR_LIGHT_FIELD_COUNT};
        return true;
    }
    if (kind == EDITOR_INSPECTOR_DECAL) {
        *out_presentation = (EditorInspectorPresentation){
            "decal instance", "Up/Down  Left/Right=edit  Type+Enter=set",
            "Reusable pattern; placement transform only",
            EDITOR_DECAL_FIELD_COUNT};
        return true;
    }
    if (kind == EDITOR_INSPECTOR_SPRITE) {
        *out_presentation = (EditorInspectorPresentation){
            "sprite instance", "Up/Down=choose  Left/Right=move  Enter=open",
            "Pattern opens load, save, and paint", EDITOR_SPRITE_FIELD_COUNT};
        return true;
    }
    if (kind == EDITOR_INSPECTOR_TRIGGER) {
        *out_presentation = (EditorInspectorPresentation){
            "trigger", "Up/Down=choose  Left/Right=edit  Enter=set",
            "Enter-region; runtime state is not authored", EDITOR_TRIGGER_FIELD_COUNT};
        return true;
    }
    if (kind == EDITOR_INSPECTOR_OBJECT) {
        *out_presentation = (EditorInspectorPresentation){
            "object", "Up/Down=choose  Left/Right=edit  Enter=open/remove",
            "Direction is stored; billboard visuals cannot confirm it yet",
            EDITOR_OBJECT_FIELD_COUNT};
        return true;
    }
    if (kind == EDITOR_INSPECTOR_FLOOR_SURFACE ||
        kind == EDITOR_INSPECTOR_CEILING_SURFACE) {
        *out_presentation = (EditorInspectorPresentation){
            kind == EDITOR_INSPECTOR_FLOOR_SURFACE ? "floor surface" : "ceiling surface",
            "Up/Down=choose  Enter=open/apply  Esc=back",
            "Heightfield cell; changes are one undo step",
            EDITOR_SURFACE_FIELD_COUNT};
        return true;
    }
    if (kind == EDITOR_INSPECTOR_WALL_MATERIAL) {
        *out_presentation = (EditorInspectorPresentation){
            "wall surface", "Up/Down=choose  Enter=open/apply  Esc=back",
            "One material applies to the entire wall cell",
            EDITOR_SURFACE_FIELD_COUNT};
        return true;
    }
    return false;
}

bool editor_domain_inspector_field_presentation(
    EditorInspectorKind kind,
    size_t field_index,
    const Map *map,
    EditorInspectorFieldPresentation *out_presentation
) {
    EditorLightFieldMetadata light;
    if (!out_presentation) return false;
    if ((kind == EDITOR_INSPECTOR_WALL_MATERIAL ||
         kind == EDITOR_INSPECTOR_FLOOR_SURFACE ||
         kind == EDITOR_INSPECTOR_CEILING_SURFACE) &&
        field_index < EDITOR_SURFACE_FIELD_COUNT) {
        if (field_index == EDITOR_SURFACE_FIELD_MATERIAL) {
            *out_presentation = (EditorInspectorFieldPresentation){
                "Material", EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 0.0, 0.0, 0U};
        } else if (field_index == EDITOR_SURFACE_FIELD_CONSTRUCTION) {
            *out_presentation = (EditorInspectorFieldPresentation){
                kind == EDITOR_INSPECTOR_WALL_MATERIAL ? "Remove Wall" : "Place Wall",
                EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 0.0, 0.0, 0U};
        } else if (field_index == EDITOR_SURFACE_FIELD_AMBIENT) {
            *out_presentation = (EditorInspectorFieldPresentation){
                "Ambient", EDITOR_INSPECTOR_FIELD_NUMBER,
                0.0, 1.0, EDITOR_AMBIENT_STEP, 2U};
        } else if (field_index == EDITOR_SURFACE_FIELD_DECALS) {
            *out_presentation = (EditorInspectorFieldPresentation){
                "Decals", EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 0.0, 0.0, 0U};
        } else if (field_index == EDITOR_SURFACE_FIELD_HEIGHT) {
            *out_presentation = (EditorInspectorFieldPresentation){
                "Height", EDITOR_INSPECTOR_FIELD_NUMBER, -8.0, 8.0, 0.25, 2U};
        } else if (field_index == EDITOR_SURFACE_FIELD_REMOVE) {
            *out_presentation = (EditorInspectorFieldPresentation){
                "Remove/Restore", EDITOR_INSPECTOR_FIELD_CHOICE,
                0.0, 1.0, 1.0, 0U};
        } else if (field_index == EDITOR_SURFACE_FIELD_GRAVITY_DIRECTION) {
            *out_presentation = (EditorInspectorFieldPresentation){
                "Cell gravity dir", EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 6.0, 1.0, 0U};
        } else if (field_index == EDITOR_SURFACE_FIELD_GRAVITY_SCALE) {
            *out_presentation = (EditorInspectorFieldPresentation){
                "Cell gravity scale", EDITOR_INSPECTOR_FIELD_NUMBER,
                0.0, 255.996, 0.25, 2U};
        } else if (field_index == EDITOR_SURFACE_FIELD_OPTICS) {
            *out_presentation = (EditorInspectorFieldPresentation){
                "Optics", EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 0.0, 0.0, 0U};
        } else {
            *out_presentation = (EditorInspectorFieldPresentation){
                "Map movement", EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 0.0, 0.0, 0U};
        }
        return true;
    }
    if (kind == EDITOR_INSPECTOR_SPRITE &&
        field_index < EDITOR_SPRITE_FIELD_COUNT) {
        return editor_domain_sprite_field_presentation(
            (EditorSpriteField)field_index, map, out_presentation);
    }
    if (kind == EDITOR_INSPECTOR_TRIGGER && field_index < EDITOR_TRIGGER_FIELD_COUNT)
        return editor_domain_trigger_field_presentation(
            (EditorTriggerField)field_index, map, out_presentation);
    if (kind != EDITOR_INSPECTOR_LIGHT ||
        field_index >= EDITOR_LIGHT_FIELD_COUNT) return false;
    if (field_index == EDITOR_LIGHT_FIELD_REMOVE) {
        *out_presentation = (EditorInspectorFieldPresentation){
            "Remove", EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 0.0, 0.0, 0U};
        return true;
    }
    if (!editor_domain_light_field_metadata(
            (EditorLightField)field_index, map, &light)) return false;
    *out_presentation = (EditorInspectorFieldPresentation){
        light.label, EDITOR_INSPECTOR_FIELD_NUMBER, light.minimum, light.maximum,
        light.step, light.decimal_places};
    return true;
}

bool editor_domain_decal_field_presentation(
    EditorDecalField field, EditorInspectorFieldPresentation *out
) {
    if (!out) return false;
    switch (field) {
        case EDITOR_DECAL_FIELD_POSITION_U:
            *out = (EditorInspectorFieldPresentation){"Position U", EDITOR_INSPECTOR_FIELD_NUMBER, 0.0, SCENE_MAX_WIDTH - 0.01, EDITOR_DECAL_POSITION_STEP, 2U}; break;
        case EDITOR_DECAL_FIELD_POSITION_V:
            *out = (EditorInspectorFieldPresentation){"Position V", EDITOR_INSPECTOR_FIELD_NUMBER, 0.0, SCENE_MAX_HEIGHT - 0.01, EDITOR_DECAL_POSITION_STEP, 2U}; break;
        case EDITOR_DECAL_FIELD_WIDTH:
            *out = (EditorInspectorFieldPresentation){"Width", EDITOR_INSPECTOR_FIELD_NUMBER, 0.05, 64.0, EDITOR_DECAL_SIZE_STEP, 2U}; break;
        case EDITOR_DECAL_FIELD_HEIGHT:
            *out = (EditorInspectorFieldPresentation){"Height", EDITOR_INSPECTOR_FIELD_NUMBER, 0.05, 64.0, EDITOR_DECAL_SIZE_STEP, 2U}; break;
        case EDITOR_DECAL_FIELD_ROTATION:
            *out = (EditorInspectorFieldPresentation){"Rotation", EDITOR_INSPECTOR_FIELD_NUMBER, -1000.0, 1000.0, EDITOR_DECAL_ROTATION_STEP, 3U}; break;
        case EDITOR_DECAL_FIELD_DEPTH:
            *out = (EditorInspectorFieldPresentation){"Depth", EDITOR_INSPECTOR_FIELD_NUMBER, 0.0, 64.0, EDITOR_DECAL_DEPTH_STEP, 2U}; break;
        case EDITOR_DECAL_FIELD_REMOVE:
            *out = (EditorInspectorFieldPresentation){"Remove", EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 0.0, 0.0, 0U}; break;
        default: return false;
    }
    return true;
}

static double decal_field_value(const SceneDecalInstance *decal, EditorDecalField field) {
    if (field == EDITOR_DECAL_FIELD_POSITION_U)
        return decal->surface == SCENE_DECAL_SURFACE_WALL ? decal->u : decal->x;
    if (field == EDITOR_DECAL_FIELD_POSITION_V)
        return decal->surface == SCENE_DECAL_SURFACE_WALL ? decal->v : decal->y;
    if (field == EDITOR_DECAL_FIELD_WIDTH) return decal->width;
    if (field == EDITOR_DECAL_FIELD_HEIGHT) return decal->height;
    if (field == EDITOR_DECAL_FIELD_ROTATION) return decal->rotation;
    return decal->depth;
}

bool editor_domain_format_decal_field(
    const SceneDecalInstance *decal, EditorDecalField field,
    char *out_text, size_t out_size
) {
    EditorInspectorFieldPresentation presentation;
    int written;
    if (!decal || !out_text || out_size == 0U ||
        !editor_domain_decal_field_presentation(field, &presentation)) return false;
    if (field == EDITOR_DECAL_FIELD_REMOVE)
        written = snprintf(out_text, out_size, "Enter=remove");
    else if (presentation.decimal_places == 3U)
        written = snprintf(out_text, out_size, "%.3f", decal_field_value(decal, field));
    else written = snprintf(out_text, out_size, "%.2f", decal_field_value(decal, field));
    return written >= 0 && (size_t)written < out_size;
}

static bool make_decal_value_request(
    const SceneDocument *document, SelectionTarget target, EditorDecalField field,
    double numeric_value, EditorMutationRequest *out_request
) {
    const SceneDecalInstance *current;
    EditorInspectorFieldPresentation presentation;
    SceneDecalInstance value;
    if (!document || !out_request || target.type != SELECTION_DECAL ||
        !isfinite(numeric_value) ||
        !editor_domain_decal_field_presentation(field, &presentation) ||
        presentation.kind != EDITOR_INSPECTOR_FIELD_NUMBER ||
        numeric_value < presentation.minimum || numeric_value > presentation.maximum)
        return false;
    current = scene_document_find_decal(document, target.value.decal.id);
    if (!current) return false;
    if (current->surface == SCENE_DECAL_SURFACE_WALL &&
        (field == EDITOR_DECAL_FIELD_POSITION_U ||
         field == EDITOR_DECAL_FIELD_POSITION_V) && numeric_value > 1.0) return false;
    if (current->surface != SCENE_DECAL_SURFACE_WALL &&
        ((field == EDITOR_DECAL_FIELD_POSITION_U &&
          numeric_value >= document->map.width) ||
         (field == EDITOR_DECAL_FIELD_POSITION_V &&
          numeric_value >= document->map.height))) return false;
    value = *current;
    if (field == EDITOR_DECAL_FIELD_POSITION_U) {
        if (value.surface == SCENE_DECAL_SURFACE_WALL) value.u = numeric_value;
        else value.x = numeric_value;
    } else if (field == EDITOR_DECAL_FIELD_POSITION_V) {
        if (value.surface == SCENE_DECAL_SURFACE_WALL) value.v = numeric_value;
        else value.y = numeric_value;
    } else if (field == EDITOR_DECAL_FIELD_WIDTH) value.width = numeric_value;
    else if (field == EDITOR_DECAL_FIELD_HEIGHT) value.height = numeric_value;
    else if (field == EDITOR_DECAL_FIELD_ROTATION) value.rotation = numeric_value;
    else if (field == EDITOR_DECAL_FIELD_DEPTH) value.depth = numeric_value;
    else return false;
    memset(out_request, 0, sizeof(*out_request));
    out_request->type = EDITOR_MUTATION_SET_DECAL;
    out_request->data.decal.id = target.value.decal.id;
    out_request->data.decal.value = value;
    return true;
}

bool editor_domain_make_decal_step_request(
    const SceneDocument *document, SelectionTarget target, EditorDecalField field,
    int direction, EditorMutationRequest *out_request
) {
    const SceneDecalInstance *current;
    EditorInspectorFieldPresentation presentation;
    double value;
    if (direction == 0 || !document || target.type != SELECTION_DECAL ||
        !editor_domain_decal_field_presentation(field, &presentation) ||
        presentation.kind != EDITOR_INSPECTOR_FIELD_NUMBER) return false;
    current = scene_document_find_decal(document, target.value.decal.id);
    if (!current) return false;
    value = decal_field_value(current, field) + (direction < 0 ? -presentation.step : presentation.step);
    if (current->surface == SCENE_DECAL_SURFACE_WALL &&
        (field == EDITOR_DECAL_FIELD_POSITION_U ||
         field == EDITOR_DECAL_FIELD_POSITION_V) && value > 1.0) value = 1.0;
    if (current->surface != SCENE_DECAL_SURFACE_WALL) {
        if (field == EDITOR_DECAL_FIELD_POSITION_U && value >= document->map.width)
            value = document->map.width - 0.01;
        if (field == EDITOR_DECAL_FIELD_POSITION_V && value >= document->map.height)
            value = document->map.height - 0.01;
    }
    if (value < presentation.minimum) value = presentation.minimum;
    if (value > presentation.maximum) value = presentation.maximum;
    return make_decal_value_request(document, target, field, value, out_request);
}

bool editor_domain_make_decal_value_request(
    const SceneDocument *document, SelectionTarget target, EditorDecalField field,
    double numeric_value, EditorMutationRequest *out_request
) {
    return make_decal_value_request(
        document, target, field, numeric_value, out_request);
}

bool editor_domain_sprite_field_presentation(
    EditorSpriteField field, const Map *map,
    EditorInspectorFieldPresentation *out
) {
    if (!map || !out || map->width <= 0 || map->height <= 0) return false;
    if (field == EDITOR_SPRITE_FIELD_X)
        *out = (EditorInspectorFieldPresentation){
            "X", EDITOR_INSPECTOR_FIELD_NUMBER, 0.0, map->width - 0.01,
            EDITOR_LIGHT_POSITION_STEP, 2U};
    else if (field == EDITOR_SPRITE_FIELD_Y)
        *out = (EditorInspectorFieldPresentation){
            "Y", EDITOR_INSPECTOR_FIELD_NUMBER, 0.0, map->height - 0.01,
            EDITOR_LIGHT_POSITION_STEP, 2U};
    else if (field == EDITOR_SPRITE_FIELD_PATTERN)
        *out = (EditorInspectorFieldPresentation){
            "Pattern...", EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 0.0, 0.0, 0U};
    else if (field == EDITOR_SPRITE_FIELD_REMOVE)
        *out = (EditorInspectorFieldPresentation){
            "Remove", EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 0.0, 0.0, 0U};
    else return false;
    return true;
}

bool editor_domain_format_sprite_field(
    const SceneSpriteInstance *sprite, EditorSpriteField field,
    char *out_text, size_t out_size
) {
    int written;
    if (!sprite || !out_text || out_size == 0U) return false;
    if (field == EDITOR_SPRITE_FIELD_PATTERN)
        written = snprintf(out_text, out_size, "Enter=open");
    else if (field == EDITOR_SPRITE_FIELD_REMOVE)
        written = snprintf(out_text, out_size, "Enter=remove");
    else if (field == EDITOR_SPRITE_FIELD_X)
        written = snprintf(out_text, out_size, "%.2f", sprite->x);
    else if (field == EDITOR_SPRITE_FIELD_Y)
        written = snprintf(out_text, out_size, "%.2f", sprite->y);
    else return false;
    return written >= 0 && (size_t)written < out_size;
}

bool editor_domain_make_sprite_value_request(
    const SceneDocument *document, SelectionTarget target,
    EditorSpriteField field, double numeric_value,
    EditorMutationRequest *out_request
) {
    const SceneSpriteInstance *current;
    EditorInspectorFieldPresentation presentation;
    SceneSpriteInstance value;
    if (!document || !out_request || target.type != SELECTION_SPRITE ||
        !isfinite(numeric_value) || !editor_domain_sprite_field_presentation(
            field, &document->map, &presentation) ||
        presentation.kind != EDITOR_INSPECTOR_FIELD_NUMBER ||
        numeric_value < presentation.minimum || numeric_value > presentation.maximum)
        return false;
    current = scene_document_find_sprite(document, target.value.sprite.id);
    if (!current) return false;
    value = *current;
    if (field == EDITOR_SPRITE_FIELD_X) value.x = numeric_value;
    else if (field == EDITOR_SPRITE_FIELD_Y) value.y = numeric_value;
    else return false;
    memset(out_request, 0, sizeof(*out_request));
    out_request->type = EDITOR_MUTATION_SET_SPRITE;
    out_request->data.sprite.id = target.value.sprite.id;
    out_request->data.sprite.value = value;
    return true;
}

bool editor_domain_make_sprite_step_request(
    const SceneDocument *document, SelectionTarget target,
    EditorSpriteField field, int direction, EditorMutationRequest *out_request
) {
    const SceneSpriteInstance *current;
    EditorInspectorFieldPresentation presentation;
    double value;
    if (!document || direction == 0 || target.type != SELECTION_SPRITE ||
        !editor_domain_sprite_field_presentation(
            field, &document->map, &presentation) ||
        presentation.kind != EDITOR_INSPECTOR_FIELD_NUMBER) return false;
    current = scene_document_find_sprite(document, target.value.sprite.id);
    if (!current) return false;
    value = (field == EDITOR_SPRITE_FIELD_X ? current->x : current->y) +
        (direction < 0 ? -presentation.step : presentation.step);
    if (value < presentation.minimum) value = presentation.minimum;
    if (value > presentation.maximum) value = presentation.maximum;
    return editor_domain_make_sprite_value_request(
        document, target, field, value, out_request);
}

bool editor_domain_trigger_field_presentation(
    EditorTriggerField field, const Map *map, EditorInspectorFieldPresentation *out
) {
    static const char *labels[] = {
        "Min X", "Min Y", "Max X", "Max Y", "Condition", "Action", "Value", "Remove"
    };
    if (!map || !out || field < 0 || field >= EDITOR_TRIGGER_FIELD_COUNT) return false;
    *out = (EditorInspectorFieldPresentation){labels[field],
        field < EDITOR_TRIGGER_FIELD_CONDITION || field == EDITOR_TRIGGER_FIELD_PAYLOAD
            ? EDITOR_INSPECTOR_FIELD_NUMBER : EDITOR_INSPECTOR_FIELD_CHOICE,
        0.0, (field == EDITOR_TRIGGER_FIELD_MIN_X || field == EDITOR_TRIGGER_FIELD_MAX_X)
            ? map->width : (field == EDITOR_TRIGGER_FIELD_MIN_Y ||
                            field == EDITOR_TRIGGER_FIELD_MAX_Y) ? map->height : 64.0,
        field == EDITOR_TRIGGER_FIELD_PAYLOAD ? 1.0 : 0.25,
        field == EDITOR_TRIGGER_FIELD_PAYLOAD ? 0U : 2U};
    return true;
}

bool editor_domain_format_trigger_field(const SceneTrigger *t, EditorTriggerField field,
                                        char *out, size_t size) {
    int n;
    if (!t || !out || !size) return false;
    if (field == EDITOR_TRIGGER_FIELD_MIN_X) n = snprintf(out, size, "%.2f", t->min_x);
    else if (field == EDITOR_TRIGGER_FIELD_MIN_Y) n = snprintf(out, size, "%.2f", t->min_y);
    else if (field == EDITOR_TRIGGER_FIELD_MAX_X) n = snprintf(out, size, "%.2f", t->max_x);
    else if (field == EDITOR_TRIGGER_FIELD_MAX_Y) n = snprintf(out, size, "%.2f", t->max_y);
    else if (field == EDITOR_TRIGGER_FIELD_CONDITION) n = snprintf(out, size, "enter region");
    else if (field == EDITOR_TRIGGER_FIELD_ACTION) n = snprintf(out, size, "%s",
        t->action == SCENE_TRIGGER_ACTION_SET_FLAG ? "set flag" :
        t->action == SCENE_TRIGGER_ACTION_TELEPORT_TO_SPAWN ? "teleport to spawn" : "toggle light");
    else if (field == EDITOR_TRIGGER_FIELD_PAYLOAD) {
        if (t->action == SCENE_TRIGGER_ACTION_SET_FLAG)
            n = snprintf(out, size, "flag %u = %s", t->flag_id,
                         t->flag_value ? "true" : "false");
        else if (t->action == SCENE_TRIGGER_ACTION_TOGGLE_LIGHT)
            n = snprintf(out, size, "light %llu",
                         (unsigned long long)t->target_id);
        else n = snprintf(out, size, "none");
    }
    else n = snprintf(out, size, "Enter=remove");
    return n >= 0 && (size_t)n < size;
}

bool editor_domain_make_trigger_step_request(
    const SceneDocument *document, SelectionTarget target, EditorTriggerField field,
    int direction, EditorMutationRequest *out
) {
    const SceneTrigger *current;
    SceneTrigger value;
    double step = direction < 0 ? -0.25 : 0.25;
    if (!document || !out || !direction || target.type != SELECTION_TRIGGER) return false;
    current = scene_document_find_trigger(document, target.value.trigger.id);
    if (!current) return false;
    value = *current;
    if (field == EDITOR_TRIGGER_FIELD_MIN_X) value.min_x += step;
    else if (field == EDITOR_TRIGGER_FIELD_MIN_Y) value.min_y += step;
    else if (field == EDITOR_TRIGGER_FIELD_MAX_X) value.max_x += step;
    else if (field == EDITOR_TRIGGER_FIELD_MAX_Y) value.max_y += step;
    else if (field == EDITOR_TRIGGER_FIELD_ACTION) {
        int action = (int)value.action + (direction < 0 ? -1 : 1);
        if (action < 0) action = SCENE_TRIGGER_ACTION_TOGGLE_LIGHT;
        if (action > SCENE_TRIGGER_ACTION_TOGGLE_LIGHT) action = 0;
        value.action = (SceneTriggerActionType)action;
        value.flag_id = value.action == SCENE_TRIGGER_ACTION_SET_FLAG ? 1U : 0U;
        value.flag_value = value.action == SCENE_TRIGGER_ACTION_SET_FLAG;
        value.target_id = value.action == SCENE_TRIGGER_ACTION_TOGGLE_LIGHT &&
            document->light_count ? document->lights[0].id : 0U;
    } else if (field == EDITOR_TRIGGER_FIELD_PAYLOAD) {
        if (value.action == SCENE_TRIGGER_ACTION_SET_FLAG) {
            int id = (int)value.flag_id + (direction < 0 ? -1 : 1);
            if (id < 1) id = 1;
            if (id > 64) id = 64;
            value.flag_id = (uint8_t)id;
        } else if (value.action == SCENE_TRIGGER_ACTION_TOGGLE_LIGHT && document->light_count) {
            size_t index;
            for (index = 0U; index < document->light_count; index++)
                if (document->lights[index].id == value.target_id) break;
            if (index == document->light_count) index = 0U;
            else if (direction < 0)
                index = index == 0U ? document->light_count - 1U : index - 1U;
            else index = (index + 1U) % document->light_count;
            value.target_id = document->lights[index].id;
        }
        else return false;
    } else return false;
    if (value.min_x < 0.0 || value.min_y < 0.0 ||
        value.max_x > document->map.width || value.max_y > document->map.height ||
        value.min_x >= value.max_x || value.min_y >= value.max_y ||
        (value.action == SCENE_TRIGGER_ACTION_TOGGLE_LIGHT && value.target_id == 0U))
        return false;
    memset(out, 0, sizeof(*out)); out->type = EDITOR_MUTATION_SET_TRIGGER;
    out->data.trigger.id = value.id; out->data.trigger.value = value;
    return true;
}

bool editor_domain_make_trigger_confirm_request(
    const SceneDocument *document, SelectionTarget target,
    EditorTriggerField field, EditorMutationRequest *out
) {
    const SceneTrigger *current;
    SceneTrigger value;
    if (!document || !out || target.type != SELECTION_TRIGGER ||
        field != EDITOR_TRIGGER_FIELD_PAYLOAD) return false;
    current = scene_document_find_trigger(document, target.value.trigger.id);
    if (!current || current->action != SCENE_TRIGGER_ACTION_SET_FLAG) return false;
    value = *current;
    value.flag_value = !value.flag_value;
    memset(out, 0, sizeof(*out));
    out->type = EDITOR_MUTATION_SET_TRIGGER;
    out->data.trigger.id = value.id;
    out->data.trigger.value = value;
    return true;
}

bool editor_domain_make_surface_material_request(
    SelectionTarget target, MaterialId material,
    EditorMutationRequest *out_request
) {
    if (!out_request || material < 1 || material > ASSET_ID_MAX) return false;
    memset(out_request, 0, sizeof(*out_request));
    if (target.type == SELECTION_WALL_FACE) {
        out_request->type = EDITOR_MUTATION_SET_WALL_MATERIAL;
        out_request->data.wall_material.wall.map_x = target.value.wall_face.map_x;
        out_request->data.wall_material.wall.map_y = target.value.wall_face.map_y;
        out_request->data.wall_material.material = material;
    } else if (target.type == SELECTION_FLOOR || target.type == SELECTION_CEILING) {
        out_request->type = target.type == SELECTION_FLOOR
            ? EDITOR_MUTATION_SET_FLOOR_MATERIAL
            : EDITOR_MUTATION_SET_CEILING_MATERIAL;
        out_request->data.surface_material.map_x = target.value.horizontal.map_x;
        out_request->data.surface_material.map_y = target.value.horizontal.map_y;
        out_request->data.surface_material.material = material;
    } else return false;
    return true;
}

bool editor_domain_make_construction_request(
    SelectionTarget target, EditorMutationRequest *out_request
) {
    if (!out_request) return false;
    memset(out_request, 0, sizeof(*out_request));
    if (target.type == SELECTION_WALL_FACE) {
        out_request->type = EDITOR_MUTATION_REMOVE_WALL;
        out_request->data.occupancy.map_x = target.value.wall_face.map_x;
        out_request->data.occupancy.map_y = target.value.wall_face.map_y;
    } else if (target.type == SELECTION_FLOOR || target.type == SELECTION_CEILING) {
        out_request->type = EDITOR_MUTATION_PLACE_WALL;
        out_request->data.occupancy.map_x = target.value.horizontal.map_x;
        out_request->data.occupancy.map_y = target.value.horizontal.map_y;
    } else return false;
    return true;
}

bool editor_domain_ambient_metadata(EditorAmbientMetadata *out_metadata) {
    if (!out_metadata) return false;
    *out_metadata = (EditorAmbientMetadata){0.0, 1.0, EDITOR_AMBIENT_STEP, 2U};
    return true;
}

bool editor_domain_format_ambient(double ambient, char *out_text, size_t out_size) {
    int written;
    if (!out_text || out_size == 0U || !isfinite(ambient) ||
        ambient < 0.0 || ambient > 1.0) return false;
    written = snprintf(out_text, out_size, "%.2f", ambient);
    return written >= 0 && (size_t)written < out_size;
}

bool editor_domain_make_ambient_value_request(
    double value, EditorMutationRequest *out_request
) {
    if (!out_request || !isfinite(value) || value < 0.0 || value > 1.0) return false;
    memset(out_request, 0, sizeof(*out_request));
    out_request->type = EDITOR_MUTATION_SET_AMBIENT_INTENSITY;
    out_request->data.ambient.intensity = value;
    return true;
}

bool editor_domain_make_ambient_step_request(
    const SceneDocument *document, int direction,
    EditorMutationRequest *out_request
) {
    double value;
    if (!document || (direction != -1 && direction != 1)) return false;
    value = scene_document_get_ambient_intensity(document) +
        direction * EDITOR_AMBIENT_STEP;
    if (value < 0.0) value = 0.0;
    if (value > 1.0) value = 1.0;
    return editor_domain_make_ambient_value_request(value, out_request);
}

static bool selected_horizontal_cell(
    SelectionTarget target, int *out_x, int *out_y
) {
    if (!out_x || !out_y ||
        (target.type != SELECTION_FLOOR && target.type != SELECTION_CEILING))
        return false;
    *out_x = target.value.horizontal.map_x;
    *out_y = target.value.horizontal.map_y;
    return true;
}

static bool begin_cell_vertical_request(
    const SceneDocument *document, SelectionTarget target,
    SceneCellVertical *out_value, EditorMutationRequest *out_request
) {
    int map_x;
    int map_y;
    if (!document || !out_value || !out_request ||
        !selected_horizontal_cell(target, &map_x, &map_y) ||
        !scene_document_get_cell_vertical(
            document, map_x, map_y, out_value)) return false;
    memset(out_request, 0, sizeof(*out_request));
    out_request->type = EDITOR_MUTATION_SET_CELL_VERTICAL;
    out_request->data.cell_vertical.map_x = map_x;
    out_request->data.cell_vertical.map_y = map_y;
    return true;
}

bool editor_domain_make_height_step_request(
    const SceneDocument *document, SelectionTarget target, int direction,
    EditorMutationRequest *out_request
) {
    SceneCellVertical value;
    int height;
    if ((direction != -1 && direction != 1) ||
        !begin_cell_vertical_request(document, target, &value, out_request))
        return false;
    height = target.type == SELECTION_FLOOR
        ? (int)value.floor_height_step : (int)value.ceiling_height_step;
    height += direction * 64;
    if (height < (int)SCENE_HEIGHT_MIN_STEP ||
        height > (int)SCENE_HEIGHT_MAX_STEP) return false;
    if (target.type == SELECTION_FLOOR) {
        value.floor_height_step = (int16_t)height;
        value.floor_present = true;
    } else {
        value.ceiling_height_step = (int16_t)height;
        value.ceiling_present = true;
    }
    out_request->data.cell_vertical.value = value;
    return true;
}

bool editor_domain_make_surface_presence_request(
    const SceneDocument *document, SelectionTarget target,
    EditorMutationRequest *out_request
) {
    SceneCellVertical value;
    if (!begin_cell_vertical_request(document, target, &value, out_request))
        return false;
    if (target.type == SELECTION_FLOOR)
        value.floor_present = !value.floor_present;
    else
        value.ceiling_present = !value.ceiling_present;
    out_request->data.cell_vertical.value = value;
    return true;
}

bool editor_domain_make_gravity_direction_step_request(
    const SceneDocument *document, SelectionTarget target, int direction,
    EditorMutationRequest *out_request
) {
    SceneCellVertical value;
    int orientation;
    if ((direction != -1 && direction != 1) ||
        !begin_cell_vertical_request(document, target, &value, out_request))
        return false;
    orientation = value.gravity_orientation + direction;
    if (orientation < SCENE_GRAVITY_INHERIT) orientation = SCENE_GRAVITY_WEST;
    if (orientation > SCENE_GRAVITY_WEST) orientation = SCENE_GRAVITY_INHERIT;
    value.gravity_orientation = (uint8_t)orientation;
    out_request->data.cell_vertical.value = value;
    return true;
}

bool editor_domain_make_gravity_scale_step_request(
    const SceneDocument *document, SelectionTarget target, int direction,
    EditorMutationRequest *out_request
) {
    SceneCellVertical value;
    int scale;
    if ((direction != -1 && direction != 1) ||
        !begin_cell_vertical_request(document, target, &value, out_request))
        return false;
    scale = (int)value.gravity_scale_step + direction * 64;
    if (scale < 0 || scale > (int)UINT16_MAX) return false;
    value.gravity_scale_step = (uint16_t)scale;
    out_request->data.cell_vertical.value = value;
    return true;
}

bool editor_domain_movement_field_presentation(
    EditorMovementField field, EditorInspectorFieldPresentation *out
) {
    if (!out) return false;
    switch (field) {
        case EDITOR_MOVEMENT_FIELD_GRAVITY_MAGNITUDE:
            *out = (EditorInspectorFieldPresentation){"Map gravity", EDITOR_INSPECTOR_FIELD_NUMBER, 0.1, 256.0, 0.5, 2U}; break;
        case EDITOR_MOVEMENT_FIELD_GRAVITY_ORIENTATION:
            *out = (EditorInspectorFieldPresentation){"Map gravity dir", EDITOR_INSPECTOR_FIELD_CHOICE, 1.0, 6.0, 1.0, 0U}; break;
        case EDITOR_MOVEMENT_FIELD_STEP_HEIGHT:
            *out = (EditorInspectorFieldPresentation){"Step height", EDITOR_INSPECTOR_FIELD_NUMBER, 0.0625, 1.0, 0.0625, 4U}; break;
        case EDITOR_MOVEMENT_FIELD_JUMP_IMPULSE:
            *out = (EditorInspectorFieldPresentation){"Jump impulse", EDITOR_INSPECTOR_FIELD_NUMBER, 0.1, 16.0, 0.25, 2U}; break;
        case EDITOR_MOVEMENT_FIELD_AIR_CONTROL:
            *out = (EditorInspectorFieldPresentation){"Air control", EDITOR_INSPECTOR_FIELD_NUMBER, 0.0, 1.0, 0.05, 2U}; break;
        case EDITOR_MOVEMENT_FIELD_EYE_HEIGHT:
            *out = (EditorInspectorFieldPresentation){"Eye height", EDITOR_INSPECTOR_FIELD_NUMBER, 0.0625, 8.0, 0.0625, 4U}; break;
        case EDITOR_MOVEMENT_FIELD_HEAD_CLEARANCE:
            *out = (EditorInspectorFieldPresentation){"Head clearance", EDITOR_INSPECTOR_FIELD_NUMBER, 0.25, 8.0, 0.0625, 4U}; break;
        default: return false;
    }
    return true;
}

bool editor_domain_make_movement_step_request(
    const SceneDocument *document, EditorMovementField field, int direction,
    EditorMutationRequest *out_request
) {
    EditorInspectorFieldPresentation metadata;
    SceneMovementParameters value;
    double *number = NULL;
    if (!document || !out_request || (direction != -1 && direction != 1) ||
        !editor_domain_movement_field_presentation(field, &metadata)) return false;
    value = document->movement;
    if (field == EDITOR_MOVEMENT_FIELD_GRAVITY_ORIENTATION) {
        int orientation = value.gravity_orientation + direction;
        if (orientation < SCENE_GRAVITY_DOWN) orientation = SCENE_GRAVITY_WEST;
        if (orientation > SCENE_GRAVITY_WEST) orientation = SCENE_GRAVITY_DOWN;
        value.gravity_orientation = (SceneGravityOrientation)orientation;
    } else {
        switch (field) {
            case EDITOR_MOVEMENT_FIELD_GRAVITY_MAGNITUDE: number = &value.gravity_magnitude; break;
            case EDITOR_MOVEMENT_FIELD_STEP_HEIGHT: number = &value.step_height; break;
            case EDITOR_MOVEMENT_FIELD_JUMP_IMPULSE: number = &value.jump_impulse; break;
            case EDITOR_MOVEMENT_FIELD_AIR_CONTROL: number = &value.air_control_scale; break;
            case EDITOR_MOVEMENT_FIELD_EYE_HEIGHT: number = &value.eye_height; break;
            case EDITOR_MOVEMENT_FIELD_HEAD_CLEARANCE: number = &value.head_clearance; break;
            default: return false;
        }
        *number += direction * metadata.step;
        if (*number < metadata.minimum) *number = metadata.minimum;
        if (*number > metadata.maximum) *number = metadata.maximum;
    }
    memset(out_request, 0, sizeof(*out_request));
    out_request->type = EDITOR_MUTATION_SET_MOVEMENT_PARAMETERS;
    out_request->data.movement.value = value;
    return true;
}

static bool optical_field_parts(
    OpticalExtension *extension, EditorOpticalField field,
    uint8_t *out_bit, uint8_t **out_value, bool *out_boolean
) {
    uint8_t bit;
    uint8_t *value;
    bool boolean = false;
    if (!extension || !out_bit || !out_value || !out_boolean) return false;
    switch (field) {
        case EDITOR_OPTICAL_FIELD_PLAYER_BLOCKS:
            bit = OPTICAL_OVERRIDE_PLAYER_BLOCKS;
            value = &extension->player_blocks;
            boolean = true;
            break;
        case EDITOR_OPTICAL_FIELD_RAY_BLOCKS:
            bit = OPTICAL_OVERRIDE_RAY_BLOCKS;
            value = &extension->ray_blocks;
            boolean = true;
            break;
        case EDITOR_OPTICAL_FIELD_LIGHT_BLOCKS:
            bit = OPTICAL_OVERRIDE_LIGHT_BLOCKS;
            value = &extension->light_blocks;
            boolean = true;
            break;
        case EDITOR_OPTICAL_FIELD_OPACITY:
            bit = OPTICAL_OVERRIDE_OPACITY;
            value = &extension->opacity;
            break;
        case EDITOR_OPTICAL_FIELD_TRANSMISSION:
            bit = OPTICAL_OVERRIDE_TRANSMISSION;
            value = &extension->transmission;
            break;
        case EDITOR_OPTICAL_FIELD_REFLECTIVITY:
            bit = OPTICAL_OVERRIDE_REFLECTIVITY;
            value = &extension->reflectivity;
            break;
        default: return false;
    }
    *out_bit = bit;
    *out_value = value;
    *out_boolean = boolean;
    return true;
}

static bool optical_target(
    const SceneDocument *document, SelectionTarget target,
    int *out_x, int *out_y, SceneSurfaceKind *out_surface
) {
    if (!document || !out_x || !out_y || !out_surface) return false;
    if (target.type == SELECTION_WALL_FACE) {
        *out_x = target.value.wall_face.map_x;
        *out_y = target.value.wall_face.map_y;
        *out_surface = SCENE_SURFACE_WALL;
    } else if (target.type == SELECTION_FLOOR || target.type == SELECTION_CEILING) {
        *out_x = target.value.horizontal.map_x;
        *out_y = target.value.horizontal.map_y;
        *out_surface = target.type == SELECTION_FLOOR
            ? SCENE_SURFACE_FLOOR : SCENE_SURFACE_CEILING;
    } else return false;
    return map_in_bounds(&document->map, *out_x, *out_y);
}

bool editor_domain_optical_field_presentation(
    EditorOpticalField field, EditorInspectorFieldPresentation *out
) {
    static const char *const labels[] = {
        "Player blocks", "Ray blocks", "Light blocks",
        "Opacity", "Transmission", "Reflectivity"
    };
    if (!out || field < EDITOR_OPTICAL_FIELD_PLAYER_BLOCKS ||
        field >= EDITOR_OPTICAL_FIELD_COUNT) return false;
    *out = (EditorInspectorFieldPresentation){
        labels[field], field <= EDITOR_OPTICAL_FIELD_LIGHT_BLOCKS
            ? EDITOR_INSPECTOR_FIELD_CHOICE : EDITOR_INSPECTOR_FIELD_NUMBER,
        0.0, field <= EDITOR_OPTICAL_FIELD_LIGHT_BLOCKS ? 1.0 : 255.0,
        field <= EDITOR_OPTICAL_FIELD_LIGHT_BLOCKS ? 1.0 : 16.0, 0U};
    return true;
}

bool editor_domain_get_optical_extension(
    const SceneDocument *document, SelectionTarget target,
    EditorOpticalScope scope, OpticalExtension *out_extension,
    uint16_t *out_material_id, size_t *out_cell_index
) {
    int x;
    int y;
    SceneSurfaceKind surface;
    MaterialId material;
    size_t cell_index;
    if (!out_extension || !optical_target(document, target, &x, &y, &surface))
        return false;
    cell_index = (size_t)y * (size_t)document->map.width + (size_t)x;
    if (!scene_document_get_surface_material(
            document, x, y, surface, &material) || material < 0 ||
        material > UINT16_MAX) return false;
    if (out_material_id) *out_material_id = (uint16_t)material;
    if (out_cell_index) *out_cell_index = cell_index;
    if (scope == EDITOR_OPTICAL_SCOPE_MATERIAL)
        return scene_document_get_optical_material_extension(
            document, (uint16_t)material, out_extension);
    if (scope == EDITOR_OPTICAL_SCOPE_CELL)
        return scene_document_get_optical_cell_extension(
            document, cell_index, out_extension);
    return false;
}

bool editor_domain_format_optical_field(
    const OpticalExtension *extension, EditorOpticalField field,
    char *out_text, size_t out_size
) {
    OpticalExtension copy;
    uint8_t bit;
    uint8_t *value;
    bool boolean;
    if (!extension || !out_text || out_size == 0U) return false;
    copy = *extension;
    if (!optical_field_parts(&copy, field, &bit, &value, &boolean)) return false;
    if ((copy.override_mask & bit) == 0U)
        return snprintf(out_text, out_size, "inherit") >= 0;
    if (boolean)
        return snprintf(out_text, out_size, "%s", *value ? "yes" : "no") >= 0;
    return snprintf(out_text, out_size, "%u", (unsigned)*value) >= 0;
}

static bool make_optical_request(
    const SceneDocument *document, SelectionTarget target,
    EditorOpticalScope scope, OpticalExtension value,
    EditorMutationRequest *out_request
) {
    uint16_t material_id;
    size_t cell_index;
    OpticalExtension ignored;
    if (!out_request || !editor_domain_get_optical_extension(
            document, target, scope, &ignored, &material_id, &cell_index)) return false;
    memset(out_request, 0, sizeof(*out_request));
    if (scope == EDITOR_OPTICAL_SCOPE_MATERIAL) {
        out_request->type = EDITOR_MUTATION_SET_OPTICAL_MATERIAL;
        out_request->data.optical_material.material_id = material_id;
        out_request->data.optical_material.value = value;
    } else {
        out_request->type = EDITOR_MUTATION_SET_OPTICAL_CELL;
        out_request->data.optical_cell.cell_index = cell_index;
        out_request->data.optical_cell.value = value;
    }
    return true;
}

static void apply_optical_extension(OpticalResolved *resolved,
                                    const OpticalExtension *extension) {
    if ((extension->override_mask & OPTICAL_OVERRIDE_RAY_BLOCKS) != 0U)
        resolved->ray_blocks = extension->ray_blocks != 0U;
    if ((extension->override_mask & OPTICAL_OVERRIDE_LIGHT_BLOCKS) != 0U)
        resolved->light_blocks = extension->light_blocks != 0U;
    if ((extension->override_mask & OPTICAL_OVERRIDE_OPACITY) != 0U)
        resolved->opacity = extension->opacity;
    if ((extension->override_mask & OPTICAL_OVERRIDE_TRANSMISSION) != 0U)
        resolved->transmission = extension->transmission;
}

static bool effective_transparency_optical(
    const SceneDocument *document, SelectionTarget target,
    EditorOpticalScope scope, OpticalResolved *out_resolved,
    OpticalExtension *out_local
) {
    OpticalResolved resolved = optical_resolved_legacy(true);
    OpticalExtension material = {0};
    OpticalExtension local = {0};
    uint16_t material_id;
    size_t cell_index;
    if (!out_resolved || !out_local ||
        !editor_domain_get_optical_extension(
            document, target, scope, &local, &material_id, &cell_index)) return false;
    (void)scene_document_get_optical_material_extension(
        document, material_id, &material);
    apply_optical_extension(&resolved, &material);
    if (scope == EDITOR_OPTICAL_SCOPE_CELL) {
        OpticalExtension cell = {0};
        (void)scene_document_get_optical_cell_extension(
            document, cell_index, &cell);
        apply_optical_extension(&resolved, &cell);
    }
    *out_resolved = resolved;
    *out_local = local;
    return true;
}

static uint8_t transparency_opacity(unsigned int percent) {
    return (uint8_t)((percent * UINT8_MAX + 50U) / 100U);
}

static uint8_t transparency_transmission(unsigned int percent) {
    return (uint8_t)(((100U - percent) * UINT8_MAX + 50U) / 100U);
}

bool editor_domain_transparency_value(
    const SceneDocument *document, SelectionTarget target,
    EditorOpticalScope scope, unsigned int *out_percent, bool *out_custom
) {
    OpticalResolved resolved;
    OpticalExtension local;
    unsigned int percent;
    if (!out_percent || !out_custom || !effective_transparency_optical(
            document, target, scope, &resolved, &local)) return false;
    (void)local;
    for (percent = 0U; percent <= 100U; percent++) {
        bool blocks = percent == 100U;
        if (resolved.opacity == transparency_opacity(percent) &&
            resolved.transmission == transparency_transmission(percent) &&
            resolved.ray_blocks == blocks && resolved.light_blocks == blocks) {
            *out_percent = percent;
            *out_custom = false;
            return true;
        }
    }
    *out_percent = 0U;
    *out_custom = true;
    return true;
}

bool editor_domain_format_transparency(
    const SceneDocument *document, SelectionTarget target,
    EditorOpticalScope scope, char *out_text, size_t out_size
) {
    unsigned int percent;
    bool custom;
    if (!out_text || out_size == 0U || !editor_domain_transparency_value(
            document, target, scope, &percent, &custom)) return false;
    if (custom) return snprintf(out_text, out_size, "Custom") >= 0;
    return snprintf(out_text, out_size, "%u%%", percent) >= 0;
}

static unsigned int transparency_estimate(const OpticalResolved *resolved) {
    unsigned int opacity_percent =
        ((unsigned int)resolved->opacity * 100U + 127U) / UINT8_MAX;
    unsigned int transmission_percent =
        ((unsigned int)(UINT8_MAX - resolved->transmission) * 100U + 127U) /
        UINT8_MAX;
    unsigned int estimate = (opacity_percent + transmission_percent + 1U) / 2U;
    return ((estimate + 2U) / 5U) * 5U;
}

bool editor_domain_make_transparency_step_request(
    const SceneDocument *document, SelectionTarget target,
    EditorOpticalScope scope, int direction,
    EditorMutationRequest *out_request
) {
    OpticalResolved resolved;
    OpticalExtension value;
    unsigned int percent;
    bool custom;
    int next;
    const uint8_t mask = OPTICAL_OVERRIDE_RAY_BLOCKS |
        OPTICAL_OVERRIDE_LIGHT_BLOCKS | OPTICAL_OVERRIDE_OPACITY |
        OPTICAL_OVERRIDE_TRANSMISSION;
    if ((direction != -1 && direction != 1) ||
        !effective_transparency_optical(
            document, target, scope, &resolved, &value) ||
        !editor_domain_transparency_value(
            document, target, scope, &percent, &custom)) return false;
    if (custom) percent = transparency_estimate(&resolved);
    next = (int)percent + direction * 5;
    if (next < 0) next = 0;
    if (next > 100) next = 100;
    percent = (unsigned int)next;
    value.override_mask = (uint8_t)(value.override_mask | mask);
    value.opacity = transparency_opacity(percent);
    value.transmission = transparency_transmission(percent);
    value.ray_blocks = percent == 100U ? 1U : 0U;
    value.light_blocks = percent == 100U ? 1U : 0U;
    return make_optical_request(document, target, scope, value, out_request);
}

bool editor_domain_make_transparency_inherit_request(
    const SceneDocument *document, SelectionTarget target,
    EditorOpticalScope scope, EditorMutationRequest *out_request
) {
    OpticalExtension value;
    const uint8_t mask = OPTICAL_OVERRIDE_RAY_BLOCKS |
        OPTICAL_OVERRIDE_LIGHT_BLOCKS | OPTICAL_OVERRIDE_OPACITY |
        OPTICAL_OVERRIDE_TRANSMISSION;
    if (!editor_domain_get_optical_extension(
            document, target, scope, &value, NULL, NULL)) return false;
    value.override_mask = (uint8_t)(value.override_mask & (uint8_t)~mask);
    value.ray_blocks = 0U;
    value.light_blocks = 0U;
    value.opacity = 0U;
    value.transmission = 0U;
    return make_optical_request(document, target, scope, value, out_request);
}

bool editor_domain_make_optical_step_request(
    const SceneDocument *document, SelectionTarget target,
    EditorOpticalScope scope, EditorOpticalField field, int direction,
    EditorMutationRequest *out_request
) {
    OpticalExtension value;
    uint8_t bit;
    uint8_t *field_value;
    bool boolean;
    if ((direction != -1 && direction != 1) ||
        !editor_domain_get_optical_extension(
            document, target, scope, &value, NULL, NULL) ||
        !optical_field_parts(&value, field, &bit, &field_value, &boolean)) return false;
    if ((value.override_mask & bit) == 0U) {
        value.override_mask = (uint8_t)(value.override_mask | bit);
        *field_value = boolean ? (uint8_t)(direction > 0) :
            (direction > 0 ? UINT8_C(16) : UINT8_MAX);
    } else if (boolean) {
        *field_value = (uint8_t)!*field_value;
    } else {
        int next = (int)*field_value + direction * 16;
        if (next < 0) next = 0;
        if (next > 255) next = 255;
        *field_value = (uint8_t)next;
    }
    return make_optical_request(document, target, scope, value, out_request);
}

bool editor_domain_make_optical_inherit_toggle_request(
    const SceneDocument *document, SelectionTarget target,
    EditorOpticalScope scope, EditorOpticalField field,
    EditorMutationRequest *out_request
) {
    OpticalExtension value;
    uint8_t bit;
    uint8_t *field_value;
    bool boolean;
    if (!editor_domain_get_optical_extension(
            document, target, scope, &value, NULL, NULL) ||
        !optical_field_parts(&value, field, &bit, &field_value, &boolean)) return false;
    (void)boolean;
    if ((value.override_mask & bit) != 0U) {
        value.override_mask = (uint8_t)(value.override_mask & (uint8_t)~bit);
        *field_value = 0U;
    } else {
        value.override_mask = (uint8_t)(value.override_mask | bit);
        *field_value = 0U;
    }
    return make_optical_request(document, target, scope, value, out_request);
}

bool editor_domain_make_wall_material_request(
    SelectionTarget target,
    MaterialId material,
    EditorMutationRequest *out_request
) {
    if (!out_request || target.type != SELECTION_WALL_FACE) return false;
    memset(out_request, 0, sizeof(*out_request));
    out_request->type = EDITOR_MUTATION_SET_WALL_MATERIAL;
    out_request->data.wall_material.wall.map_x = target.value.wall_face.map_x;
    out_request->data.wall_material.wall.map_y = target.value.wall_face.map_y;
    out_request->data.wall_material.material = material;
    return true;
}

bool editor_domain_light_field_metadata(
    EditorLightField field,
    const Map *map,
    EditorLightFieldMetadata *out_metadata
) {
    EditorLightFieldMetadata metadata = {field, NULL, 0.0, 0.0, 0.0, 0U};
    if (!out_metadata) return false;
    switch (field) {
        case EDITOR_LIGHT_FIELD_X:
            if (!map || map->width <= 0) return false;
            metadata = (EditorLightFieldMetadata){
                field, "X", 0.0, (double)map->width - EDITOR_LIGHT_POSITION_EDGE,
                EDITOR_LIGHT_POSITION_STEP, 2U};
            break;
        case EDITOR_LIGHT_FIELD_Y:
            if (!map || map->height <= 0) return false;
            metadata = (EditorLightFieldMetadata){
                field, "Y", 0.0, (double)map->height - EDITOR_LIGHT_POSITION_EDGE,
                EDITOR_LIGHT_POSITION_STEP, 2U};
            break;
        case EDITOR_LIGHT_FIELD_RED:
            metadata = (EditorLightFieldMetadata){field, "Red", 0.0, 255.0, 1.0, 0U};
            break;
        case EDITOR_LIGHT_FIELD_GREEN:
            metadata = (EditorLightFieldMetadata){field, "Green", 0.0, 255.0, 1.0, 0U};
            break;
        case EDITOR_LIGHT_FIELD_BLUE:
            metadata = (EditorLightFieldMetadata){field, "Blue", 0.0, 255.0, 1.0, 0U};
            break;
        case EDITOR_LIGHT_FIELD_ALPHA:
            metadata = (EditorLightFieldMetadata){field, "Alpha", 0.0, 255.0, 1.0, 0U};
            break;
        case EDITOR_LIGHT_FIELD_INTENSITY:
            metadata = (EditorLightFieldMetadata){
                field, "Intensity", EDITOR_LIGHT_INTENSITY_MIN,
                EDITOR_LIGHT_INTENSITY_MAX, EDITOR_LIGHT_INTENSITY_STEP, 2U};
            break;
        case EDITOR_LIGHT_FIELD_RADIUS:
            metadata = (EditorLightFieldMetadata){
                field, "Radius", EDITOR_LIGHT_RADIUS_MIN,
                EDITOR_LIGHT_RADIUS_MAX, EDITOR_LIGHT_RADIUS_STEP, 2U};
            break;
        case EDITOR_LIGHT_FIELD_TYPE:
            metadata = (EditorLightFieldMetadata){field, "Type (0=Point 1=Spot)", 0.0, 1.0, 1.0, 0U};
            break;
        case EDITOR_LIGHT_FIELD_DIRECTION:
            metadata = (EditorLightFieldMetadata){
                field, "Direction", SCENE_LIGHT_DIRECTION_MIN,
                SCENE_LIGHT_DIRECTION_MAX - 0.01, 0.05, 2U};
            break;
        case EDITOR_LIGHT_FIELD_CONE:
            metadata = (EditorLightFieldMetadata){
                field, "Cone", SCENE_LIGHT_CONE_MIN,
                SCENE_LIGHT_CONE_MAX, 0.05, 2U};
            break;
        case EDITOR_LIGHT_FIELD_FALLOFF:
            metadata = (EditorLightFieldMetadata){
                field, "Falloff", SCENE_LIGHT_FALLOFF_MIN,
                SCENE_LIGHT_FALLOFF_MAX, 0.1, 2U};
            break;
        case EDITOR_LIGHT_FIELD_REMOVE:
        case EDITOR_LIGHT_FIELD_COUNT:
        default:
            return false;
    }
    *out_metadata = metadata;
    return true;
}

static double clamp_step(double value, const EditorLightFieldMetadata *metadata,
                         int direction) {
    double next = value + (direction < 0 ? -metadata->step : metadata->step);
    if (next < metadata->minimum) return metadata->minimum;
    if (next > metadata->maximum) return metadata->maximum;
    return next;
}

bool editor_domain_format_light_field(
    const SceneLight *light,
    EditorLightField field,
    char *out_text,
    size_t out_size
) {
    int written;
    if (!light || !out_text || out_size == 0U) return false;
    switch (field) {
        case EDITOR_LIGHT_FIELD_X: written = snprintf(out_text, out_size, "%.2f", light->x); break;
        case EDITOR_LIGHT_FIELD_Y: written = snprintf(out_text, out_size, "%.2f", light->y); break;
        case EDITOR_LIGHT_FIELD_RED: written = snprintf(out_text, out_size, "%u", light->red); break;
        case EDITOR_LIGHT_FIELD_GREEN: written = snprintf(out_text, out_size, "%u", light->green); break;
        case EDITOR_LIGHT_FIELD_BLUE: written = snprintf(out_text, out_size, "%u", light->blue); break;
        case EDITOR_LIGHT_FIELD_ALPHA: written = snprintf(out_text, out_size, "%u", light->alpha); break;
        case EDITOR_LIGHT_FIELD_INTENSITY:
            written = snprintf(out_text, out_size, "%.2f", light->intensity);
            break;
        case EDITOR_LIGHT_FIELD_RADIUS:
            written = snprintf(out_text, out_size, "%.2f", light->radius);
            break;
        case EDITOR_LIGHT_FIELD_TYPE:
            written = snprintf(out_text, out_size, "%s",
                               light->type == SCENE_LIGHT_SPOT ? "Spot" : "Point");
            break;
        case EDITOR_LIGHT_FIELD_DIRECTION:
            written = snprintf(out_text, out_size, "%.2f", light->direction);
            break;
        case EDITOR_LIGHT_FIELD_CONE:
            written = snprintf(out_text, out_size, "%.2f", light->cone);
            break;
        case EDITOR_LIGHT_FIELD_FALLOFF:
            written = snprintf(out_text, out_size, "%.2f", light->falloff);
            break;
        case EDITOR_LIGHT_FIELD_REMOVE:
            written = snprintf(out_text, out_size, "Enter=remove");
            break;
        case EDITOR_LIGHT_FIELD_COUNT:
        default:
            return false;
    }
    return written >= 0 && (size_t)written < out_size;
}

bool editor_domain_make_light_step_request(
    const SceneDocument *document,
    SelectionTarget target,
    EditorLightField field,
    int direction,
    EditorMutationRequest *out_request
) {
    const SceneLight *current;
    EditorLightFieldMetadata metadata;
    SceneLight value;
    double next;
    if (!document || !out_request || target.type != SELECTION_LIGHT ||
        (direction != -1 && direction != 1) ||
        !editor_domain_light_field_metadata(
            field, scene_document_get_map(document), &metadata)) return false;
    current = scene_document_find_light(document, target.value.light.id);
    if (!current) return false;
    value = *current;
    switch (field) {
        case EDITOR_LIGHT_FIELD_X:
            value.x = clamp_step(value.x, &metadata, direction);
            break;
        case EDITOR_LIGHT_FIELD_Y:
            value.y = clamp_step(value.y, &metadata, direction);
            break;
        case EDITOR_LIGHT_FIELD_RED:
            next = clamp_step(value.red, &metadata, direction);
            value.red = (uint8_t)next;
            break;
        case EDITOR_LIGHT_FIELD_GREEN:
            next = clamp_step(value.green, &metadata, direction);
            value.green = (uint8_t)next;
            break;
        case EDITOR_LIGHT_FIELD_BLUE:
            next = clamp_step(value.blue, &metadata, direction);
            value.blue = (uint8_t)next;
            break;
        case EDITOR_LIGHT_FIELD_ALPHA:
            next = clamp_step(value.alpha, &metadata, direction);
            value.alpha = (uint8_t)next;
            break;
        case EDITOR_LIGHT_FIELD_INTENSITY:
            value.intensity = clamp_step(value.intensity, &metadata, direction);
            break;
        case EDITOR_LIGHT_FIELD_RADIUS:
            value.radius = clamp_step(value.radius, &metadata, direction);
            break;
        case EDITOR_LIGHT_FIELD_TYPE:
            if (direction < 0) {
                value.type = SCENE_LIGHT_POINT;
            } else {
                value.type = SCENE_LIGHT_SPOT;
                if (value.cone == SCENE_LIGHT_CONE_MAX)
                    value.cone = SCENE_LIGHT_SPOT_CONE_DEFAULT;
            }
            break;
        case EDITOR_LIGHT_FIELD_DIRECTION:
            value.direction = clamp_step(value.direction, &metadata, direction);
            break;
        case EDITOR_LIGHT_FIELD_CONE:
            value.cone = clamp_step(value.cone, &metadata, direction);
            break;
        case EDITOR_LIGHT_FIELD_FALLOFF:
            value.falloff = clamp_step(value.falloff, &metadata, direction);
            break;
        case EDITOR_LIGHT_FIELD_COUNT:
        default:
            return false;
    }
    memset(out_request, 0, sizeof(*out_request));
    out_request->type = EDITOR_MUTATION_SET_LIGHT;
    out_request->data.light.id = target.value.light.id;
    out_request->data.light.value = value;
    return true;
}

bool editor_domain_make_light_value_request(
    const SceneDocument *document,
    SelectionTarget target,
    EditorLightField field,
    double numeric_value,
    EditorMutationRequest *out_request
) {
    const SceneLight *current;
    EditorLightFieldMetadata metadata;
    SceneLight value;
    if (!document || !out_request || target.type != SELECTION_LIGHT ||
        !isfinite(numeric_value) ||
        !editor_domain_light_field_metadata(
            field, scene_document_get_map(document), &metadata) ||
        numeric_value < metadata.minimum || numeric_value > metadata.maximum) {
        return false;
    }
    current = scene_document_find_light(document, target.value.light.id);
    if (!current) return false;
    value = *current;
    switch (field) {
        case EDITOR_LIGHT_FIELD_X: value.x = numeric_value; break;
        case EDITOR_LIGHT_FIELD_Y: value.y = numeric_value; break;
        case EDITOR_LIGHT_FIELD_RED:
            if (numeric_value != (double)(uint8_t)numeric_value) return false;
            value.red = (uint8_t)numeric_value;
            break;
        case EDITOR_LIGHT_FIELD_GREEN:
            if (numeric_value != (double)(uint8_t)numeric_value) return false;
            value.green = (uint8_t)numeric_value;
            break;
        case EDITOR_LIGHT_FIELD_BLUE:
            if (numeric_value != (double)(uint8_t)numeric_value) return false;
            value.blue = (uint8_t)numeric_value;
            break;
        case EDITOR_LIGHT_FIELD_ALPHA:
            if (numeric_value != (double)(uint8_t)numeric_value) return false;
            value.alpha = (uint8_t)numeric_value;
            break;
        case EDITOR_LIGHT_FIELD_INTENSITY: value.intensity = numeric_value; break;
        case EDITOR_LIGHT_FIELD_RADIUS: value.radius = numeric_value; break;
        case EDITOR_LIGHT_FIELD_TYPE:
            if (numeric_value != 0.0 && numeric_value != 1.0) return false;
            value.type = numeric_value == 0.0 ? SCENE_LIGHT_POINT : SCENE_LIGHT_SPOT;
            if (value.type == SCENE_LIGHT_SPOT && value.cone == SCENE_LIGHT_CONE_MAX)
                value.cone = SCENE_LIGHT_SPOT_CONE_DEFAULT;
            break;
        case EDITOR_LIGHT_FIELD_DIRECTION: value.direction = numeric_value; break;
        case EDITOR_LIGHT_FIELD_CONE: value.cone = numeric_value; break;
        case EDITOR_LIGHT_FIELD_FALLOFF: value.falloff = numeric_value; break;
        case EDITOR_LIGHT_FIELD_COUNT:
        default: return false;
    }
    memset(out_request, 0, sizeof(*out_request));
    out_request->type = EDITOR_MUTATION_SET_LIGHT;
    out_request->data.light.id = target.value.light.id;
    out_request->data.light.value = value;
    return true;
}