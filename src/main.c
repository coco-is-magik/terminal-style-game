#include "renderer.h"
#include "grid.h"
#include <stdio.h>
#include <stdbool.h>

// Window size independent from character grid size
int WINDOW_WIDTH = 1920;
int WINDOW_HEIGHT = 1080;

// These two variables control the logical cell count
int GRID_WIDTH = 240;
int GRID_HEIGHT = 160;

// Logical visual size of each cell
#define CELL_WIDTH 8
#define CELL_HEIGHT 8

// Configurable target FPS
int TARGET_FPS = 120;

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

void draw_ui_overlay(Grid *grid, uint64_t frame_count, double frame_time_ms, double target_time_ms, double spare_time_ms, bool over_budget) {
    char ui_text[512];
    snprintf(ui_text, sizeof(ui_text), 
             "Grid: %dx%d\n"
             "Frame: %llu\n"
             "Press ESC to quit\n"
             "---\n"
             "FPS Target: %d\n"
             "Frame Time: %.2f ms\n"
             "Target Time: %.2f ms\n"
             "Spare Time: %.2f ms\n"
             "Budget Status: %s", 
             grid->width, grid->height, 
             (unsigned long long)frame_count,
             TARGET_FPS,
             frame_time_ms,
             target_time_ms,
             spare_time_ms,
             over_budget ? "OVER BUDGET" : "OK");

    SDL_Color ui_fg = {255, 255, 255, 255};
    SDL_Color ui_bg = over_budget ? (SDL_Color){150, 0, 0, 255} : (SDL_Color){0, 0, 0, 255};

    // Draw UI at an offset (x=2, y=2)
    grid_print(grid, 2, 2, ui_text, ui_fg, ui_bg);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Renderer *ren = renderer_create(WINDOW_WIDTH, WINDOW_HEIGHT, GRID_WIDTH, GRID_HEIGHT, CELL_WIDTH, CELL_HEIGHT);
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

    double last_frame_time_ms = 0.0;
    double last_spare_time_ms = 0.0;
    bool last_over_budget = false;

    while (running) {
        uint64_t start_time = SDL_GetPerformanceCounter();

        running = renderer_process_events();

        // 1. Update World Layer
        draw_world_pattern(grid, frame_count);

        double target_time_ms = 1000.0 / TARGET_FPS;

        // 2. Update UI Layer (using timing from previous frame)
        draw_ui_overlay(grid, frame_count, last_frame_time_ms, target_time_ms, last_spare_time_ms, last_over_budget);

        // 3. Render
        renderer_draw(ren, grid);

        uint64_t end_time = SDL_GetPerformanceCounter();
        
        last_frame_time_ms = (double)((end_time - start_time) * 1000) / SDL_GetPerformanceFrequency();
        last_spare_time_ms = target_time_ms - last_frame_time_ms;
        last_over_budget = last_spare_time_ms < 0;

        if (last_spare_time_ms > 0) {
            SDL_Delay((uint32_t)last_spare_time_ms);
        }

        frame_count++;
    }

    grid_destroy(grid);
    renderer_destroy(ren);

    return 0;
}
