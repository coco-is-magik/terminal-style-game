/**
 * heightfield_trace.h — Deterministic bounded-cell heightfield intersections
 *
 * The tracer is allocation-free and owns no state. Rendering and editor picking
 * use this same boundary so a horizontal surface cannot bleed outside its cell.
 */
#ifndef HEIGHTFIELD_TRACE_H
#define HEIGHTFIELD_TRACE_H

#include "camera.h"
#include "height_view.h"
#include "map.h"

typedef enum {
    HEIGHTFIELD_HIT_NONE = 0,
    HEIGHTFIELD_HIT_FLOOR,
    HEIGHTFIELD_HIT_CEILING,
    HEIGHTFIELD_HIT_WALL
} HeightfieldHitKind;

typedef struct {
    HeightfieldHitKind kind;
    double distance;
    double perpendicular_distance;
    double world_x;
    double world_y;
    double world_z;
    int map_x;
    int map_y;
    int side;
    uint16_t material;
    bool generated_boundary;
    bool hit;
} HeightfieldHit;

#define HEIGHTFIELD_TRACE_MAX_INTERVALS \
    (SCENE_MAX_WIDTH + SCENE_MAX_HEIGHT + 2)

typedef struct {
    const Camera *camera;
    const Map *map;
    const SceneHeightView *heights;
    int viewport_height;
    int start_map_x;
    int start_map_y;
    int step_x;
    int step_y;
    double direction_x;
    double direction_y;
    double correction;
    double delta_x;
    double delta_y;
    double initial_side_x;
    double initial_side_y;
    double max_distance;
    size_t interval_count;
    int interval_map_x[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    int interval_map_y[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    int interval_next_x[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    int interval_next_y[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    int interval_side[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    double interval_enter[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    double interval_exit[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    const SceneAuthoredCell *interval_cell[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    const SceneAuthoredCell *interval_next_cell[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    double interval_floor_z[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    double interval_ceiling_z[HEIGHTFIELD_TRACE_MAX_INTERVALS];
    bool valid;
} HeightfieldTraceColumn;

bool heightfield_view_is_flat_default(const SceneHeightView *view,
                                      int width, int height);

bool heightfield_trace_prepare_column(
    HeightfieldTraceColumn *column, const Camera *camera, const Map *map,
    const SceneHeightView *heights, int viewport_width, int viewport_height,
    int screen_x, double max_distance
);

HeightfieldHit heightfield_trace_prepared_sample(
    const HeightfieldTraceColumn *column, int screen_y
);

HeightfieldHit heightfield_trace_screen_sample(
    const Camera *camera, const Map *map, const SceneHeightView *heights,
    int viewport_width, int viewport_height, int screen_x, int screen_y,
    double max_distance
);

#endif /* HEIGHTFIELD_TRACE_H */