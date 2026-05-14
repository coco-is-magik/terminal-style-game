/**
 * glyph_atlas.h — SDL texture atlas for bitmap glyph rendering
 *
 * Defines the GlyphAtlas struct and its create/destroy functions.
 * The atlas is a single SDL_Texture containing all 128 ASCII characters
 * (U+0000–U+007F) as 8×8 monochrome bitmaps, laid out in a horizontal
 * strip (1024 × 8 pixels).
 *
 * See glyph_atlas.c for the implementation.
 */

#ifndef GLYPH_ATLAS_H
#define GLYPH_ATLAS_H

#include <SDL3/SDL.h>     /* SDL_Texture, SDL_Renderer */

/**
 * GlyphAtlas — A packed texture containing all glyph bitmaps
 *
 * texture      — The SDL texture containing the rasterised glyph strip
 * glyph_width  — Width of a single glyph in pixels (always 8)
 * glyph_height — Height of a single glyph in pixels (always 8)
 */
typedef struct {
    SDL_Texture *texture;       /* GPU texture with all 128 glyphs */
    int glyph_width;            /* Width of one glyph (pixels) */
    int glyph_height;           /* Height of one glyph (pixels) */
} GlyphAtlas;

/**
 * glyph_atlas_create_builtin() — Build a glyph atlas from the built-in 8×8 font
 *
 * Creates a 1024×8 RGBA32 texture containing the 128 ASCII characters
 * from font8x8_basic[].  The current software renderer maps higher glyph
 * codes to space when drawing from the raw font data.
 *
 * @param sdl_ren  SDL_Renderer to create the texture with
 * @return         New GlyphAtlas, or NULL on failure
 */
GlyphAtlas* glyph_atlas_create_builtin(SDL_Renderer *sdl_ren);

/**
 * glyph_atlas_destroy() — Free a glyph atlas
 *
 * Destroys the SDL_Texture and frees the struct.
 * Safe to call with NULL.
 *
 * @param atlas  GlyphAtlas to destroy (NULL-safe)
 */
void glyph_atlas_destroy(GlyphAtlas *atlas);

#endif /* GLYPH_ATLAS_H */