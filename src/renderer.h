#ifndef RENDERER_H
#define RENDERER_H

#include "grid.h"
#include <SDL3/SDL.h>
#include <stdbool.h>

// Instrumentation counters
extern int renderer_alloc_count;
extern int renderer_free_count;
extern int renderer_texture_create_count;

#include "glyph_atlas.h"

typedef struct {
    SDL_Window *window;
    SDL_Renderer *sdl_ren;
    SDL_Texture *screen_texture;
    GlyphAtlas *atlas;
    uint32_t *pixel_buffer;
    int logical_w;
    int logical_h;
    int cell_w;
    int cell_h;
} Renderer;

Renderer* renderer_create(int win_w, int win_h, int grid_w, int grid_h, int cell_w, int cell_h);
void renderer_destroy(Renderer *ren);
void renderer_draw(Renderer *ren, Grid *grid);
bool renderer_process_events(void);

#endif
