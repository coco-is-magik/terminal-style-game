/**
 * glyph_missing.c — see glyph_missing.h for why the bitmap is exact.
 */

#include "glyph_missing.h"

#include <stddef.h>
#include <string.h>

/* Adopt the source's face identity, clearing what was reported if it is a different face. A face is
   identified by its set's provenance digest, which changes exactly when the converted bytes do. */
static void adopt_face(GlyphMissingState *state, const GlyphSource *source) {
    bool has_face = source && source->set;
    uint8_t digest[SHA256_DIGEST_SIZE];

    memset(digest, 0, sizeof(digest));
    if (has_face) memcpy(digest, source->set->provenance, sizeof(digest));

    if (state->has_face == has_face && memcmp(state->face_digest, digest, sizeof(digest)) == 0) return;

    memset(state->reported, 0, sizeof(state->reported));
    memcpy(state->face_digest, digest, sizeof(digest));
    state->has_face = has_face;
}

void glyph_missing_reset(GlyphMissingState *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

void glyph_missing_begin_frame(GlyphMissingState *state) {
    if (!state) return;
    state->affected_this_frame = 0U;
    state->overflow_this_frame = 0U;
    state->report_count = 0U;
}

void glyph_missing_note(GlyphMissingState *state, const GlyphSource *source, uint32_t codepoint,
                        int cell_x, int cell_y) {
    size_t byte;
    uint8_t mask;

    if (!state) return;
    /* A cell cannot carry anything above 16 bits, so an out-of-range codepoint is a caller defect
       rather than a coverage gap: counting it would corrupt the meaning of the count. */
    if (codepoint >= GLYPH_MISSING_CODEPOINT_COUNT) return;

    adopt_face(state, source);
    state->affected_this_frame++;

    byte = (size_t)codepoint / 8U;
    mask = (uint8_t)(1U << (codepoint % 8U));
    if ((state->reported[byte] & mask) != 0U) return;
    state->reported[byte] |= mask;

    if (state->report_count < GLYPH_MISSING_MAX_REPORTS) {
        GlyphMissingReport *report = &state->reports[state->report_count];
        report->codepoint = codepoint;
        report->cell_x = cell_x;
        report->cell_y = cell_y;
        state->report_count++;
    } else {
        state->overflow_this_frame++;
    }
}

uint32_t glyph_missing_affected_this_frame(const GlyphMissingState *state) {
    return state ? state->affected_this_frame : 0U;
}

uint32_t glyph_missing_overflow_this_frame(const GlyphMissingState *state) {
    return state ? state->overflow_this_frame : 0U;
}

unsigned glyph_missing_report_count(const GlyphMissingState *state) {
    return state ? state->report_count : 0U;
}

bool glyph_missing_report(const GlyphMissingState *state, unsigned index, GlyphMissingReport *out) {
    if (!state || !out || index >= state->report_count) return false;
    *out = state->reports[index];
    return true;
}

void glyph_missing_face_text(const GlyphMissingState *state, char out[GLYPH_MISSING_FACE_TEXT_SIZE]) {
    static const char digits[] = "0123456789abcdef";
    unsigned i;

    if (!out) return;
    if (!state || !state->has_face) {
        memcpy(out, "builtin", 8U);
        return;
    }
    /* Eight bytes of the provenance is plenty to tell two installed faces apart in a log line, and
       keeps the diagnostic short enough to read. */
    for (i = 0U; i < 8U; i++) {
        out[i * 2U] = digits[(state->face_digest[i] >> 4) & 0x0FU];
        out[(i * 2U) + 1U] = digits[state->face_digest[i] & 0x0FU];
    }
    out[16] = '\0';
}
