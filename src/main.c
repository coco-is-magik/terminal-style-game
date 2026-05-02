#include "renderer.h"
#include "grid.h"
#include <stdio.h>

// These two variables control the logical cell count
int GRID_WIDTH = 40;
int GRID_HEIGHT = 20;

// Logical visual size of each cell
#define CELL_WIDTH 8
#define CELL_HEIGHT 8

void draw_world_pattern(Grid *grid, uint64_t frame_count) {
    SDL_Color bg_color = {0, 0, 0, 255};
    grid_clear(grid, bg_color);

    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            // Draw a moving diagonal/wave test pattern
            uint8_t glyph = 33 + ((x + y + frame_count / 4) % 94);
            
            SDL_Color fg = {
                (uint8_t)(128 + 127 * SDL_sinf((x + frame_count / 10.0f) * 0.1f)),
                (uint8_t)(128 + 127 * SDL_sinf((y + frame_count / 10.0f) * 0.1f)),
                (uint8_t)(255),
                255
            };
            SDL_Color bg = {0, 0, 0, 255};

            // Draw border
            if (x == 0 || x == grid->width - 1 || y == 0 || y == grid->height - 1) {
                glyph = '#';
                fg = (SDL_Color){255, 255, 255, 255};
                bg = (SDL_Color){100, 0, 0, 255};
            }

            grid_set(grid, x, y, glyph, fg, bg);
        }
    }
}

void draw_ui_overlay(Grid *grid, uint64_t frame_count) {
    char ui_text[256];
    snprintf(ui_text, sizeof(ui_text), "Grid: %dx%d\nFrame: %llu\nPress ESC to quit", 
             grid->width, grid->height, (unsigned long long)frame_count);

    SDL_Color ui_fg = {255, 255, 255, 255};
    SDL_Color ui_bg = {0, 0, 0, 255};

    // Draw UI at an offset (x=2, y=2)
    grid_print(grid, 2, 2, ui_text, ui_fg, ui_bg);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Renderer *ren = renderer_create(GRID_WIDTH, GRID_HEIGHT, CELL_WIDTH, CELL_HEIGHT);
    if (!ren) {
        fprintf(stderr, "Failed to initialize renderer.\n");
        return 1;
    }

    Grid *grid = grid_create(GRID_WIDTH, GRID_HEIGHT);
    if (!grid) {
        fprintf(stderr, "Failed to initialize grid.\n");
        renderer_destroy(ren);
        return 1;
    }

    uint64_t frame_count = 0;
    bool running = true;

    while (running) {
        running = renderer_process_events();

        // 1. Update World Layer
        draw_world_pattern(grid, frame_count);

        // 2. Update UI Layer
        draw_ui_overlay(grid, frame_count);

        // 3. Render
        renderer_draw(ren, grid);

        frame_count++;
        SDL_Delay(16); // ~60 FPS
    }

    grid_destroy(grid);
    renderer_destroy(ren);

    return 0;
}
