#include "glyph_atlas.h"
#include "font8x8.h"
#include <stdlib.h>

extern int renderer_alloc_count;
extern int renderer_free_count;
extern int renderer_texture_create_count;

GlyphAtlas* glyph_atlas_create_builtin(SDL_Renderer *sdl_ren) {
    if (!sdl_ren) return NULL;
    
    GlyphAtlas *atlas = malloc(sizeof(GlyphAtlas));
    if (!atlas) return NULL;
    renderer_alloc_count++;

    atlas->glyph_width = 8;
    atlas->glyph_height = 8;

    int atlas_w = 128 * 8;
    int atlas_h = 8;

    SDL_Surface *surface = SDL_CreateSurface(atlas_w, atlas_h, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        free(atlas);
        renderer_free_count++;
        return NULL;
    }

    SDL_FillSurfaceRect(surface, NULL, SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format), NULL, 0, 0, 0, 0));

    uint32_t *pixels = (uint32_t*)surface->pixels;
    int pitch = surface->pitch / 4;

    for (int c = 0; c < 128; c++) {
        for (int y = 0; y < 8; y++) {
            uint8_t row = font8x8_basic[c][y];
            for (int x = 0; x < 8; x++) {
                if (row & (1 << x)) {
                    pixels[y * pitch + (c * 8) + x] = 0xFFFFFFFF; // White pixel
                }
            }
        }
    }

    atlas->texture = SDL_CreateTextureFromSurface(sdl_ren, surface);
    SDL_DestroySurface(surface);
    
    if (!atlas->texture) {
        free(atlas);
        renderer_free_count++;
        return NULL;
    }
    
    renderer_texture_create_count++;
    SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);

    return atlas;
}

void glyph_atlas_destroy(GlyphAtlas *atlas) {
    if (!atlas) return;
    if (atlas->texture) {
        SDL_DestroyTexture(atlas->texture);
    }
    free(atlas);
    renderer_free_count++;
}
