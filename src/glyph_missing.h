/**
 * glyph_missing.h — "exactly one diagnostic per (face, codepoint)", made exact
 *
 * D8 requires a missing glyph to raise one diagnostic per (face, codepoint) and to be visible in the
 * cell. The visibility is glyph_paint's job; this is the bookkeeping, and it is deliberately exact
 * rather than approximate: a 65,536-bit bitmap per source records what has already been reported, so
 * there is no eviction, no "recently seen" list that can overflow, and no re-reporting after a frame
 * boundary. 8 KB against a 1 MB glyph budget is cheaper than a rule nobody can state precisely.
 *
 * The bitmap belongs to a **face**, not a process: when the source's set changes — its provenance
 * digest, or the built-in face — the bitmap is cleared, because a different face is a different
 * subject and its coverage gaps are its own.
 *
 * Plain values only: no heap, no global mutable state, no formatted strings. The caller owns the
 * struct and decides when a frame begins; the draw sites note misses; the application boundary prints
 * them (C_STYLE_AND_OWNERSHIP.md: log once, at the boundary that owns the consequence).
 */

#ifndef GLYPH_MISSING_H
#define GLYPH_MISSING_H

#include "glyph_source.h"
#include "sha256.h"

#include <stdbool.h>
#include <stdint.h>

/* The cell addresses 16 bits, so this is every codepoint a miss can name. */
#define GLYPH_MISSING_CODEPOINT_COUNT 65536U
#define GLYPH_MISSING_BITMAP_BYTES (GLYPH_MISSING_CODEPOINT_COUNT / 8U)

/* How many *newly distinct* misses one frame names. Beyond this the frame reports a count instead,
   the same bounded-reporting rule the converter uses for refused glyphs. */
#define GLYPH_MISSING_MAX_REPORTS 8U

typedef struct {
    uint32_t codepoint;
    int cell_x;
    int cell_y;
} GlyphMissingReport;

typedef struct {
    uint8_t reported[GLYPH_MISSING_BITMAP_BYTES];
    /* Identity of the face the bitmap describes: the set's provenance, or all-zero for the built-in
       face. A change clears the bitmap. */
    uint8_t face_digest[SHA256_DIGEST_SIZE];
    bool has_face;
    uint32_t affected_this_frame;
    uint32_t overflow_this_frame;
    unsigned report_count;
    GlyphMissingReport reports[GLYPH_MISSING_MAX_REPORTS];
} GlyphMissingState;

/* Zero everything, including the face identity: the built-in face, nothing reported.
 *
 * **Call this before first use.** A GlyphMissingState that has not been reset is not a valid input —
 * its `has_face` flag is indeterminate, and reading an indeterminate _Bool is undefined behaviour
 * (UBSan found exactly that in this module's first test). Embedded owners such as Renderer call it
 * during construction. */
void glyph_missing_reset(GlyphMissingState *state);

/* Start a frame: clear the per-frame counters and the newly-distinct list. The reported bitmap
   survives, because its whole purpose is to outlive frames. */
void glyph_missing_begin_frame(GlyphMissingState *state);

/* Record one missing cell. `source` names the face that failed to answer; `cell_x`/`cell_y` are the
   first offending coordinate the caller has. Calling this again for the same (face, codepoint) in the
   same or a later frame counts as affected but does not report it again. */
void glyph_missing_note(GlyphMissingState *state, const GlyphSource *source, uint32_t codepoint,
                        int cell_x, int cell_y);

/* How many cells this frame resolved to a missing glyph, including repeats of an already-reported
   codepoint. */
uint32_t glyph_missing_affected_this_frame(const GlyphMissingState *state);

/* Distinct codepoints observed this frame beyond GLYPH_MISSING_MAX_REPORTS, named as a count rather
   than silently dropped. */
uint32_t glyph_missing_overflow_this_frame(const GlyphMissingState *state);

/* The newly-distinct misses this frame, in the order they were first seen. */
unsigned glyph_missing_report_count(const GlyphMissingState *state);
bool glyph_missing_report(const GlyphMissingState *state, unsigned index, GlyphMissingReport *out);

/* "builtin" when the face is the compiled-in one, else the provenance digest in short hex form.
   Written into `out`, which must hold GLYPH_MISSING_FACE_TEXT_SIZE bytes. */
#define GLYPH_MISSING_FACE_TEXT_SIZE 17U /* 16 hex digits plus NUL */
void glyph_missing_face_text(const GlyphMissingState *state, char out[GLYPH_MISSING_FACE_TEXT_SIZE]);

#endif /* GLYPH_MISSING_H */
