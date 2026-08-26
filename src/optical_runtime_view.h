/**
 * optical_runtime_view.h — Borrowed derived optical lookup view
 *
 * The view owns nothing. Optional material defaults and sparse cell overrides
 * remain owned by their future authored/runtime adapter and must outlive the view.
 */
#ifndef OPTICAL_RUNTIME_VIEW_H
#define OPTICAL_RUNTIME_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OPTICAL_OVERRIDE_PLAYER_BLOCKS UINT8_C(0x01)
#define OPTICAL_OVERRIDE_RAY_BLOCKS UINT8_C(0x02)
#define OPTICAL_OVERRIDE_LIGHT_BLOCKS UINT8_C(0x04)
#define OPTICAL_OVERRIDE_OPACITY UINT8_C(0x08)
#define OPTICAL_OVERRIDE_TRANSMISSION UINT8_C(0x10)
#define OPTICAL_OVERRIDE_REFLECTIVITY UINT8_C(0x20)
#define OPTICAL_OVERRIDE_ALL UINT8_C(0x3F)
#define OPTICAL_MATERIAL_CAPACITY_MAX 65536U

typedef struct {
    uint8_t override_mask;
    uint8_t player_blocks;
    uint8_t ray_blocks;
    uint8_t light_blocks;
    uint8_t opacity;
    uint8_t transmission;
    uint8_t reflectivity;
    uint8_t reserved;
} OpticalExtension;

typedef struct {
    bool player_blocks;
    bool ray_blocks;
    bool light_blocks;
    uint8_t opacity;
    uint8_t transmission;
    uint8_t reflectivity;
} OpticalResolved;

typedef struct {
    uint32_t cell_index;
    OpticalExtension optical;
} OpticalCellOverride;

typedef struct {
    const OpticalExtension *material_defaults;
    size_t material_capacity;
    const OpticalCellOverride *cell_overrides;
    size_t cell_override_count;
    size_t cell_count;
    uint32_t source_generation;
    bool valid;
} OpticalRuntimeView;

OpticalResolved optical_resolved_legacy(bool legacy_blocks);
bool optical_extension_is_valid(const OpticalExtension *extension);

/** Transactionally initialize a borrowed, allocation-free lookup view. */
bool optical_runtime_view_init(
    OpticalRuntimeView *out_view,
    size_t cell_count,
    const OpticalExtension *material_defaults,
    size_t material_capacity,
    const OpticalCellOverride *cell_overrides,
    size_t cell_override_count,
    uint32_t source_generation
);

/** Return whether a valid borrowed view still matches its source generation. */
bool optical_runtime_view_is_current(
    const OpticalRuntimeView *view, uint32_t source_generation
);

/** Resolve legacy -> material default -> sparse cell override. */
bool optical_runtime_view_resolve(
    const OpticalRuntimeView *view,
    size_t cell_index,
    uint16_t material_id,
    bool legacy_blocks,
    OpticalResolved *out_resolved
);

#endif /* OPTICAL_RUNTIME_VIEW_H */
