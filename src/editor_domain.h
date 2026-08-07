/**
 * editor_domain.h — Typed inspector adapters for authored editor domains
 *
 * Pure helpers describe inspector fields and construct command requests. They
 * borrow document values and never mutate authored or runtime state.
 */
#ifndef EDITOR_DOMAIN_H
#define EDITOR_DOMAIN_H

#include "command_system.h"
#include "editor_types.h"
#include "scene_document.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    EDITOR_INSPECTOR_NONE = 0,
    EDITOR_INSPECTOR_WALL_MATERIAL,
    EDITOR_INSPECTOR_LIGHT
} EditorInspectorKind;

typedef enum {
    EDITOR_LIGHT_FIELD_X = 0,
    EDITOR_LIGHT_FIELD_Y,
    EDITOR_LIGHT_FIELD_RED,
    EDITOR_LIGHT_FIELD_GREEN,
    EDITOR_LIGHT_FIELD_BLUE,
    EDITOR_LIGHT_FIELD_ALPHA,
    EDITOR_LIGHT_FIELD_INTENSITY,
    EDITOR_LIGHT_FIELD_RADIUS,
    EDITOR_LIGHT_FIELD_COUNT
} EditorLightField;

typedef struct {
    EditorLightField field;
    const char *label;
    double minimum;
    double maximum;
    double step;
    unsigned int decimal_places;
} EditorLightFieldMetadata;

typedef enum {
    EDITOR_INSPECTOR_FIELD_CHOICE = 0,
    EDITOR_INSPECTOR_FIELD_NUMBER
} EditorInspectorFieldKind;

typedef struct {
    const char *title;
    const char *controls;
    const char *note;
    size_t field_count;
} EditorInspectorPresentation;

typedef struct {
    const char *label;
    EditorInspectorFieldKind kind;
    double minimum;
    double maximum;
    double step;
    unsigned int decimal_places;
} EditorInspectorFieldPresentation;

EditorInspectorKind editor_domain_inspector_kind(SelectionTarget target);

bool editor_domain_inspector_presentation(
    EditorInspectorKind kind,
    EditorInspectorPresentation *out_presentation
);

bool editor_domain_inspector_field_presentation(
    EditorInspectorKind kind,
    size_t field_index,
    const Map *map,
    EditorInspectorFieldPresentation *out_presentation
);

bool editor_domain_make_wall_material_request(
    SelectionTarget target,
    MaterialId material,
    EditorMutationRequest *out_request
);

bool editor_domain_light_field_metadata(
    EditorLightField field,
    const Map *map,
    EditorLightFieldMetadata *out_metadata
);

bool editor_domain_format_light_field(
    const SceneLight *light,
    EditorLightField field,
    char *out_text,
    size_t out_size
);

bool editor_domain_make_light_step_request(
    const SceneDocument *document,
    SelectionTarget target,
    EditorLightField field,
    int direction,
    EditorMutationRequest *out_request
);

bool editor_domain_make_light_value_request(
    const SceneDocument *document,
    SelectionTarget target,
    EditorLightField field,
    double numeric_value,
    EditorMutationRequest *out_request
);

#endif /* EDITOR_DOMAIN_H */