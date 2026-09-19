#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/checked_size.h"
#include "../src/grid.h"
#include "../src/menu_state.h"
#include "../src/ui_app_theme_adapter.h"
#include "../src/ui_preferences.h"
#include "../src/ui_workbench.h"
#include "../src/ui_workbench_frame.h"
#include "../src/ui_workbench_runtime.h"
#include "../src/ui_workbench_guide.h"
#include "../src/number_parse.h"

#include "ui_workbench_frame_fixtures.h"

static int write_text_frame(const Grid *grid) {
    int y;
    int x;
    if (!grid || !grid->cells) return 1;
    for (y = 0; y < grid->height; y++) {
        for (x = 0; x < grid->width; x++) {
            const Cell *cell = &grid->cells[(size_t)y * (size_t)grid->width +
                                            (size_t)x];
            uint8_t glyph = cell->glyph;
            if (glyph >= 32 && glyph < 127) {
                fputc((int)glyph, stdout);
            } else if (glyph == 0) {
                fputc('.', stdout);
            } else {
                fputc(' ', stdout);
            }
        }
        fputc('\n', stdout);
    }
    return 0;
}

static int usage(const char *program) {
    (void)fprintf(stderr,
                  "usage: %s --context main|pause|settings|confirm [--scale 100|125|150|200] [--workbench-scale 100|125|150|200] [--elapsed-ms N] [--checksum-only] [--pixels output.ppm]\n",
                  program ? program : "ui-workbench-frame");
    return 2;
}

static MenuId parse_context(const char *text, bool *ok) {
    if (!text || !ok) return MENU_NONE;
    *ok = true;
    if (strcmp(text, "main") == 0) return MENU_MAIN;
    if (strcmp(text, "pause") == 0) return MENU_PAUSE;
    if (strcmp(text, "settings") == 0) return MENU_SETTINGS;
    if (strcmp(text, "confirm") == 0) return MENU_CONFIRM_QUIT;
    *ok = false;
    return MENU_NONE;
}

