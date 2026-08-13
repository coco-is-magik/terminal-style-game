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
    return EDITOR_INSPECTOR_NONE;
}

bool editor_domain_inspector_presentation(
    EditorInspectorKind kind,
    EditorInspectorPresentation *out_presentation
) {
    if (!out_presentation) return false;
    if (kind == EDITOR_INSPECTOR_LIGHT) {
        *out_presentation = (EditorInspectorPresentation){
            "point light", "Up/Down  Left/Right=edit  Type+Enter=set",
            "RGB colors marker; scene illumination is scalar",
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
    if (kind == EDITOR_INSPECTOR_FLOOR_SURFACE ||
        kind == EDITOR_INSPECTOR_CEILING_SURFACE) {
        *out_presentation = (EditorInspectorPresentation){
            kind == EDITOR_INSPECTOR_FLOOR_SURFACE ? "floor surface" : "ceiling surface",
            "Up/Down=choose  Enter=open/apply  Esc=back",
            "Fixed-height surface; construction places one wall cell",
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
        } else *out_presentation = (EditorInspectorFieldPresentation){
            "Decals", EDITOR_INSPECTOR_FIELD_CHOICE, 0.0, 0.0, 0.0, 0U};
        return true;
    }
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
        case EDITOR_LIGHT_FIELD_INTENSITY:
            written = snprintf(out_text, out_size, "%.2f", light->intensity);
            break;
        case EDITOR_LIGHT_FIELD_RADIUS:
            written = snprintf(out_text, out_size, "%.2f", light->radius);
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
        case EDITOR_LIGHT_FIELD_INTENSITY:
            value.intensity = clamp_step(value.intensity, &metadata, direction);
            break;
        case EDITOR_LIGHT_FIELD_RADIUS:
            value.radius = clamp_step(value.radius, &metadata, direction);
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
        case EDITOR_LIGHT_FIELD_INTENSITY: value.intensity = numeric_value; break;
        case EDITOR_LIGHT_FIELD_RADIUS: value.radius = numeric_value; break;
        case EDITOR_LIGHT_FIELD_COUNT:
        default: return false;
    }
    memset(out_request, 0, sizeof(*out_request));
    out_request->type = EDITOR_MUTATION_SET_LIGHT;
    out_request->data.light.id = target.value.light.id;
    out_request->data.light.value = value;
    return true;
}