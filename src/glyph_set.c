/**
 * glyph_set.c — encode, validate, look up
 *
 * Validation is the load-bearing half. A set arrives as a file from a build step, possibly copied
 * from another machine, possibly stale, possibly truncated by a full disk. Every one of those has
 * a name and a message here, because the alternative is a renderer reading entry boundaries off
 * the end of a buffer.
 */

#include "glyph_set.h"

#include <string.h>

static const uint8_t GLYPH_SET_MAGIC[GLYPH_SET_MAGIC_SIZE] = {'T', 'S', 'G', 'F',
                                                             'O', 'N', 'T', '1'};

static void put_u32(uint8_t *buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value & 0xFFU);
    buffer[1] = (uint8_t)((value >> 8U) & 0xFFU);
    buffer[2] = (uint8_t)((value >> 16U) & 0xFFU);
    buffer[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static uint32_t read_u32(const uint8_t *buffer) {
    return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8U) | ((uint32_t)buffer[2] << 16U) |
           ((uint32_t)buffer[3] << 24U);
}

bool glyph_set_codepoint_is_addressable(uint32_t codepoint) {
    return codepoint <= GLYPH_SET_MAX_CODEPOINT;
}

size_t glyph_set_encoded_size(unsigned count) {
    if (count > GLYPH_SET_MAX_GLYPHS) return 0U;
    return (size_t)GLYPH_SET_HEADER_SIZE + (size_t)count * GLYPH_SET_ENTRY_SIZE;
}

bool glyph_set_write(const GlyphSetEntry *entries, unsigned count,
                     const uint8_t provenance[SHA256_DIGEST_SIZE], uint8_t *buffer,
                     size_t buffer_size, size_t *out_length) {
    size_t needed;
    unsigned index;
    /* Modes are 0..3 today; a wider value would be a reader's problem, so it is a writer's
       rejection: the producer must not emit what its own reader refuses. */
    static const uint8_t MAX_MODE = 3U;
    if (!provenance || !buffer) return false;
    if (count > 0U && !entries) return false;
    needed = glyph_set_encoded_size(count);
    if (needed == 0U || needed > buffer_size) return false;
    memset(buffer, 0, GLYPH_SET_HEADER_SIZE);
    memcpy(buffer, GLYPH_SET_MAGIC, GLYPH_SET_MAGIC_SIZE);
    put_u32(buffer + 8U, GLYPH_SET_CELL_SIZE);
    put_u32(buffer + 12U, (uint32_t)count);
    memcpy(buffer + 16U, provenance, SHA256_DIGEST_SIZE);
    for (index = 0U; index < count; index++) {
        uint8_t *entry = buffer + GLYPH_SET_HEADER_SIZE + (size_t)index * GLYPH_SET_ENTRY_SIZE;
        if (entries[index].mode > MAX_MODE) return false;
        /* Emitting an unaddressable codepoint would produce a file this module itself rejects. */
        if (!glyph_set_codepoint_is_addressable(entries[index].codepoint)) return false;
        put_u32(entry, entries[index].codepoint);
        entry[4] = entries[index].mode;
        memcpy(entry + 5U, entries[index].rows, GLYPH_SET_CELL_SIZE);
    }
    if (out_length) *out_length = needed;
    return true;
}

const char *glyph_set_result_name(GlyphSetResult result) {
    switch (result) {
    case GLYPH_SET_OK: return "OK";
    case GLYPH_SET_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case GLYPH_SET_TOO_SHORT: return "TOO_SHORT";
    case GLYPH_SET_BAD_MAGIC: return "BAD_MAGIC";
    case GLYPH_SET_UNSUPPORTED_CELL: return "UNSUPPORTED_CELL";
    case GLYPH_SET_TRUNCATED: return "TRUNCATED";
    case GLYPH_SET_TOO_MANY_GLYPHS: return "TOO_MANY_GLYPHS";
    case GLYPH_SET_UNSORTED: return "UNSORTED";
    case GLYPH_SET_DUPLICATE_CODEPOINT: return "DUPLICATE_CODEPOINT";
    case GLYPH_SET_BAD_MODE: return "BAD_MODE";
    case GLYPH_SET_CODEPOINT_TOO_WIDE: return "CODEPOINT_TOO_WIDE";
    case GLYPH_SET_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
    default: return "UNKNOWN_RESULT";
    }
}

