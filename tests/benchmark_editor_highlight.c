/** benchmark_editor_highlight.c — Headless editor highlight performance/stability */
#define _POSIX_C_SOURCE 200809L

#include "../src/config.h"
#include "../src/editor_highlight.h"
#include "../src/math.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define BENCHMARK_ITERATIONS 20000U
#define STABILITY_ITERATIONS 100000U
#define EDITOR_HIGHLIGHT_PASS_MS 1.0

static double now_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return -1.0;
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1000000.0;
}

static uint64_t grid_checksum(const Grid *grid) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t count = (size_t)grid->width * (size_t)grid->height;
    size_t i;
    for (i = 0U; i < count; i++) {
        const Cell *cell = &grid->cells[i];
        const uint8_t bytes[] = {
            cell->glyph,
            cell->fg.r, cell->fg.g, cell->fg.b, cell->fg.a,
            cell->bg.r, cell->bg.g, cell->bg.b, cell->bg.a
        };
        size_t j;
        for (j = 0U; j < sizeof(bytes); j++) {
            hash ^= bytes[j];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static SelectionTarget wall_target(void) {
    SelectionTarget target = {0};
    target.type = SELECTION_WALL_FACE;
    target.value.wall_face = (WallFaceRef){5, 3, WALL_FACE_WEST};
    return target;
}

static SelectionTarget light_target(SceneInstanceId id) {
    SelectionTarget target = {0};
    target.type = SELECTION_LIGHT;
    target.value.light.id = id;
    return target;
}

static EditorHit hover_target(SelectionTarget target) {
    EditorHit hover = {0};
    hover.valid = true;
    hover.distance = 3.0;
    hover.target = target;
    return hover;
}

static bool render_scenario(Grid *grid, Map *map, Camera *camera,
                            const SceneLight *lights, size_t light_count,
                            unsigned int scenario) {
    SelectionTarget selection = {0};
    EditorHit hover = {0};
    SDL_Color dark = {0, 0, 0, 255};
    int x;
    int y;

    for (y = 0; y < grid->height; y++) {
        for (x = 0; x < grid->width; x++) {
            if (!grid_set(grid, x, y, 'w', dark, dark)) return false;
        }
    }
    map_set(map, 3, 4, 0);
    switch (scenario & 3U) {
        case 0U:
            selection = light_target(11U);
            hover = hover_target(light_target(12U));
            break;
        case 1U:
            selection = wall_target();
            break;
        case 2U:
            map_set(map, 3, 4, 1);
            selection = light_target(13U);
            break;
        case 3U:
            selection = light_target(11U);
            hover = hover_target(selection);
            break;
    }
    editor_highlight_render(grid, map, camera, lights, light_count,
                            selection, hover);
    return true;
}

int main(int argc, char **argv) {
    bool stability = argc == 2 && strcmp(argv[1], "--stability") == 0;
    uint64_t iterations = stability ? STABILITY_ITERATIONS : BENCHMARK_ITERATIONS;
    Grid *grid = NULL;
    Map *map = NULL;
    Camera camera;
    SceneLight lights[3] = {
        {.id = 11U, .x = 4.5, .y = 3.5, .radius = 1.0},
        {.id = 12U, .x = 4.5, .y = 2.5, .radius = 1.0},
        {.id = 13U, .x = 4.5, .y = 4.5, .radius = 1.0}
    };
    uint64_t expected[4] = {0U, 0U, 0U, 0U};
    uint64_t i;
    double start;
    double elapsed;
    double average;
    bool deterministic = true;
    int result = 1;

    if (argc > 2 || (argc == 2 && !stability)) {
        fprintf(stderr, "usage: %s [--stability]\n", argv[0]);
        return 2;
    }
    config_init_defaults();
    grid = grid_create(80, 40);
    map = map_create(7, 7);
    if (!grid || !map) goto cleanup;
    map_set(map, 5, 3, 1);
    camera_init(&camera, 1.5, 3.5, 0.0, PI / 2.0);

    for (i = 0U; i < 4U; i++) {
        if (!render_scenario(grid, map, &camera, lights, 3U, (unsigned int)i))
            goto cleanup;
        expected[i] = grid_checksum(grid);
    }
    start = now_ms();
    if (start < 0.0) goto cleanup;
    for (i = 0U; i < iterations; i++) {
        unsigned int scenario = (unsigned int)(i & 3U);
        if (!render_scenario(grid, map, &camera, lights, 3U, scenario))
            goto cleanup;
        if (grid_checksum(grid) != expected[scenario]) {
            deterministic = false;
            break;
        }
    }
    elapsed = now_ms() - start;
    if (elapsed < 0.0) goto cleanup;
    average = elapsed / (double)(i == 0U ? 1U : i);
    printf("{\n");
    printf("  \"mode\": \"%s\",\n", stability ? "stability" : "benchmark");
    printf("  \"iterations\": %llu,\n", (unsigned long long)i);
    printf("  \"elapsed_ms\": %.3f,\n", elapsed);
    printf("  \"avg_scenario_ms\": %.6f,\n", average);
    printf("  \"pass_budget_ms\": %.3f,\n", EDITOR_HIGHLIGHT_PASS_MS);
    printf("  \"deterministic\": %s,\n", deterministic ? "true" : "false");
    printf("  \"result\": \"%s\"\n",
           deterministic && average <= EDITOR_HIGHLIGHT_PASS_MS ? "pass" : "fail");
    printf("}\n");
    result = deterministic && i == iterations &&
        average <= EDITOR_HIGHLIGHT_PASS_MS ? 0 : 1;

cleanup:
    map_destroy(map);
    grid_destroy(grid);
    return result;
}