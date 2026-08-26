#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "../src/r9_optical_compositor.h"

#ifndef R9_OPTICAL_RESEARCH
#error "P3 focused runner requires R9_OPTICAL_RESEARCH=1"
#endif

static const Cell darkness = {
    ' ', {0U, 0U, 0U, 255U}, {0U, 0U, 0U, 255U}
};

static R9OpticalLayer layer(uint8_t glyph, SDL_Color fg, SDL_Color bg,
                            uint8_t opacity, uint8_t transmission,
                            bool ray_blocks, bool light_blocks,
                            bool generated_boundary) {
    R9OpticalLayer value = {
        {glyph, fg, bg},
        {false, ray_blocks, light_blocks, opacity, transmission, 0U},
        generated_boundary
    };
    return value;
}

static uint64_t cell_checksum(const Cell *cell) {
    const uint8_t bytes[] = {
        cell->glyph,
        cell->fg.r, cell->fg.g, cell->fg.b, cell->fg.a,
        cell->bg.r, cell->bg.g, cell->bg.b, cell->bg.a
    };
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0U; i < sizeof(bytes); i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void assert_color(SDL_Color color, uint8_t r, uint8_t g, uint8_t b) {
    assert_int_equal(color.r, r);
    assert_int_equal(color.g, g);
    assert_int_equal(color.b, b);
    assert_int_equal(color.a, 255U);
}

static void test_single_opaque_preserves_sample_exactly(void **state) {
    R9OpticalLayer layers[] = {
        layer('#', (SDL_Color){200U, 100U, 50U, 17U},
              (SDL_Color){10U, 20U, 30U, 23U}, 255U, 0U, true, true, false)
    };
    R9OpticalComposite result;
    (void)state;
    assert_true(r9_optical_composite(layers, 1U, false, &darkness, &result));
    assert_int_equal(result.cell.glyph, '#');
    assert_color(result.cell.fg, 200U, 100U, 50U);
    assert_color(result.cell.bg, 10U, 20U, 30U);
    assert_int_equal(result.consumed_layers, 1U);
    assert_true(result.terminated_by_surface);
    assert_false(result.reached_opening);
    assert_false(result.layer_cap_exhausted);
    assert_int_equal(cell_checksum(&result.cell), UINT64_C(4429488470940348798));
}

static void test_glass_over_wall(void **state) {
    R9OpticalLayer layers[] = {
        layer('g', (SDL_Color){100U, 150U, 200U, 255U},
              (SDL_Color){20U, 40U, 60U, 255U}, 64U, 192U, false, false, false),
        layer('W', (SDL_Color){200U, 100U, 50U, 255U},
              (SDL_Color){100U, 80U, 60U, 255U}, 255U, 0U, true, true, false)
    };
    R9OpticalComposite result;
    (void)state;
    assert_true(r9_optical_composite(layers, 2U, false, &darkness, &result));
    assert_int_equal(result.cell.glyph, 'W');
    assert_color(result.cell.fg, 176U, 113U, 88U);
    assert_color(result.cell.bg, 80U, 70U, 60U);
    assert_int_equal(result.consumed_layers, 2U);
    assert_true(result.terminated_by_surface);
    assert_int_equal(cell_checksum(&result.cell), UINT64_C(11839026229836985365));
}

static void test_two_glass_layers_over_wall(void **state) {
    R9OpticalLayer layers[] = {
        layer('a', (SDL_Color){100U, 150U, 200U, 255U},
              (SDL_Color){20U, 40U, 60U, 255U}, 64U, 192U, false, false, false),
        layer('b', (SDL_Color){40U, 80U, 120U, 255U},
              (SDL_Color){10U, 20U, 30U, 255U}, 80U, 175U, false, false, false),
        layer('W', (SDL_Color){200U, 100U, 50U, 255U},
              (SDL_Color){100U, 80U, 60U, 255U}, 255U, 0U, true, true, false)
    };
    R9OpticalComposite result;
    (void)state;
    assert_true(r9_optical_composite(layers, 3U, false, &darkness, &result));
    assert_int_equal(result.cell.glyph, 'W');
    assert_color(result.cell.fg, 138U, 108U, 104U);
    assert_color(result.cell.bg, 59U, 56U, 53U);
    assert_int_equal(cell_checksum(&result.cell), UINT64_C(8305363331603521064));
}

static void test_glass_over_opening_uses_darkness(void **state) {
    R9OpticalLayer layers[] = {
        layer('g', (SDL_Color){100U, 150U, 200U, 255U},
              (SDL_Color){20U, 40U, 60U, 255U}, 64U, 192U, false, false, false)
    };
    R9OpticalComposite result;
    (void)state;
    assert_true(r9_optical_composite(layers, 1U, true, &darkness, &result));
    assert_int_equal(result.cell.glyph, ' ');
    assert_color(result.cell.fg, 25U, 38U, 50U);
    assert_color(result.cell.bg, 5U, 10U, 15U);
    assert_false(result.terminated_by_surface);
    assert_true(result.reached_opening);
    assert_false(result.layer_cap_exhausted);
    assert_int_equal(cell_checksum(&result.cell), UINT64_C(14523133264066336396));
}

static void test_glass_adjacent_to_generated_discontinuity(void **state) {
    R9OpticalLayer layers[] = {
        layer('g', (SDL_Color){100U, 150U, 200U, 255U},
              (SDL_Color){20U, 40U, 60U, 255U}, 64U, 192U, false, false, false),
        layer('D', (SDL_Color){240U, 40U, 20U, 255U},
              (SDL_Color){20U, 10U, 240U, 255U}, 255U, 0U, true, true, true)
    };
    R9OpticalComposite result;
    (void)state;
    assert_true(layers[1].generated_boundary);
    assert_true(r9_optical_composite(layers, 2U, false, &darkness, &result));
    assert_int_equal(result.cell.glyph, 'D');
    assert_color(result.cell.fg, 206U, 68U, 65U);
    assert_color(result.cell.bg, 20U, 18U, 196U);
    assert_int_equal(cell_checksum(&result.cell), UINT64_C(5371222891600692856));
}

static void test_glyph_threshold_and_absorption(void **state) {
    R9OpticalLayer layers[] = {
        layer('N', (SDL_Color){255U, 0U, 0U, 255U},
              (SDL_Color){0U, 0U, 0U, 255U}, 127U, 64U, false, false, false),
        layer('F', (SDL_Color){0U, 255U, 0U, 255U},
              (SDL_Color){0U, 0U, 0U, 255U}, 128U, 0U, true, true, false)
    };
    R9OpticalComposite result;
    (void)state;
    assert_true(r9_optical_composite(layers, 2U, false, &darkness, &result));
    assert_int_equal(result.cell.glyph, 'F');
    assert_color(result.cell.fg, 127U, 32U, 0U);
}

static void test_selective_continuation_and_light_are_independent(void **state) {
    R9OpticalResolved optical = {false, false, false, 255U, 128U, 0U};
    (void)state;
    assert_true(r9_optical_sight_continues(&optical));
    assert_int_equal(r9_optical_transmit_light(&optical, 200U), 100U);
    optical.ray_blocks = true;
    assert_false(r9_optical_sight_continues(&optical));
    assert_int_equal(r9_optical_transmit_light(&optical, 200U), 100U);
    optical.ray_blocks = false;
    optical.light_blocks = true;
    assert_true(r9_optical_sight_continues(&optical));
    assert_int_equal(r9_optical_transmit_light(&optical, 200U), 0U);
    optical.light_blocks = false;
    optical.transmission = 0U;
    assert_false(r9_optical_sight_continues(&optical));
    assert_int_equal(r9_optical_transmit_light(&optical, 200U), 0U);
    assert_false(r9_optical_sight_continues(NULL));
    assert_int_equal(r9_optical_transmit_light(NULL, 200U), 0U);
}

static void test_transmissive_cap_is_not_reported_as_opening(void **state) {
    R9OpticalLayer layers[R9_OPTICAL_COMPOSITOR_MAX_LAYERS];
    R9OpticalComposite result;
    (void)state;
    for (size_t i = 0U; i < R9_OPTICAL_COMPOSITOR_MAX_LAYERS; i++) {
        layers[i] = layer((uint8_t)('a' + i),
                          (SDL_Color){10U, 20U, 30U, 255U},
                          (SDL_Color){1U, 2U, 3U, 255U},
                          32U, 223U, false, false, false);
    }
    assert_true(r9_optical_composite(
        layers, R9_OPTICAL_COMPOSITOR_MAX_LAYERS, false, &darkness, &result));
    assert_int_equal(result.consumed_layers, R9_OPTICAL_COMPOSITOR_MAX_LAYERS);
    assert_false(result.terminated_by_surface);
    assert_false(result.reached_opening);
    assert_true(result.layer_cap_exhausted);
}

static void test_empty_and_invalid_inputs_are_transactional(void **state) {
    R9OpticalComposite result;
    R9OpticalComposite sentinel;
    (void)state;
    assert_true(r9_optical_composite(NULL, 0U, true, &darkness, &result));
    assert_memory_equal(&result.cell, &darkness, sizeof(darkness));
    assert_int_equal(result.consumed_layers, 0U);
    assert_true(result.reached_opening);
    assert_false(result.layer_cap_exhausted);
    memset(&sentinel, 0x5a, sizeof(sentinel));
    result = sentinel;
    assert_false(r9_optical_composite(NULL, 1U, false, &darkness, &result));
    assert_false(r9_optical_composite(NULL, 0U, true, NULL, &result));
    assert_false(r9_optical_composite(
        NULL, R9_OPTICAL_COMPOSITOR_MAX_LAYERS + 1U, false, &darkness, &result));
    assert_false(r9_optical_composite(NULL, 0U, true, &darkness, NULL));
    assert_memory_equal(&result, &sentinel, sizeof(result));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_single_opaque_preserves_sample_exactly),
        cmocka_unit_test(test_glass_over_wall),
        cmocka_unit_test(test_two_glass_layers_over_wall),
        cmocka_unit_test(test_glass_over_opening_uses_darkness),
        cmocka_unit_test(test_glass_adjacent_to_generated_discontinuity),
        cmocka_unit_test(test_glyph_threshold_and_absorption),
        cmocka_unit_test(test_selective_continuation_and_light_are_independent),
        cmocka_unit_test(test_transmissive_cap_is_not_reported_as_opening),
        cmocka_unit_test(test_empty_and_invalid_inputs_are_transactional)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}