static GlyphSetResult reject(GlyphSetError *error, GlyphSetResult result, unsigned index,
                             uint32_t codepoint) {
    if (error) {
        error->result = result;
        error->index = index;
        error->codepoint = codepoint;
    }
    return result;
}

GlyphSetResult glyph_set_from_bytes(const uint8_t *data, size_t size, GlyphSet *out_set,
                                    GlyphSetError *out_error) {
    GlyphSet set;
    uint32_t cell;
    uint32_t count;
    size_t expected;
    unsigned index;
    uint32_t previous = 0U;

    if (out_error) {
        out_error->result = GLYPH_SET_OK;
        out_error->index = 0U;
        out_error->codepoint = 0U;
    }
    if (!data || !out_set) return reject(out_error, GLYPH_SET_INVALID_ARGUMENT, 0U, 0U);
    if (size < GLYPH_SET_HEADER_SIZE) return reject(out_error, GLYPH_SET_TOO_SHORT, 0U, 0U);
    if (memcmp(data, GLYPH_SET_MAGIC, GLYPH_SET_MAGIC_SIZE) != 0) {
        return reject(out_error, GLYPH_SET_BAD_MAGIC, 0U, 0U);
    }
    cell = read_u32(data + 8U);
    if (cell != GLYPH_SET_CELL_SIZE) {
        return reject(out_error, GLYPH_SET_UNSUPPORTED_CELL, 0U, 0U);
    }
    count = read_u32(data + 12U);
    if (count > GLYPH_SET_MAX_GLYPHS) {
        return reject(out_error, GLYPH_SET_TOO_MANY_GLYPHS, 0U, 0U);
    }
    /* Exact length, not "at least": a file with trailing bytes is a different file, and accepting
       it would let two sets with different bytes pass for the same set. */
    expected = glyph_set_encoded_size(count);
    if (size != expected) return reject(out_error, GLYPH_SET_TRUNCATED, 0U, 0U);

    for (index = 0U; index < count; index++) {
        const uint8_t *entry = data + GLYPH_SET_HEADER_SIZE + (size_t)index * GLYPH_SET_ENTRY_SIZE;
        uint32_t codepoint = read_u32(entry);
        if (entry[4] > 3U) return reject(out_error, GLYPH_SET_BAD_MODE, index, codepoint);
        /* A set cannot have been produced by this pipeline and contain one of these: the converter
           refuses to write them. So it is corruption or a stale format, and saying which entry is
           the difference between a five-minute fix and an afternoon. */
        if (!glyph_set_codepoint_is_addressable(codepoint)) {
            return reject(out_error, GLYPH_SET_CODEPOINT_TOO_WIDE, index, codepoint);
        }
        if (index > 0U) {
            /* Ascending and unique is what makes lookup a binary search and what makes the writer
               deterministic; a set that violates it was not produced by this pipeline. */
            if (codepoint == previous) {
                return reject(out_error, GLYPH_SET_DUPLICATE_CODEPOINT, index, codepoint);
            }
            if (codepoint < previous) return reject(out_error, GLYPH_SET_UNSORTED, index, codepoint);
        }
        previous = codepoint;
    }

    set.data = data;
    set.size = size;
    set.glyph_count = count;
    memcpy(set.provenance, data + 16U, SHA256_DIGEST_SIZE);
    *out_set = set;
    return GLYPH_SET_OK;
}

bool glyph_set_lookup(const GlyphSet *set, uint32_t codepoint, GlyphSetEntry *out_entry) {
    unsigned low;
    unsigned high;
    if (!set || !set->data || set->glyph_count == 0U) return false;
    low = 0U;
    high = set->glyph_count;
    while (low < high) {
        unsigned middle = low + (high - low) / 2U;
        const uint8_t *entry =
            set->data + GLYPH_SET_HEADER_SIZE + (size_t)middle * GLYPH_SET_ENTRY_SIZE;
        uint32_t found = read_u32(entry);
        if (found == codepoint) {
            if (out_entry) {
                out_entry->codepoint = found;
                out_entry->mode = entry[4];
                memcpy(out_entry->rows, entry + 5U, GLYPH_SET_CELL_SIZE);
            }
            return true;
        }
        if (found < codepoint) {
            low = middle + 1U;
        } else {
            high = middle;
        }
    }
    return false;
}

