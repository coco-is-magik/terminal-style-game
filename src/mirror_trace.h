/** mirror_trace.h — Coarse-reuse, one-bounce vertical-wall reflection. */
#ifndef MIRROR_TRACE_H
#define MIRROR_TRACE_H

#include "heightfield_trace.h"

#define MIRROR_TRACE_ORIGIN_EPSILON 0.000001
#define MIRROR_TRACE_MAX_BOUNCES 1U

typedef struct {
    Camera reflected_camera;
    HeightfieldTraceColumn reflected_column;
    double mirror_world_x;
    double mirror_world_y;
    int mirror_side;
    size_t preparation_count;
    bool prepared;
} MirrorTraceColumnCache;

typedef struct {
    HeightfieldOpticalResult optical;
    double reflected_direction_x;
    double reflected_direction_y;
    unsigned int bounce_count;
    bool darkness_fallback;
} MirrorTraceResult;

void mirror_trace_column_cache_init(MirrorTraceColumnCache *cache);

bool mirror_trace_reflect_direction(
    double incoming_x, double incoming_y, int mirror_side,
    double *out_x, double *out_y
);

/**
 * Sample one reflected row. The first mirror plane prepares the reflected XY
 * intervals; later rows on that same plane reuse them. A different plane returns
 * darkness without preparing another column.
 */
bool mirror_trace_sample_once(
    MirrorTraceColumnCache *cache,
    const HeightfieldTraceColumn *incoming_column,
    const HeightfieldHit *mirror_hit,
    const OpticalRuntimeView *optical_view,
    uint32_t source_generation,
    int screen_y,
    double max_distance,
    MirrorTraceResult *out_result
);

#endif /* MIRROR_TRACE_H */