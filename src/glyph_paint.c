/**
 * glyph_paint.c — the missing-glyph colour rule, in one place
 *
 * Alpha is forced opaque for the sentinel. A half-transparent diagnostic is worse than none: the
 * point is that the cell cannot be mistaken for art, and compositing it over whatever is behind
 * would soften exactly the clash that makes it noticeable.
 */

#include "glyph_paint.h"

void glyph_paint_colors(const GlyphSourceGlyph *glyph, SDL_Color *fg, SDL_Color *bg) {
    if (!glyph || !glyph_source_is_missing(glyph)) return;
    if (fg) {
        fg->r = GLYPH_SOURCE_SENTINEL_FG_R;
        fg->g = GLYPH_SOURCE_SENTINEL_FG_G;
        fg->b = GLYPH_SOURCE_SENTINEL_FG_B;
        fg->a = 0xFFU;
    }
    if (bg) {
        bg->r = GLYPH_SOURCE_SENTINEL_BG_R;
        bg->g = GLYPH_SOURCE_SENTINEL_BG_G;
        bg->b = GLYPH_SOURCE_SENTINEL_BG_B;
        bg->a = 0xFFU;
    }
}

GlyphSourceGlyph glyph_paint_resolve(const GlyphSource *source, uint32_t codepoint,
                                     SDL_Color *fg, SDL_Color *bg) {
    GlyphSourceGlyph glyph = glyph_source_resolve(source, codepoint);
    glyph_paint_colors(&glyph, fg, bg);
    return glyph;
}
