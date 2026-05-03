#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>

int renderer_alloc_count = 0;
int renderer_free_count = 0;
int renderer_texture_create_count = 0;

static void* ren_malloc(size_t size) {
    renderer_alloc_count++;
    return malloc(size);
}

static void ren_free(void* ptr) {
    if (ptr) {
        renderer_free_count++;
        free(ptr);
    }
}

// Macro to quickly pack SDL_Color into an ABGR/RGBA uint32_t suitable for SDL_PIXELFORMAT_RGBA32.
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    #define COLOR_TO_UINT32(c) (((c).r << 24) | ((c).g << 16) | ((c).b << 8) | (c).a)
#else
    #define COLOR_TO_UINT32(c) (((c).a << 24) | ((c).b << 16) | ((c).g << 8) | (c).r)
#endif

// We need the raw font bits for the software pixel buffer. We will extract them from the atlas.
extern const unsigned char font8x8_basic[128][8];

Renderer* renderer_create(int win_w, int win_h, int grid_w, int grid_h, int cell_w, int cell_h) {
    if (grid_w <= 0 || grid_h <= 0 || cell_w <= 0 || cell_h <= 0) return NULL;

    Renderer *ren = ren_malloc(sizeof(Renderer));
    if (!ren) return NULL;

    ren->cell_w = cell_w;
    ren->cell_h = cell_h;
    ren->logical_w = grid_w * cell_w;
    ren->logical_h = grid_h * cell_h;
    ren->atlas = NULL;

    ren->pixel_buffer = ren_malloc(ren->logical_w * ren->logical_h * sizeof(uint32_t));
    if (!ren->pixel_buffer) {
        ren_free(ren);
        return NULL;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }

    ren->window = SDL_CreateWindow("ASCII FPS Prototype", win_w, win_h, SDL_WINDOW_RESIZABLE);
    if (!ren->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }

    ren->sdl_ren = SDL_CreateRenderer(ren->window, NULL);
    if (!ren->sdl_ren) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(ren->window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }

    SDL_SetRenderLogicalPresentation(ren->sdl_ren, ren->logical_w, ren->logical_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    ren->screen_texture = SDL_CreateTexture(ren->sdl_ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, ren->logical_w, ren->logical_h);
    if (!ren->screen_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ren->sdl_ren);
        SDL_DestroyWindow(ren->window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }
    renderer_texture_create_count++;

    ren->atlas = glyph_atlas_create_builtin(ren->sdl_ren);
    if (!ren->atlas) {
        fprintf(stderr, "glyph_atlas_create_builtin failed\n");
        SDL_DestroyTexture(ren->screen_texture);
        SDL_DestroyRenderer(ren->sdl_ren);
        SDL_DestroyWindow(ren->window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }

    return ren;
}

void renderer_destroy(Renderer *ren) {
    if (!ren) return;
    if (ren->atlas) glyph_atlas_destroy(ren->atlas);
    if (ren->screen_texture) SDL_DestroyTexture(ren->screen_texture);
    if (ren->sdl_ren) SDL_DestroyRenderer(ren->sdl_ren);
    if (ren->window) SDL_DestroyWindow(ren->window);
    if (ren->pixel_buffer) ren_free(ren->pixel_buffer);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    ren_free(ren);
}


void renderer_draw(Renderer *ren, Grid *grid) {
    uint32_t *pixels = ren->pixel_buffer;
    int pitch_pixels = ren->logical_w;
    
    // Write directly into the CPU memory buffer, completely avoiding SDL API call overhead
    for (int cy = 0; cy < grid->height; cy++) {
        for (int cx = 0; cx < grid->width; cx++) {
            Cell c = grid->cells[cy * grid->width + cx];
            
            uint32_t fg = COLOR_TO_UINT32(c.fg);
            uint32_t bg = COLOR_TO_UINT32(c.bg);
            
            const uint8_t *glyph_data = font8x8_basic[c.glyph < 128 ? c.glyph : 32];
            int base_x = cx * 8;
            int base_y = cy * 8;
            
            for (int y = 0; y < 8; y++) {
                uint8_t row = glyph_data[y];
                uint32_t *row_pixels = &pixels[(base_y + y) * pitch_pixels + base_x];
                
                // Unrolled inner loop for the 8 pixels of the font glyph
                row_pixels[0] = (row & 1) ? fg : bg;
                row_pixels[1] = (row & 2) ? fg : bg;
                row_pixels[2] = (row & 4) ? fg : bg;
                row_pixels[3] = (row & 8) ? fg : bg;
                row_pixels[4] = (row & 16) ? fg : bg;
                row_pixels[5] = (row & 32) ? fg : bg;
                row_pixels[6] = (row & 64) ? fg : bg;
                row_pixels[7] = (row & 128) ? fg : bg;
            }
        }
    }

    // Single texture update and render call per frame
    SDL_UpdateTexture(ren->screen_texture, NULL, pixels, pitch_pixels * sizeof(uint32_t));
    SDL_RenderClear(ren->sdl_ren);
    SDL_RenderTexture(ren->sdl_ren, ren->screen_texture, NULL, NULL);
    SDL_RenderPresent(ren->sdl_ren);
}

bool renderer_process_events(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            return false;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_ESCAPE) {
                return false;
            }
        }
    }
    return true;
}
