/**
 * glyph_atlas.c — SDL texture atlas for bitmap glyph rendering
 *
 * This file implements a simple glyph atlas: a single SDL_Texture that packs
 * all 128 ASCII characters (U+0000–U+007F) side-by-side in a row.  Each glyph
 * is an 8×8 monochrome bitmap sourced from the embedded font8x8_basic[] array.
 *
 * The atlas is prepared as a potential GPU-side glyph sprite sheet.  The
 * current renderer_draw() path uses the raw font8x8_basic bits directly in a
 * CPU pixel buffer instead of sampling this texture.
 *
 * Layout:
 *   - Glyph size:  8 × 8 pixels
 *   - Atlas width:  128 × 8 = 1024 pixels  (one glyph per ASCII code)
 *   - Atlas height: 8 pixels                (single row)
 *
 * Memory / texture tracking:
 *   This file participates in the engine's per-frame allocation detection by
 *   incrementing three global counters declared in renderer.h:
 *     renderer_alloc_count         — each malloc()
 *     renderer_free_count          — each free()
 *     renderer_texture_create_count — each SDL_CreateTextureFromSurface()
 */

#include "glyph_atlas.h"   /* GlyphAtlas struct, glyph_atlas_create_builtin(),
                              glyph_atlas_destroy() */
#include "font8x8.h"       /* font8x8_basic[128][8] — the embedded 8×8 bitmap font */
#include <stdlib.h>         /* malloc(), free() */

/* ===================================================================
 *  Instrumentation counters (defined in renderer.c)
 * =================================================================== */

extern int renderer_alloc_count;          /* Tracks number of malloc() calls */
extern int renderer_free_count;           /* Tracks number of free() calls */
extern int renderer_texture_create_count; /* Tracks number of SDL textures created */

/* ===================================================================
 *  Atlas creation
 * =================================================================== */

/**
 * glyph_atlas_create_builtin() — Build a glyph atlas texture from the built-in font
 *
 * This function:
 *   1. Allocates a GlyphAtlas struct (tracked by renderer_alloc_count)
 *   2. Creates an SDL_Surface large enough to hold 128 8×8 glyphs (1024×8 pixels)
 *   3. Fills the surface with transparent pixels (RGBA = 0,0,0,0)
 *   4. Iterates over font8x8_basic and for each character, for each of its 8 rows,
 *      sets pixels to white (0xFFFFFFFF) wherever the bitmap has a '1' bit
 *   5. Converts the surface to an SDL_Texture with blending enabled
 *   6. Cleans up the temporary surface and returns the atlas
 *
 * Only character codes 0–127 are supported (basic Latin).  Codes beyond that
 * render as whitespace.
 *
 * @param sdl_ren  The SDL_Renderer to create the texture with (must not be NULL)
 * @return         Pointer to a new GlyphAtlas, or NULL on failure
 */
GlyphAtlas* glyph_atlas_create_builtin(SDL_Renderer *sdl_ren) {
    if (!sdl_ren) return NULL;

    /* --- Allocate the atlas struct on the heap --- */
    GlyphAtlas *atlas = malloc(sizeof(GlyphAtlas));
    if (!atlas) return NULL;
    renderer_alloc_count++;              /* Track this allocation */

    /* Each glyph is an 8×8 pixel block */
    atlas->glyph_width  = 8;
    atlas->glyph_height = 8;

    /* The atlas texture is a single horizontal strip: 128 glyphs wide × 1 glyph tall.
     * Total pixel dimensions: 1024 × 8. */
    int atlas_w = 128 * 8;               /* 1024 pixels wide */
    int atlas_h = 8;                     /* 8 pixels tall */

    /* --- Create a temporary surface in RGBA32 format --- */
    SDL_Surface *surface = SDL_CreateSurface(atlas_w, atlas_h, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        free(atlas);
        renderer_free_count++;
        return NULL;
    }

    /* Fill the entire surface with transparent black (R=0, G=0, B=0, A=0).
     * This means any pixel NOT written by the font bitmap loop below will
     * remain fully transparent — the background will show through. */
    SDL_FillSurfaceRect(surface, NULL,
        SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format), NULL, 0, 0, 0, 0));

    /* Get a pointer to the raw pixel data.
     * pitch is the number of bytes per row; dividing by 4 gives pixels per row
     * (since each pixel is 4 bytes in RGBA32 format). */
    uint32_t *pixels = (uint32_t*)surface->pixels;
    int pitch = surface->pitch / 4;

    /* ---- Rasterise each of the 128 characters into the atlas ---- */
    for (int c = 0; c < 128; c++) {          /* For each character code 0–127 */
        for (int y = 0; y < 8; y++) {        /* For each of the 8 pixel rows */
            uint8_t row = font8x8_basic[c][y]; /* Bitmask for this row (8 bits) */

            for (int x = 0; x < 8; x++) {    /* For each column in the row */
                /* The current renderer interprets bit 0 as output pixel 0,
                 * bit 1 as output pixel 1, and so on.  This matches the
                 * software rasterizer path in renderer_draw(). */
                if (row & (1 << x)) {
                    /* Bit is set → this pixel is part of the glyph.
                     * Write it as fully opaque white (0xFFFFFFFF = RGBA: 255,255,255,255).
                     * The renderer will tint this white source pixel to any
                     * colour when drawing via SDL_SetTextureColorMod. */
                    pixels[y * pitch + (c * 8) + x] = 0xFFFFFFFF;
                }
                /* Bit is clear → pixel remains transparent (as set by the fill above) */
            }
        }
    }

    /* --- Convert the surface to an SDL_Texture (GPU upload) --- */
    atlas->texture = SDL_CreateTextureFromSurface(sdl_ren, surface);
    SDL_DestroySurface(surface);             /* Surface is no longer needed */

    if (!atlas->texture) {
        /* Texture creation failed — clean up the atlas struct */
        free(atlas);
        renderer_free_count++;
        return NULL;
    }

    renderer_texture_create_count++;         /* Track this texture creation */

    /* Enable alpha blending so transparent pixels let the background show through.
     * This is crucial: without it, glyph backgrounds would be black squares. */
    SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);

    return atlas;
}

/* ===================================================================
 *  Atlas destruction
 * =================================================================== */

/**
 * glyph_atlas_destroy() — Free all resources held by a GlyphAtlas
 *
 * Destroys the SDL_Texture and frees the atlas struct itself.
 * Safe to call with NULL (no-op).
 *
 * @param atlas  Pointer to the GlyphAtlas to destroy (NULL-safe)
 */
void glyph_atlas_destroy(GlyphAtlas *atlas) {
    if (!atlas) return;
    if (atlas->texture) {
        SDL_DestroyTexture(atlas->texture);  /* Free GPU-side texture memory */
    }
    free(atlas);                              /* Free the heap-allocated struct */
    renderer_free_count++;                    /* Track this free */
}