/**
 * r9_optical_semantics.h — Compile-time-gated R9 P1 semantics research
 *
 * This module is a headless research boundary. It is excluded unless
 * R9_OPTICAL_RESEARCH=1 is defined and does not modify shipping authored data.
 */
#ifndef R9_OPTICAL_SEMANTICS_H
#define R9_OPTICAL_SEMANTICS_H

#ifdef R9_OPTICAL_RESEARCH

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define R9_OPTICAL_OVERRIDE_PLAYER_BLOCKS UINT8_C(0x01)
#define R9_OPTICAL_OVERRIDE_RAY_BLOCKS UINT8_C(0x02)
#define R9_OPTICAL_OVERRIDE_LIGHT_BLOCKS UINT8_C(0x04)
#define R9_OPTICAL_OVERRIDE_OPACITY UINT8_C(0x08)
#define R9_OPTICAL_OVERRIDE_TRANSMISSION UINT8_C(0x10)
#define R9_OPTICAL_OVERRIDE_REFLECTIVITY UINT8_C(0x20)
#define R9_OPTICAL_OVERRIDE_ALL UINT8_C(0x3F)

/** Candidate compact extension. A zero mask inherits all legacy semantics. */
typedef struct {
    uint8_t override_mask;
    uint8_t player_blocks;
    uint8_t ray_blocks;
    uint8_t light_blocks;
    uint8_t opacity;
    uint8_t transmission;
    uint8_t reflectivity;
    uint8_t reserved;
} R9OpticalExtension;

/** Fully resolved semantics consumed by a future renderer/physics boundary. */
typedef struct {
    bool player_blocks;
    bool ray_blocks;
    bool light_blocks;
    uint8_t opacity;
    uint8_t transmission;
    uint8_t reflectivity;
} R9OpticalResolved;

/** Candidate sparse cell override record. */
typedef struct {
    uint32_t cell_index;
    R9OpticalExtension optical;
} R9OpticalCellOverride;

typedef struct {
    size_t cell_count;
    size_t material_capacity;
    size_t extension_bytes;
    size_t sparse_record_bytes;
    size_t material_extension_bytes;
    size_t dense_cell_extension_bytes;
    size_t sparse_cell_extension_bytes;
    size_t material_plus_sparse_bytes;
    size_t dense_break_even_sparse_count;
    size_t material_plus_sparse_break_even_count;
    size_t material_text_payload_bytes;
    size_t dense_cell_text_payload_bytes;
    size_t sparse_cell_text_payload_bytes;
    size_t material_plus_sparse_text_payload_bytes;
} R9OpticalMemoryReport;

R9OpticalResolved r9_optical_legacy_semantics(bool occupied);
bool r9_optical_extension_is_valid(const R9OpticalExtension *extension);
bool r9_optical_resolve(const R9OpticalResolved *legacy,
                        const R9OpticalExtension *extension,
                        R9OpticalResolved *out_resolved);
bool r9_optical_memory_report(int width, int height, size_t material_capacity,
                              size_t sparse_override_count,
                              R9OpticalMemoryReport *out_report);

#endif /* R9_OPTICAL_RESEARCH */

#endif /* R9_OPTICAL_SEMANTICS_H */
