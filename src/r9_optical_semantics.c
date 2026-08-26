#include "r9_optical_semantics.h"

#ifdef R9_OPTICAL_RESEARCH

#include "checked_size.h"

#include <limits.h>

#define R9_MATERIAL_TEXT_RECORD_BYTES 22U
#define R9_SPARSE_CELL_TEXT_RECORD_BYTES 23U
#define R9_EXTENSION_TEXT_TOKEN_BYTES 16U

_Static_assert(sizeof(R9OpticalExtension) == 8U,
               "P1 memory findings depend on the compact extension size");
_Static_assert(sizeof(R9OpticalCellOverride) == 12U,
               "P1 memory findings depend on the sparse record size");

static bool checked_add(size_t left, size_t right, size_t *out_sum) {
    if (!out_sum || left > SIZE_MAX - right) return false;
    *out_sum = left + right;
    return true;
}

R9OpticalResolved r9_optical_legacy_semantics(bool occupied) {
    R9OpticalResolved resolved;
    resolved.player_blocks = occupied;
    resolved.ray_blocks = occupied;
    resolved.light_blocks = occupied;
    resolved.opacity = occupied ? UINT8_MAX : 0U;
    resolved.transmission = occupied ? 0U : UINT8_MAX;
    resolved.reflectivity = 0U;
    return resolved;
}

bool r9_optical_extension_is_valid(const R9OpticalExtension *extension) {
    if (!extension ||
        (extension->override_mask & (uint8_t)~R9_OPTICAL_OVERRIDE_ALL) != 0U ||
        extension->reserved != 0U) return false;
    if ((extension->override_mask & R9_OPTICAL_OVERRIDE_PLAYER_BLOCKS) != 0U &&
        extension->player_blocks > 1U) return false;
    if ((extension->override_mask & R9_OPTICAL_OVERRIDE_RAY_BLOCKS) != 0U &&
        extension->ray_blocks > 1U) return false;
    if ((extension->override_mask & R9_OPTICAL_OVERRIDE_LIGHT_BLOCKS) != 0U &&
        extension->light_blocks > 1U) return false;
    return true;
}

bool r9_optical_resolve(const R9OpticalResolved *legacy,
                        const R9OpticalExtension *extension,
                        R9OpticalResolved *out_resolved) {
    R9OpticalResolved resolved;
    if (!legacy || !out_resolved ||
        !r9_optical_extension_is_valid(extension)) return false;
    resolved = *legacy;
    if ((extension->override_mask & R9_OPTICAL_OVERRIDE_PLAYER_BLOCKS) != 0U)
        resolved.player_blocks = extension->player_blocks != 0U;
    if ((extension->override_mask & R9_OPTICAL_OVERRIDE_RAY_BLOCKS) != 0U)
        resolved.ray_blocks = extension->ray_blocks != 0U;
    if ((extension->override_mask & R9_OPTICAL_OVERRIDE_LIGHT_BLOCKS) != 0U)
        resolved.light_blocks = extension->light_blocks != 0U;
    if ((extension->override_mask & R9_OPTICAL_OVERRIDE_OPACITY) != 0U)
        resolved.opacity = extension->opacity;
    if ((extension->override_mask & R9_OPTICAL_OVERRIDE_TRANSMISSION) != 0U)
        resolved.transmission = extension->transmission;
    if ((extension->override_mask & R9_OPTICAL_OVERRIDE_REFLECTIVITY) != 0U)
        resolved.reflectivity = extension->reflectivity;
    *out_resolved = resolved;
    return true;
}

bool r9_optical_memory_report(int width, int height, size_t material_capacity,
                              size_t sparse_override_count,
                              R9OpticalMemoryReport *out_report) {
    R9OpticalMemoryReport report = {0};
    size_t cell_count;
    if (!out_report || material_capacity == 0U ||
        !checked_size_2d(width, height, &cell_count) ||
        sparse_override_count > cell_count ||
        !checked_size_bytes(material_capacity, sizeof(R9OpticalExtension),
                            &report.material_extension_bytes) ||
        !checked_size_bytes(cell_count, sizeof(R9OpticalExtension),
                            &report.dense_cell_extension_bytes)) return false;
    if (sparse_override_count > 0U) {
        if (!checked_size_bytes(sparse_override_count,
                                sizeof(R9OpticalCellOverride),
                                &report.sparse_cell_extension_bytes)) return false;
    }
    if (!checked_add(report.material_extension_bytes,
                     report.sparse_cell_extension_bytes,
                     &report.material_plus_sparse_bytes)) return false;
    if (!checked_size_bytes(material_capacity, R9_MATERIAL_TEXT_RECORD_BYTES,
                            &report.material_text_payload_bytes) ||
        !checked_size_bytes(cell_count, R9_EXTENSION_TEXT_TOKEN_BYTES + 1U,
                            &report.dense_cell_text_payload_bytes)) return false;
    if (sparse_override_count > 0U &&
        !checked_size_bytes(sparse_override_count,
                            R9_SPARSE_CELL_TEXT_RECORD_BYTES,
                            &report.sparse_cell_text_payload_bytes)) return false;
    if (!checked_add(report.material_text_payload_bytes,
                     report.sparse_cell_text_payload_bytes,
                     &report.material_plus_sparse_text_payload_bytes)) return false;
    report.cell_count = cell_count;
    report.material_capacity = material_capacity;
    report.extension_bytes = sizeof(R9OpticalExtension);
    report.sparse_record_bytes = sizeof(R9OpticalCellOverride);
    report.dense_break_even_sparse_count =
        report.dense_cell_extension_bytes / sizeof(R9OpticalCellOverride);
    if (report.dense_cell_extension_bytes > report.material_extension_bytes) {
        report.material_plus_sparse_break_even_count =
            (report.dense_cell_extension_bytes - report.material_extension_bytes) /
            sizeof(R9OpticalCellOverride);
    }
    *out_report = report;
    return true;
}

#else

typedef int r9_optical_semantics_disabled_translation_unit;

#endif /* R9_OPTICAL_RESEARCH */
