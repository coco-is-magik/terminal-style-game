/**
 * scene_format.h — Headless native scene format and migration boundary
 *
 * Parsing and serialization operate only on owned authored values. They perform
 * no file I/O and never mutate a SceneDocument or runtime-derived state.
 */

#ifndef SCENE_FORMAT_H
#define SCENE_FORMAT_H

#include "map.h"
#include "optical_runtime_view.h"
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
    SceneMovementParameters movement;
    SceneLight *lights;
    size_t light_count;
    SceneDecalInstance *decals;
    size_t decal_count;
    SceneSpriteInstance *sprites;
    size_t sprite_count;
    SceneTrigger *triggers;
    size_t trigger_count;
    SceneObjectInstance *objects;
    size_t object_count;
    OpticalExtension *optical_material_defaults;
    size_t optical_material_capacity;
    OpticalCellOverride *optical_cell_overrides;
    size_t optical_cell_override_count;
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

/* Applies v5 height/movement defaults in place to an already parsed v1-v4 candidate. */
SceneFormatResult scene_format_migrate_to_v5(
    SceneFormatCandidate *candidate,
    SceneDiagnostic *out_diagnostic
);

/* Adds the allocation-free legacy optical defaults to a parsed v5 candidate. */
SceneFormatResult scene_format_migrate_v5_to_v6(
    SceneFormatCandidate *candidate,
    SceneDiagnostic *out_diagnostic
);

/* Adds explicit point-light type/cone/falloff defaults to a parsed v6 candidate. */
SceneFormatResult scene_format_migrate_v6_to_v7(
    SceneFormatCandidate *candidate,
    SceneDiagnostic *out_diagnostic
);

/* Adds the empty authored sprite collection to a parsed v7 candidate. */
SceneFormatResult scene_format_migrate_v7_to_v8(
    SceneFormatCandidate *candidate,
    SceneDiagnostic *out_diagnostic
);
SceneFormatResult scene_format_migrate_v8_to_v9(
    SceneFormatCandidate *candidate, SceneDiagnostic *out_diagnostic
);
SceneFormatResult scene_format_migrate_v9_to_v10(
    SceneFormatCandidate *candidate, SceneDiagnostic *out_diagnostic
);
SceneFormatResult scene_format_migrate_v10_to_v11(
    SceneFormatCandidate *candidate, SceneDiagnostic *out_diagnostic
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

/* ---- Load-time memory policy ----
 *
 * The format ceiling (SCENE_MAX_WIDTH/HEIGHT) says what a file may contain; this policy says
 * what a given host will accept before allocating. The fraction is deliberately a build-time
 * default, not a magic number in the load path: override it with
 * -DSCENE_LOAD_MEMORY_BUDGET_PERCENT=n for a bigger or smaller host budget. `config.ini`
 * promotion is recorded as a follow-up because the format layer does not link the config
 * module today.
 *
 * Unknown installed memory (0) means "no budget": the ceiling and the allocation-failure
 * diagnostic still apply, so behaviour degrades to the pre-guard path rather than guessing.
 */
#ifndef SCENE_LOAD_MEMORY_BUDGET_PERCENT
#define SCENE_LOAD_MEMORY_BUDGET_PERCENT 25
#endif

/* Bytes a load needs for a map of this size: the material grid, the light map, and (for
   v2+ scenes) the authored-cell array. Pure arithmetic, so it is testable without a
   machine-specific budget. Returns false on overflow. */
bool scene_format_load_cost_bytes(int width, int height, bool authored_cells,
                                  size_t *out_bytes);

/* True when a load of `cost_bytes` fits `installed_bytes` under the policy above.
   `installed_bytes` of 0 means unknown, which always fits. Pure, so the policy can be
   tested with injected values. */
bool scene_format_load_fits_budget(size_t cost_bytes, size_t installed_bytes);

#endif /* SCENE_FORMAT_H */