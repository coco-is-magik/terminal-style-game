#include "optical_runtime_view.h"

#include <limits.h>

_Static_assert(sizeof(OpticalExtension) == 8U,
               "optical extension layout is part of the I1 lookup budget");
_Static_assert(sizeof(OpticalCellOverride) == 12U,
               "sparse optical override layout is part of the I1 lookup budget");

OpticalResolved optical_resolved_legacy(bool legacy_blocks) {
    OpticalResolved resolved;
    resolved.player_blocks = legacy_blocks;
    resolved.ray_blocks = legacy_blocks;
    resolved.light_blocks = legacy_blocks;
    resolved.opacity = legacy_blocks ? UINT8_MAX : 0U;
    resolved.transmission = legacy_blocks ? 0U : UINT8_MAX;
    resolved.reflectivity = 0U;
    return resolved;
}

bool optical_extension_is_valid(const OpticalExtension *extension) {
    if (!extension ||
        (extension->override_mask & (uint8_t)~OPTICAL_OVERRIDE_ALL) != 0U ||
        extension->reserved != 0U) return false;
    if ((extension->override_mask & OPTICAL_OVERRIDE_PLAYER_BLOCKS) != 0U &&
        extension->player_blocks > 1U) return false;
    if ((extension->override_mask & OPTICAL_OVERRIDE_RAY_BLOCKS) != 0U &&
        extension->ray_blocks > 1U) return false;
    if ((extension->override_mask & OPTICAL_OVERRIDE_LIGHT_BLOCKS) != 0U &&
        extension->light_blocks > 1U) return false;
    return true;
}

static void apply_extension(OpticalResolved *resolved,
                            const OpticalExtension *extension) {
    if ((extension->override_mask & OPTICAL_OVERRIDE_PLAYER_BLOCKS) != 0U)
        resolved->player_blocks = extension->player_blocks != 0U;
    if ((extension->override_mask & OPTICAL_OVERRIDE_RAY_BLOCKS) != 0U)
        resolved->ray_blocks = extension->ray_blocks != 0U;
    if ((extension->override_mask & OPTICAL_OVERRIDE_LIGHT_BLOCKS) != 0U)
        resolved->light_blocks = extension->light_blocks != 0U;
    if ((extension->override_mask & OPTICAL_OVERRIDE_OPACITY) != 0U)
        resolved->opacity = extension->opacity;
    if ((extension->override_mask & OPTICAL_OVERRIDE_TRANSMISSION) != 0U)
        resolved->transmission = extension->transmission;
    if ((extension->override_mask & OPTICAL_OVERRIDE_REFLECTIVITY) != 0U)
        resolved->reflectivity = extension->reflectivity;
}

bool optical_runtime_view_init(
    OpticalRuntimeView *out_view,
    size_t cell_count,
    const OpticalExtension *material_defaults,
    size_t material_capacity,
    const OpticalCellOverride *cell_overrides,
    size_t cell_override_count,
    uint32_t source_generation
) {
    OpticalRuntimeView candidate;
    size_t i;
    if (!out_view || cell_count == 0U ||
        material_capacity > OPTICAL_MATERIAL_CAPACITY_MAX ||
        ((material_defaults == NULL) != (material_capacity == 0U)) ||
        ((cell_overrides == NULL) != (cell_override_count == 0U)) ||
        cell_override_count > cell_count) return false;
    for (i = 0U; i < material_capacity; i++)
        if (!optical_extension_is_valid(&material_defaults[i])) return false;
    for (i = 0U; i < cell_override_count; i++) {
        if ((size_t)cell_overrides[i].cell_index >= cell_count ||
            !optical_extension_is_valid(&cell_overrides[i].optical) ||
            (i > 0U && cell_overrides[i - 1U].cell_index >=
                        cell_overrides[i].cell_index)) return false;
    }
    candidate.material_defaults = material_defaults;
    candidate.material_capacity = material_capacity;
    candidate.cell_overrides = cell_overrides;
    candidate.cell_override_count = cell_override_count;
    candidate.cell_count = cell_count;
    candidate.source_generation = source_generation;
    candidate.valid = true;
    *out_view = candidate;
    return true;
}

bool optical_runtime_view_is_current(
    const OpticalRuntimeView *view, uint32_t source_generation
) {
    return view && view->valid && view->source_generation == source_generation;
}

static const OpticalExtension *find_cell_override(
    const OpticalRuntimeView *view, size_t cell_index
) {
    size_t low = 0U;
    size_t high = view->cell_override_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2U;
        size_t candidate = view->cell_overrides[middle].cell_index;
        if (candidate < cell_index) low = middle + 1U;
        else high = middle;
    }
    if (low < view->cell_override_count &&
        (size_t)view->cell_overrides[low].cell_index == cell_index)
        return &view->cell_overrides[low].optical;
    return NULL;
}

bool optical_runtime_view_resolve(
    const OpticalRuntimeView *view,
    size_t cell_index,
    uint16_t material_id,
    bool legacy_blocks,
    OpticalResolved *out_resolved
) {
    OpticalResolved resolved;
    const OpticalExtension *cell_override;
    if (!view || !view->valid || !out_resolved || cell_index >= view->cell_count)
        return false;
    resolved = optical_resolved_legacy(legacy_blocks);
    if ((size_t)material_id < view->material_capacity)
        apply_extension(&resolved, &view->material_defaults[material_id]);
    cell_override = find_cell_override(view, cell_index);
    if (cell_override) apply_extension(&resolved, cell_override);
    *out_resolved = resolved;
    return true;
}
