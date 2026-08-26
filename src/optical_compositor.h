/** optical_compositor.h — Deterministic bounded terminal-cell composition */
#ifndef OPTICAL_COMPOSITOR_H
#define OPTICAL_COMPOSITOR_H

#include "grid.h"
#include "optical_runtime_view.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OPTICAL_COMPOSITOR_MAX_LAYERS 4U
#define OPTICAL_GLYPH_OPACITY_THRESHOLD 128U

typedef struct {
    Cell sampled_cell;
    OpticalResolved optical;
    bool generated_boundary;
} OpticalCompositeLayer;

typedef struct {
    Cell cell;
    size_t consumed_layers;
    bool terminated_by_surface;
    bool reached_opening;
    bool layer_cap_exhausted;
} OpticalCompositeResult;

/** Compose near-to-far input layers by blending colors far-to-near. */
bool optical_composite_layers(
    const OpticalCompositeLayer *layers,
    size_t layer_count,
    bool terminated_by_surface,
    bool reached_opening,
    bool layer_cap_exhausted,
    const Cell *fallback,
    OpticalCompositeResult *out_result
);

/** Mix direct mirror appearance with one reflected composite by reflectivity. */
bool optical_mix_reflection(
    const Cell *direct_cell, const Cell *reflected_cell,
    uint8_t reflectivity, Cell *out_cell
);

#endif /* OPTICAL_COMPOSITOR_H */