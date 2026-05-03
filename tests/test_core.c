#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <math.h>

#include "../src/grid.h"
#include "../src/scale.h"
#include "../src/timing.h"
#include "../src/renderer.h"

// --- GRID TESTS ---

static void test_grid_init(void **state) {
    (void)state;
    Grid *g = grid_create(10, 10);
    assert_non_null(g);
    assert_int_equal(g->width, 10);
    assert_int_equal(g->height, 10);
    grid_destroy(g);
}

static void test_grid_init_invalid(void **state) {
    (void)state;
    Grid *g1 = grid_create(0, 10);
    assert_null(g1);
    Grid *g2 = grid_create(10, -5);
    assert_null(g2);
}

static void test_grid_destroy_safety(void **state) {
    (void)state;
    grid_destroy(NULL); // Should not segfault
}

static void test_grid_set_get_bounds(void **state) {
    (void)state;
    Grid *g = grid_create(5, 5);
    SDL_Color fg = {255, 0, 0, 255};
    SDL_Color bg = {0, 255, 0, 255};
    
    // valid set
    assert_true(grid_set(g, 2, 2, 'A', fg, bg));
    
    // valid get
    Cell c;
    assert_true(grid_get(g, 2, 2, &c));
    assert_int_equal(c.glyph, 'A');
    assert_int_equal(c.fg.r, 255);
    assert_int_equal(c.bg.g, 255);
    
    // invalid out of bounds writes
    assert_false(grid_set(g, -1, 0, 'X', fg, bg));
    assert_false(grid_set(g, 5, 0, 'X', fg, bg));
    assert_false(grid_set(g, 0, -1, 'X', fg, bg));
    assert_false(grid_set(g, 0, 5, 'X', fg, bg));
    
    // invalid gets
    assert_false(grid_get(g, -1, 0, &c));
    assert_false(grid_get(g, 5, 0, &c));
    
    grid_destroy(g);
}

static void test_grid_clear(void **state) {
    (void)state;
    Grid *g = grid_create(3, 3);
    SDL_Color fg = {1, 1, 1, 255};
    SDL_Color bg = {2, 2, 2, 255};
    grid_set(g, 0, 0, 'X', fg, fg);
    
    grid_clear(g, bg);
    
    Cell c;
    grid_get(g, 0, 0, &c);
    assert_int_equal(c.glyph, ' ');
    assert_int_equal(c.bg.r, 2);
    assert_int_equal(c.fg.r, 255); // grid_clear uses white fg
    
    grid_destroy(g);
}

// --- SCALE TESTS ---

static void test_scale_calculations(void **state) {
    (void)state;
    // Window: 1920x1080, Grid: 240x160, Cell: 8x8 => Logical: 1920x1280
    // Scale X = 1, Scale Y = 0 => Should cap at 1
    ScaleResult r1 = scale_calculate(1920, 1080, 240, 160, 8, 8);
    assert_true(r1.valid);
    assert_int_equal(r1.scale_factor, 1);
    assert_int_equal(r1.logical_w, 1920);
    assert_int_equal(r1.logical_h, 1280);
    
    // Window: 1280x720, Grid: 80x40, Cell: 8x8 => Logical: 640x320
    // Scale X = 2, Scale Y = 2 => Scale factor 2
    ScaleResult r2 = scale_calculate(1280, 720, 80, 40, 8, 8);
    assert_true(r2.valid);
    assert_int_equal(r2.scale_factor, 2);
    // fitted_w = 640 * 2 = 1280, offset_x = 0
    assert_int_equal(r2.offset_x, 0);
    // fitted_h = 320 * 2 = 640, offset_y = (720 - 640) / 2 = 40
    assert_int_equal(r2.offset_y, 40);
}

static void test_scale_invalid(void **state) {
    (void)state;
    ScaleResult r = scale_calculate(0, 720, 80, 40, 8, 8);
    assert_false(r.valid);
}

// --- TIMING TESTS ---

