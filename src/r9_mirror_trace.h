/** Compile-time-gated R9 P4 single-bounce mirror tracing research. */
#ifndef R9_MIRROR_TRACE_H
#define R9_MIRROR_TRACE_H

#ifdef R9_OPTICAL_RESEARCH

#include "heightfield_trace.h"
#include "r9_optical_semantics.h"

#include <stdbool.h>

#define R9_MIRROR_ORIGIN_EPSILON 0.000001
#define R9_MIRROR_MAX_BOUNCES 1U

typedef enum {
    R9_MIRROR_RESULT_REFLECTED_HIT = 0,
    R9_MIRROR_RESULT_DARKNESS_FALLBACK
} R9MirrorResultKind;

typedef struct {
    R9MirrorResultKind kind;
    HeightfieldHit reflected_hit;
    double reflected_direction_x;
    double reflected_direction_y;
    unsigned int bounce_count;
} R9MirrorSample;

bool r9_mirror_reflect_direction(double incoming_x, double incoming_y,
                                 int mirror_side, double *out_x, double *out_y);
bool r9_mirror_sample_once(const HeightfieldTraceColumn *incoming_column,
                           const HeightfieldHit *mirror_hit,
                           const R9OpticalResolved *mirror_optical,
                           int screen_y, double max_distance,
                           R9MirrorSample *out_sample);

#endif /* R9_OPTICAL_RESEARCH */

#endif /* R9_MIRROR_TRACE_H */