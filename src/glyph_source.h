/**
 * glyph_source.h — one answer to "what draws this codepoint?"
 *
 * Three things can answer for a codepoint: the converted set the user supplied, the built-in
 * 8×8 ASCII face, or nothing. Putting that decision in one place is what keeps the missing-glyph
 * rule (V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md, D8) from being re-implemented — and
 * re-interpreted — at every draw site.
 *
 * Precedence: **the set answers first; the built-in face answers only when the source declares no set
 * at all.** Those are two different situations and conflating them was a defect: a source with no set
 * is a launch with no conversion installed (and the application chrome, which declares the built-in
 * face permanently), so the built-in face is not a fallback there — it is the declared face. A source
 * that HAS a set and lacks the codepoint is a coverage gap, and D8 says a gap is a diagnostic, not a
 * quiet borrow from another face (V1_5 plan, D-C).
 *
 * Missing is a result, not an exception: the caller gets the sentinel's pixels and a `kind` that
 * says they are a sentinel, so the cell can be painted conspicuously (magenta on red) rather than
 * looking like a legitimate glyph that happens to be an X. The colours live here as named
 * constants for the same reason the decision does — one definition, quoted from the palette.
 *
 * Derived, never stored: the sentinel is the built-in 'X' read at call time, so it cannot be
 * authored, saved, or smuggled into a document (D8).
 */

#ifndef GLYPH_SOURCE_H
#define GLYPH_SOURCE_H

#include "glyph_set.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GLYPH_SOURCE_CELL_SIZE 8U
/* The sentinel is an ASCII codepoint so it can come from the built-in face: no glyph art, no
   stored bytes, nothing to keep in sync. */
#define GLYPH_SOURCE_SENTINEL_CODEPOINT 0x0058U /* 'X' */
/* Palette values, not new hues: magenta #FF2BD6 and red #FF2B2B (src/ui_theme.c). Painting a
   diagnostic mark is deliberately outside the look identity, which is why it may use them. */
#define GLYPH_SOURCE_SENTINEL_FG_R 0xFFU
#define GLYPH_SOURCE_SENTINEL_FG_G 0x2BU
#define GLYPH_SOURCE_SENTINEL_FG_B 0xD6U
#define GLYPH_SOURCE_SENTINEL_BG_R 0xFFU
#define GLYPH_SOURCE_SENTINEL_BG_G 0x2BU
#define GLYPH_SOURCE_SENTINEL_BG_B 0x2BU

typedef enum {
    /* A converted set supplied these pixels. */
    GLYPH_SOURCE_SET = 0,
    /* The built-in ASCII face did; codepoint < 0x80. */
    GLYPH_SOURCE_BUILTIN,
    /* Nothing had it: `rows` holds the sentinel and the caller must paint it conspicuously. */
    GLYPH_SOURCE_MISSING
} GlyphSourceKind;

typedef struct {
    uint8_t rows[GLYPH_SOURCE_CELL_SIZE]; /* bit 0 = leftmost, the renderer's convention */
    uint32_t codepoint;                   /* what was asked for, for diagnostics */
    GlyphSourceKind kind;
    /* Which fit mode produced a set glyph; 0 (none) for built-in and sentinel results. */
    uint8_t mode;
} GlyphSourceGlyph;

typedef struct {
    /* Borrowed, may be NULL: no set loaded is the ordinary state before a conversion is shipped,
       and it must behave like "the built-in face only", not like an error. */
    const GlyphSet *set;
} GlyphSource;

/* Total: a NULL set means built-in only. */
void glyph_source_init(GlyphSource *source, const GlyphSet *set);

/* Resolve one codepoint. Never fails: an unknown codepoint is a MISSING answer carrying the
   sentinel, because a draw path that has to handle an error per cell is a draw path that will
   eventually ignore one. */
GlyphSourceGlyph glyph_source_resolve(const GlyphSource *source, uint32_t codepoint);

/* True when `glyph` is the sentinel rather than a real glyph, so a caller can log or count it
   without re-deriving the rule. */
bool glyph_source_is_missing(const GlyphSourceGlyph *glyph);

/* True when `set` contains every printable ASCII codepoint, U+0020 through U+007E.
 *
 * This is the test behind the chrome's face policy (V1-6 plan, D-A): the application's own interface
 * may use an installed face **only if that face can draw the interface**. It asks the bytes rather than
 * the manifest's `coverage=` line, because a declaration can be wrong and a lookup cannot. */
bool glyph_source_set_covers_printable_ascii(const GlyphSet *set);

/* The face the application chrome may draw with, given the face the application installed, or NULL for
 * the built-in ASCII face. This *is* the chrome's policy (V1-6 plan, D-A) and lives here rather than in
 * the renderer so it can be tested without a window: a face that cannot draw the interface is not
 * adopted, however well it draws the world. */
const GlyphSet *glyph_source_ui_face(const GlyphSet *installed);

/* Stable short name of a kind, for tool output and test failure messages. */
const char *glyph_source_kind_name(GlyphSourceKind kind);

#endif /* GLYPH_SOURCE_H */