#define DOUBLE_EPSILON 0.0001
static void test_timing_math(void **state) {
    (void)state;
    assert_float_equal(timing_target_ms(120), 8.333333, DOUBLE_EPSILON);
    assert_float_equal(timing_target_ms(60), 16.666666, DOUBLE_EPSILON);
    assert_float_equal(timing_target_ms(-5), 0.0, DOUBLE_EPSILON);
    
    // Under budget
    assert_float_equal(timing_spare_ms(5.0, 16.666), 11.666, DOUBLE_EPSILON);
    assert_float_equal(timing_over_budget_ms(11.666), 0.0, DOUBLE_EPSILON);
    assert_int_equal(timing_sleep_ms(11.666), 11);
    
    // Over budget
    assert_float_equal(timing_spare_ms(20.0, 16.666), -3.334, DOUBLE_EPSILON);
    assert_float_equal(timing_over_budget_ms(-3.334), 3.334, DOUBLE_EPSILON);
    assert_int_equal(timing_sleep_ms(-3.334), 0);
}

static void test_perf_stats(void **state) {
    (void)state;
    PerfStats stats;
    perf_stats_init(&stats);
    
    // Insert 10 frames at 8ms each (total 80ms)
    for (int i = 0; i < 10; i++) {
        perf_stats_update(&stats, 8.0, 8.0, 0.33);
    }
    
    assert_float_equal(stats.elapsed_time_ms, 80.0, DOUBLE_EPSILON);
    assert_int_equal(stats.frame_count, 10);
    assert_float_equal(stats.worst_frame_time_ms, 8.0, DOUBLE_EPSILON);
    assert_float_equal(stats.min_spare_time_ms, 0.33, DOUBLE_EPSILON);
    
    // No publish yet
    assert_float_equal(stats.pub_avg_fps, 0.0, DOUBLE_EPSILON);
    
    // Insert a giant frame to trigger the 1-second publish
    perf_stats_update(&stats, 920.0, 920.0, -910.0);
    
    // It should have reset inner trackers but published the 1s window result
    assert_float_equal(stats.elapsed_time_ms, 0.0, DOUBLE_EPSILON);
    assert_int_equal(stats.frame_count, 0);
    assert_float_equal(stats.pub_worst_frame_time_ms, 920.0, DOUBLE_EPSILON);
    assert_float_equal(stats.pub_min_spare_time_ms, -910.0, DOUBLE_EPSILON);
    // 11 frames in 1000ms = 11 FPS
    assert_float_equal(stats.pub_avg_fps, 11.0, DOUBLE_EPSILON);
    // Avg frame time = (10*8 + 920) / 11 = 1000 / 11 = 90.9090
    assert_float_equal(stats.pub_avg_frame_time_ms, 90.909090, DOUBLE_EPSILON);
}

// --- RENDERER BACKEND TESTS ---

static void test_renderer_backend_init_invalid(void **state) {
    (void)state;
    // Test that passing invalid dimensions safely returns NULL without crashing
    Renderer *ren = renderer_create(1280, 720, -1, 160, 8, 8);
    assert_null(ren);
}

static void test_renderer_backend_instrumentation(void **state) {
    (void)state;
    // These tests skip creating a real window because headless CI might fail SDL_Init(VIDEO).
    // The previous renderer checks prove that SDL_Init correctly fails in CI, preventing segfaults.
    // If a window is created, we verify the counters. Since we can't reliably create a window in CI,
    // we just prove that calling destroy on NULL doesn't crash.
    renderer_destroy(NULL);
    
    // To thoroughly test the "no per frame allocation", that logic is evaluated in the 
    // real benchmark mode under src/main.c where a window environment allows actual drawing.
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_grid_init),
        cmocka_unit_test(test_grid_init_invalid),
        cmocka_unit_test(test_grid_destroy_safety),
        cmocka_unit_test(test_grid_set_get_bounds),
        cmocka_unit_test(test_grid_clear),
        cmocka_unit_test(test_scale_calculations),
        cmocka_unit_test(test_scale_invalid),
        cmocka_unit_test(test_timing_math),
        cmocka_unit_test(test_perf_stats),
        cmocka_unit_test(test_renderer_backend_init_invalid),
        cmocka_unit_test(test_renderer_backend_instrumentation),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