int main(int argc, char **argv) {
    const char *context_text = NULL;
    const char *scale_text = "100";
    const char *elapsed_text = "0";
    bool checksum_only = false;
    bool editing = false;
    bool reduced_motion = false;
    const char *pixel_path = NULL;
    int workbench_scale = 100;
    MenuId context = MENU_NONE;
    UiWorkbench workbench;
    Grid *grid = NULL;
    UiAppWorkbenchPalette palette;
    UiWorkbenchFrameInput input = {0};
    Cell *cells = NULL;
    size_t cell_count = 0U;
    bool context_ok = false;
    int scale_percent = 100;
    double elapsed_ms = 0.0;
    int status = 1;
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--context") == 0 && i + 1 < argc) {
            context_text = argv[++i];
        } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            scale_text = argv[++i];
        } else if (strcmp(argv[i], "--elapsed-ms") == 0 && i + 1 < argc) {
            elapsed_text = argv[++i];
        } else if (strcmp(argv[i], "--checksum-only") == 0) {
            checksum_only = true;
        } else if (strcmp(argv[i], "--editing") == 0) {
            editing = true;
        } else if (strcmp(argv[i], "--reduced-motion") == 0) {
            reduced_motion = true;
        } else if (strcmp(argv[i], "--pixels") == 0 && i + 1 < argc) {
            pixel_path = argv[++i];
        } else if (strcmp(argv[i], "--workbench-scale") == 0 && i + 1 < argc) {
            if (!number_parse_int(argv[++i], 100, 200, &workbench_scale) ||
                !ui_preferences_is_valid_scale(workbench_scale)) return usage(argv[0]);
        } else {
            return usage(argv[0]);
        }
    }
    if (!context_text) return usage(argv[0]);
    context = parse_context(context_text, &context_ok);
    if (!context_ok) return usage(argv[0]);
    if (!number_parse_int(scale_text, 100, 200, &scale_percent) ||
        !number_parse_finite_double(elapsed_text, &elapsed_ms) ||
        !ui_preferences_is_valid_scale(scale_percent) || elapsed_ms < 0.0 ||
        elapsed_ms > 3600000.0)
        return usage(argv[0]);
    ui_workbench_init(&workbench);
    if (ui_workbench_open(&workbench, context) != UI_WORKBENCH_OK) {
        (void)fprintf(stderr, "ui-workbench-frame: context load failed\n");
        goto cleanup;
    }
    if (!ui_app_theme_workbench_palette(&palette)) {
        (void)fprintf(stderr, "ui-workbench-frame: palette unavailable\n");
        goto cleanup;
    }
    workbench.editing = editing;
    if (pixel_path) {
        const size_t count = 2080U * 1280U;
        uint32_t *pixels = malloc(count * sizeof(*pixels));
        UiWorkbenchGuide guide;
        FILE *file = NULL;
        size_t p;
        uint64_t hash = UINT64_C(14695981039346656037);
        if (!pixels) goto cleanup;
        if (ui_workbench_guide_load(&guide, "assets/editor_tooltips.txt") != UI_WORKBENCH_GUIDE_OK) {
            free(pixels);
            goto cleanup;
        }
        if (!ui_workbench_runtime_snapshot(&workbench, &palette,
                ui_workbench_guide_tooltip(&guide, &workbench), scale_percent,
                 workbench_scale, elapsed_ms, reduced_motion, pixels, count)) {
            ui_workbench_guide_destroy(&guide);
            free(pixels);
            goto cleanup;
        }
        file = fopen(pixel_path, "wb");
        status = file && fprintf(file, "P6\n2080 1280\n255\n") > 0 ? 0 : 1;
        for (p = 0; status == 0 && p < count; p++) {
            unsigned char rgba[4];
            size_t component;
            memcpy(rgba, &pixels[p], sizeof(rgba));
            for (component = 0; component < 4; component++) {
                hash ^= rgba[component];
                hash *= UINT64_C(1099511628211);
            }
            if (fwrite(rgba, 1, 3, file) != 3) status = 1;
        }
        if (file && fclose(file) != 0) status = 1;
        if (status == 0) printf("pixel-checksum=%llu\n", (unsigned long long)hash);
        ui_workbench_guide_destroy(&guide);
        free(pixels);
        goto cleanup;
    }
    grid = grid_create(UI_WORKBENCH_FRAME_CONTRACT_COLUMNS,
                       UI_WORKBENCH_FRAME_CONTRACT_ROWS);
    if (!grid) {
        (void)fprintf(stderr, "ui-workbench-frame: grid unavailable\n");
        goto cleanup;
    }
    input.grid = grid;
    input.workbench = &workbench;
    input.palette = &palette;
    input.scale_percent = scale_percent;
    input.elapsed_ms = elapsed_ms;
    input.reduced_motion = reduced_motion;
    if (!ui_workbench_frame_render(input)) {
        (void)fprintf(stderr, "ui-workbench-frame: composition failed\n");
        goto cleanup;
    }
    if (!checked_size_2d(grid->width, grid->height, &cell_count) ||
        cell_count == 0U) {
        (void)fprintf(stderr, "ui-workbench-frame: frame too large\n");
        goto cleanup;
    }
    cells = malloc(cell_count * sizeof(*cells));
    if (!cells) {
        (void)fprintf(stderr, "ui-workbench-frame: cells unavailable\n");
        goto cleanup;
    }
    if (!ui_workbench_frame_copy_cells(grid, cells, cell_count)) {
        (void)fprintf(stderr, "ui-workbench-frame: cells copy failed\n");
        goto cleanup;
    }
    (void)printf("checksum=%llu\n",
                 (unsigned long long)ui_workbench_frame_checksum_fixture(
                     cells, cell_count));
    status = 0;
    if (checksum_only) goto cleanup;
    status = write_text_frame(grid);
cleanup:
    free(cells);
    if (grid) grid_destroy(grid);
    ui_workbench_destroy(&workbench);
    return status;
}