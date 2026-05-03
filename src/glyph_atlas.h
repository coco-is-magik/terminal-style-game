#ifndef GLYPH_ATLAS_H
#define GLYPH_ATLAS_H
#include <SDL3/SDL.h>

typedef struct {
    SDL_Texture *texture;
    int glyph_width;
    int glyph_height;
} GlyphAtlas;

GlyphAtlas* glyph_atlas_create_builtin(SDL_Renderer *sdl_ren);
void glyph_atlas_destroy(GlyphAtlas *atlas);

#endif
