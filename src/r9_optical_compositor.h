/** Compile-time-gated R9 P3 terminal-cell optical compositor research. */
#ifndef R9_OPTICAL_COMPOSITOR_H
#define R9_OPTICAL_COMPOSITOR_H

#ifdef R9_OPTICAL_RESEARCH

#include "grid.h"
#include "r9_optical_semantics.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define R9_OPTICAL_COMPOSITOR_MAX_LAYERS 4U
#define R9_OPTICAL_GLYPH_OPACITY_THRESHOLD 128U

typedef struct {
    Cell sampled_cell;
    R9OpticalResolved optical;
    bool generated_boundary;
} R9OpticalLayer;

typedef struct {
    Cell cell;
    size_t consumed_layers;
    bool terminated_by_surface;
    bool reached_opening;
    bool layer_cap_exhausted;
} R9OpticalComposite;

bool r9_optical_sight_continues(const R9OpticalResolved *optical);
uint8_t r9_optical_transmit_light(const R9OpticalResolved *optical,
                                  uint8_t incoming_light);
bool r9_optical_composite(const R9OpticalLayer *layers, size_t layer_count,
                          bool terminal_opening, const Cell *opening_fallback,
                          R9OpticalComposite *out_composite);

#endif /* R9_OPTICAL_RESEARCH */

#endif /* R9_OPTICAL_COMPOSITOR_H */