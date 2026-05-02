#include "renderer.h"
#include "font8x8.h"
#include <stdio.h>
#include <stdlib.h>

// Generates an atlas texture containing the 128 ASCII characters 
// from our built-in 8x8 bitmap font.
static SDL_Texture* generate_font_atlas(SDL_Renderer *sdl_ren) {
    // 128 characters, each 8x8 pixels. We'll lay them out horizontally.
    int atlas_w = 128 * 8;
    int atlas_h = 8;

    SDL_Surface *surface = SDL_CreateSurface(atlas_w, atlas_h, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return NULL;

    // Fill with transparent background
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

    SDL_Texture *atlas = SDL_CreateTextureFromSurface(sdl_ren, surface);
    SDL_DestroySurface(surface);
    return atlas;
}

Renderer* renderer_create(int grid_w, int grid_h, int cell_w, int cell_h) {
    Renderer *ren = malloc(sizeof(Renderer));
    if (!ren) return NULL;

    ren->cell_w = cell_w;
    ren->cell_h = cell_h;

    int window_w = grid_w * cell_w;
    int window_h = grid_h * cell_h;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        free(ren);
        return NULL;
    }

    ren->window = SDL_CreateWindow("ASCII FPS Prototype", window_w, window_h, 0);
    if (!ren->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        free(ren);
        return NULL;
    }

    ren->sdl_ren = SDL_CreateRenderer(ren->window, NULL);
    if (!ren->sdl_ren) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(ren->window);
        SDL_Quit();
        free(ren);
        return NULL;
    }

    ren->font_atlas = generate_font_atlas(ren->sdl_ren);
    if (!ren->font_atlas) {
        fprintf(stderr, "Failed to generate font atlas.\n");
        SDL_DestroyRenderer(ren->sdl_ren);
        SDL_DestroyWindow(ren->window);
        SDL_Quit();
        free(ren);
        return NULL;
    }
    
    // Allow color modulation for the white text
    SDL_SetTextureBlendMode(ren->font_atlas, SDL_BLENDMODE_BLEND);

    return ren;
}

void renderer_destroy(Renderer *ren) {
    if (!ren) return;
    if (ren->font_atlas) SDL_DestroyTexture(ren->font_atlas);
    if (ren->sdl_ren) SDL_DestroyRenderer(ren->sdl_ren);
    if (ren->window) SDL_DestroyWindow(ren->window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    free(ren);
}

void renderer_draw(Renderer *ren, Grid *grid) {
    SDL_SetRenderDrawColor(ren->sdl_ren, 0, 0, 0, 255);
    SDL_RenderClear(ren->sdl_ren);

    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            Cell c = grid->cells[y * grid->width + x];
            
            SDL_FRect dest_rect = { 
                (float)(x * ren->cell_w), 
                (float)(y * ren->cell_h), 
                (float)ren->cell_w, 
                (float)ren->cell_h 
            };

            // Draw Background
            SDL_SetRenderDrawColor(ren->sdl_ren, c.bg.r, c.bg.g, c.bg.b, c.bg.a);
            SDL_RenderFillRect(ren->sdl_ren, &dest_rect);

            // Draw Glyph if it's printable and not a space
            if (c.glyph > 32 && c.glyph < 127) {
                SDL_SetTextureColorMod(ren->font_atlas, c.fg.r, c.fg.g, c.fg.b);
                SDL_SetTextureAlphaMod(ren->font_atlas, c.fg.a);
                
                SDL_FRect src_rect = {
                    (float)(c.glyph * 8), 0.0f, 8.0f, 8.0f
                };

                SDL_RenderTexture(ren->sdl_ren, ren->font_atlas, &src_rect, &dest_rect);
            }
        }
    }

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
