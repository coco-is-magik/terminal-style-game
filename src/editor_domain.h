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
    EDITOR_INSPECTOR_LIGHT,
    EDITOR_INSPECTOR_FLOOR_SURFACE,
    EDITOR_INSPECTOR_CEILING_SURFACE,
    EDITOR_INSPECTOR_DECAL
} EditorInspectorKind;

typedef enum {
    EDITOR_SURFACE_FIELD_MATERIAL = 0,
    EDITOR_SURFACE_FIELD_CONSTRUCTION,
    EDITOR_SURFACE_FIELD_AMBIENT,
    EDITOR_SURFACE_FIELD_DECALS,
    EDITOR_SURFACE_FIELD_COUNT
} EditorSurfaceField;

typedef struct {
    double minimum;
    double maximum;
    double step;
    unsigned int decimal_places;
} EditorAmbientMetadata;

typedef enum {
    EDITOR_LIGHT_FIELD_X = 0,
    EDITOR_LIGHT_FIELD_Y,
    EDITOR_LIGHT_FIELD_RED,
    EDITOR_LIGHT_FIELD_GREEN,
    EDITOR_LIGHT_FIELD_BLUE,
    EDITOR_LIGHT_FIELD_INTENSITY,
    EDITOR_LIGHT_FIELD_RADIUS,
    EDITOR_LIGHT_FIELD_REMOVE,
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
    EDITOR_DECAL_FIELD_POSITION_U = 0,
    EDITOR_DECAL_FIELD_POSITION_V,
    EDITOR_DECAL_FIELD_WIDTH,
    EDITOR_DECAL_FIELD_HEIGHT,
    EDITOR_DECAL_FIELD_ROTATION,
    EDITOR_DECAL_FIELD_DEPTH,
    EDITOR_DECAL_FIELD_REMOVE,
    EDITOR_DECAL_FIELD_COUNT
} EditorDecalField;

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
bool editor_domain_make_surface_material_request(
    SelectionTarget target,
    MaterialId material,
    EditorMutationRequest *out_request
);
bool editor_domain_make_construction_request(
    SelectionTarget target,
    EditorMutationRequest *out_request
);
bool editor_domain_ambient_metadata(EditorAmbientMetadata *out_metadata);
bool editor_domain_format_ambient(
    double ambient,
    char *out_text,
    size_t out_size
);
bool editor_domain_make_ambient_step_request(
    const SceneDocument *document,
    int direction,
    EditorMutationRequest *out_request
);
bool editor_domain_make_ambient_value_request(
    double value,
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

bool editor_domain_decal_field_presentation(
    EditorDecalField field,
    EditorInspectorFieldPresentation *out_presentation
);
bool editor_domain_format_decal_field(
    const SceneDecalInstance *decal,
    EditorDecalField field,
    char *out_text,
    size_t out_size
);
bool editor_domain_make_decal_step_request(
    const SceneDocument *document,
    SelectionTarget target,
    EditorDecalField field,
    int direction,
    EditorMutationRequest *out_request
);
bool editor_domain_make_decal_value_request(
    const SceneDocument *document,
    SelectionTarget target,
    EditorDecalField field,
    double numeric_value,
    EditorMutationRequest *out_request
);

#endif /* EDITOR_DOMAIN_H */