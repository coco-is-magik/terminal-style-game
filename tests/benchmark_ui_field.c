/** benchmark_ui_field.c — Headless cost of the paused-context living_field backdrop.
 *
 * Reference: UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §7 — dense coordinated
 * decoration must fit the 6 ms surface budget. This measures the shared
 * evaluator cost of composing the pause context (authored menu + the
 * living_field animation unit) at several phases, and verifies checksum
 * determinism.
 */
#define _POSIX_C_SOURCE 200809L

#include "../src/grid.h"
#include "../src/ui_animation.h"
#include "../src/ui_ele.h"
#include "../src/ui_theme.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCHMARK_ITERATIONS 200U
#define STABILITY_ITERATIONS 1000U
#define SURFACE_RENDER_PASS_MS 6.0
#define FIELD_PHASES 8U

static double now_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return -1.0;
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1000000.0;
}

static uint64_t grid_checksum(const Grid *grid) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t count = (size_t)grid->width * (size_t)grid->height;
    for (size_t i = 0U; i < count; i++) {
        const Cell *cell = &grid->cells[i];
        const uint8_t bytes[] = {
            cell->glyph,
            cell->fg.r, cell->fg.g, cell->fg.b, cell->fg.a,
            cell->bg.r, cell->bg.g, cell->bg.b, cell->bg.a
        };
        for (size_t j = 0U; j < sizeof(bytes); j++) {
            hash ^= bytes[j];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static void set_content(UiElement *element, const char *text) {
    size_t length = strlen(text) + 1U;
    element->content = malloc(length);
    if (element->content) {
        memcpy(element->content, text, length);
        element->content_capacity = length;
    }
}

static UiElement make_element(const char *name, UiElementType type,
                              int x, int y, int width, int height) {
    UiElement value = {0};
    (void)snprintf(value.name, sizeof(value.name), "%s", name);
    value.type = type;
    value.layout = (UiElementLayout){x, y, UI_COORD_ABSOLUTE, width, height};
    value.visible = 1;
    value.z_index = 0;
    value.align = UI_ALIGN_LEFT;
    (void)snprintf(value.style, sizeof(value.style), "plain");
    (void)snprintf(value.transition, sizeof(value.transition), "none");
    (void)snprintf(value.focus_effect, sizeof(value.focus_effect), "none");
    return value;
}

/* Times one field configuration, verifying that every phase reproduces its
   reference cells. Returns false if a phase is not deterministic. */
static bool measure_field(UiLayout *layout, Grid *grid, const double *phase_time,
                          uint64_t iterations, SDL_Color background,
                          SDL_Color foreground, double *out_average) {
    uint64_t reference[FIELD_PHASES];
    double total = 0.0;

    for (unsigned int phase = 0U; phase < FIELD_PHASES; phase++) {
        (void)grid_clear(grid, background);
        ui_layout_render(layout, grid, foreground, background);
        if (!ui_animation_render_layout(layout, grid, phase_time[phase], false,
                                        false, UI_ANIMATION_EVENT_WHILE_VISIBLE))
            return false;
        reference[phase] = grid_checksum(grid);
    }

    for (uint64_t i = 0U; i < iterations; i++) {
        unsigned int phase = (unsigned int)(i % FIELD_PHASES);
        double start;
        double end;
        (void)grid_clear(grid, background);
        ui_layout_render(layout, grid, foreground, background);
        start = now_ms();
        if (!ui_animation_render_layout(layout, grid, phase_time[phase], false,
                                        false, UI_ANIMATION_EVENT_WHILE_VISIBLE))
            return false;
        end = now_ms();
        if (start < 0.0 || end < start || grid_checksum(grid) != reference[phase])
            return false;
        total += end - start;
    }
    *out_average = total / (double)iterations;
    return true;
}

int main(int argc, char **argv) {
    bool stability = argc == 2 && strcmp(argv[1], "--stability") == 0;
    uint64_t iterations = stability ? STABILITY_ITERATIONS : BENCHMARK_ITERATIONS;
    Grid *grid = grid_create(260, 160);
    UiElement container = make_element("pause_menu_container", UI_ELE_CONTAINER, 120, 69, 24, 18);
    UiElement title = make_element("pause_menu_title", UI_ELE_TEXT, 121, 69, 22, 1);
    UiElement resume = make_element("pause_menu_resume", UI_ELE_BUTTON, 122, 74, 20, 1);
    UiElement main_menu = make_element("pause_menu_main", UI_ELE_BUTTON, 122, 77, 20, 1);
    UiElement settings = make_element("pause_menu_settings", UI_ELE_BUTTON, 122, 80, 20, 1);
    UiElement quit = make_element("pause_menu_quit", UI_ELE_BUTTON, 122, 83, 20, 1);
    UiElement field = make_element("animation_living_field", UI_ELE_ANIMATION, -30, -9, 80, 40);
    UiLayout layout = {0};
    SDL_Color background = {5, 8, 10, 255};
    SDL_Color foreground = {242, 247, 248, 255};
    double phase_time[FIELD_PHASES];
    unsigned int period;
    double bounded_average = 0.0;
    double surface_average = 0.0;
    int result = 1;

    if (argc > 2 || (argc == 2 && !stability)) {
        fprintf(stderr, "usage: %s [--stability]\n", argv[0]);
        return 2;
    }
    if (!grid) return 1;
    period = ui_theme_motion_duration_ms(UI_THEME_MOTION_AMBIENT, false);
    if (period == 0U) {
        grid_destroy(grid);
        return 1;
    }

    (void)snprintf(field.target, sizeof(field.target), "pause_menu_container");
    (void)snprintf(field.preset, sizeof(field.preset), "living_field");
    (void)snprintf(field.trigger, sizeof(field.trigger), "while_visible");
    (void)snprintf(field.orientation, sizeof(field.orientation), "radial");
    set_content(&title, "-- PAUSED --");
    set_content(&resume, "RESUME");
    set_content(&main_menu, "MAIN MENU");
    set_content(&settings, "SETTINGS");
    set_content(&quit, "QUIT");
    if (title.content == NULL || resume.content == NULL || main_menu.content == NULL ||
        settings.content == NULL || quit.content == NULL) goto cleanup;

    layout.elements[layout.element_count++] = &container;
    layout.elements[layout.element_count++] = &title;
    layout.elements[layout.element_count++] = &resume;
    layout.elements[layout.element_count++] = &main_menu;
    layout.elements[layout.element_count++] = &settings;
    layout.elements[layout.element_count++] = &quit;
    layout.elements[layout.element_count++] = &field;

    if (!ui_ele_animation_is_valid(&field)) goto cleanup;

    for (unsigned int phase = 0U; phase < FIELD_PHASES; phase++)
        phase_time[phase] = (double)period * (double)phase / (double)FIELD_PHASES;

    /* Both authorable shapes are measured: a bounded backdrop region (`box`) and
       the shipped surface-filling backdrop (`surface`, the production shape). */
    if (!measure_field(&layout, grid, phase_time, iterations, background, foreground,
                       &bounded_average)) goto cleanup;
    field.extent = UI_EXTENT_SURFACE;
    if (!ui_ele_animation_is_valid(&field)) goto cleanup;
    if (!measure_field(&layout, grid, phase_time, iterations, background, foreground,
                       &surface_average)) goto cleanup;

    printf("ui-field: living_field bounded %.3f ms, surface %.3f ms over %llu iterations "
           "(budget %.1f ms)\n",
           bounded_average, surface_average, (unsigned long long)iterations,
           SURFACE_RENDER_PASS_MS);
    result = bounded_average < SURFACE_RENDER_PASS_MS &&
             surface_average < SURFACE_RENDER_PASS_MS ? 0 : 1;

cleanup:
    free(title.content);
    free(resume.content);
    free(main_menu.content);
    free(settings.content);
    free(quit.content);
    grid_destroy(grid);
    return result;
}

