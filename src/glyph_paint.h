/**
 * glyph_paint.h — what colours a resolved glyph is drawn with
 *
 * Three draw sites need the same answer to "given this cell's colours and this resolved glyph, what
 * do I actually paint?" — the software rasteriser, the compositor, and the block cache. Putting it
 * here means the missing-glyph rule (V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md, D8) has one
 * implementation instead of three that drift.
 *
 * The rule is deliberately blunt: a missing glyph takes over the cell's colours entirely — magenta
 * foreground on red background — because a diagnostic that a theme or an author can restyle is a
 * diagnostic that can be hidden. Everything else keeps the cell's own colours untouched.
 */

#ifndef GLYPH_PAINT_H
#define GLYPH_PAINT_H

#include "glyph_source.h"

#include <SDL3/SDL.h>

/**
 * Compute the colours to paint a cell with.
 *
 * `fg`/`bg` are the cell's requested colours on input and the colours to paint on output. For a
 * missing glyph they are replaced wholesale with the sentinel palette; for anything else they are
 * left exactly as given, because the caller owns its own look.
 */
void glyph_paint_colors(const GlyphSourceGlyph *glyph, SDL_Color *fg, SDL_Color *bg);

/**
 * Resolve a cell's codepoint against **the source this surface declared** and fix up its colours in
 * one call.
 *
 * There is no process-wide selection: the caller owns the face decision, which is what lets the world
 * grid and the application chrome answer from different faces without either borrowing the other's.
 * `source` may be NULL, which means "the built-in ASCII face only".
 *
 * This is the whole per-cell decision a draw site needs: which face answers, what bits to paint,
 * and whether the cell has become a diagnostic. Returning the glyph by value keeps the caller from
 * holding a pointer into a source it does not own.
 */
GlyphSourceGlyph glyph_paint_resolve(const GlyphSource *source, uint32_t codepoint,
                                     SDL_Color *fg, SDL_Color *bg);

#endif /* GLYPH_PAINT_H */
