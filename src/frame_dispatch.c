#include "frame_dispatch.h"
#include <string.h>

void frame_dispatch_apply_scenario(Grid *grid, Camera *camera,
                                   const char *scenario, uint64_t frame_count) {
    if (!grid || !camera || !scenario || frame_count < 64) return;
    if (strcmp(scenario, "camera") == 0) camera->transform.angle += 0.01047f;
    else if (strcmp(scenario, "rotate") == 0) camera->transform.angle += 0.04189f;
    else if (strcmp(scenario, "flicker") == 0) {
        for (int index = 0; index < grid->width * grid->height; index++) {
            if (((index * 1103515245u + (unsigned)frame_count * 12345u) % 100u) < 3u) {
                int x = index % grid->width;
                int y = index / grid->width;
                Cell cell;
                if (grid_get(grid, x, y, &cell)) {
                    cell.bg.r = (frame_count & 1U) ? 0xFF : 0x80;
                    grid_set(grid, x, y, cell.glyph, cell.fg, cell.bg);
                }
            }
        }
    } else if (strcmp(scenario, "ui") == 0) {
        int row_start = grid->height >= 2 ? grid->height - 2 : 0;
        for (int y = row_start; y < grid->height; y++) {
            for (int x = 0; x < grid->width; x++) {
                Cell cell;
                if (grid_get(grid, x, y, &cell)) {
                    cell.bg.r = (frame_count & 1U) ? 0x40 : 0x20;
                    cell.bg.g = (frame_count & 2U) ? 0x40 : 0x20;
                    cell.bg.b = (frame_count & 4U) ? 0x40 : 0x20;
                    grid_set(grid, x, y, cell.glyph, cell.fg, cell.bg);
                }
            }
        }
    } else if (strcmp(scenario, "fullchange") == 0) {
        for (int index = 0; index < grid->width * grid->height; index++) {
            int x = index % grid->width;
            int y = index / grid->width;
            Cell cell;
            if (grid_get(grid, x, y, &cell)) {
                cell.bg.r = (frame_count & 1U) ? 0xFF : 0x00;
                grid_set(grid, x, y, cell.glyph, cell.fg, cell.bg);
            }
        }
    }
}