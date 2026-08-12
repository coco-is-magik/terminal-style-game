/**
 * scene_format.h — Headless native scene format and migration boundary
 *
 * Parsing and serialization operate only on owned authored values. They perform
 * no file I/O and never mutate a SceneDocument or runtime-derived state.
 */

#ifndef SCENE_FORMAT_H
#define SCENE_FORMAT_H

#include "map.h"
#include "scene_diagnostic.h"
#include "scene_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    Map map;
    unsigned int source_version;
    SceneAuthoredCell *authored_cells;
    size_t authored_cell_count;
    char name[SCENE_NAME_MAX + 1U];
    double ambient_intensity;
    double spawn_x;
    double spawn_y;
    double spawn_angle;
    SceneLight *lights;
    size_t light_count;
    SceneDecalInstance *decals;
    size_t decal_count;
    SceneInstanceId next_instance_id;
    char *legacy_source_path;
    int east_growth[SCENE_MAX_WIDTH];
    size_t east_growth_count;
    int south_growth[SCENE_MAX_HEIGHT];
    size_t south_growth_count;
} SceneFormatCandidate;

typedef struct {
    char *data;
    size_t size;
} SceneFormatBuffer;

typedef enum {
    SCENE_FORMAT_OK = 0,
    SCENE_FORMAT_INVALID_ARGUMENT,
    SCENE_FORMAT_REJECTED,
    SCENE_FORMAT_OUT_OF_MEMORY
} SceneFormatResult;

void scene_format_candidate_init(SceneFormatCandidate *candidate);
void scene_format_candidate_destroy(SceneFormatCandidate *candidate);
void scene_format_buffer_destroy(SceneFormatBuffer *buffer);

void scene_format_set_allocator_for_test(
    void *(*calloc_fn)(size_t count, size_t size)
);
void scene_format_reset_allocator_for_test(void);

/* Transactionally adds the v2 authored surface model to a parsed v1 candidate. */
SceneFormatResult scene_format_migrate_v1_to_v2(
    SceneFormatCandidate *candidate,
    unsigned int default_material,
    SceneDiagnostic *out_diagnostic
);

SceneFormatResult scene_format_parse(
    const char *source,
    size_t source_size,
    const char *source_path,
    SceneFormatCandidate *out_candidate,
    SceneDiagnostic *out_diagnostic
);

SceneFormatResult scene_format_validate(
    const SceneFormatCandidate *candidate,
    const char *source_path,
    SceneDiagnostic *out_diagnostic
);

SceneFormatResult scene_format_serialize(
    const SceneFormatCandidate *candidate,
    SceneFormatBuffer *out_buffer,
    SceneDiagnostic *out_diagnostic
);

#endif /* SCENE_FORMAT_H */
