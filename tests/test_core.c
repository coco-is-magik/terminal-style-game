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
#include "../src/math.h"
#include "../src/map.h"
#include "../src/camera.h"
#include "../src/raycast.h"
#include "../src/lighting.h"
#include "../src/config.h"

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
    grid_destroy(NULL);
}

static void test_grid_set_get_bounds(void **state) {
    (void)state;
    Grid *g = grid_create(5, 5);
    SDL_Color fg = {255, 0, 0, 255};
    SDL_Color bg = {0, 255, 0, 255};
    
    assert_true(grid_set(g, 2, 2, 'A', fg, bg));
    
    Cell c;
    assert_true(grid_get(g, 2, 2, &c));
    assert_int_equal(c.glyph, 'A');
    assert_int_equal(c.fg.r, 255);
    assert_int_equal(c.bg.g, 255);
    
    assert_false(grid_set(g, -1, 0, 'X', fg, bg));
    assert_false(grid_set(g, 5, 0, 'X', fg, bg));
    
    assert_false(grid_get(g, -1, 0, &c));
    
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
    
    grid_destroy(g);
}

// --- SCALE TESTS ---

static void test_scale_calculations(void **state) {
    (void)state;
    ScaleResult r1 = scale_calculate(1920, 1080, 240, 160, 8, 8);
    assert_true(r1.valid);
    assert_int_equal(r1.scale_factor, 1);
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
    assert_float_equal(timing_spare_ms(5.0, 16.666), 11.666, DOUBLE_EPSILON);
}

static void test_perf_stats(void **state) {
    (void)state;
    PerfStats stats;
    perf_stats_init(&stats);
    
    for (int i = 0; i < 10; i++) {
        perf_stats_update(&stats, 8.0, 8.0, 0.33);
    }
    
    assert_float_equal(stats.elapsed_time_ms, 80.0, DOUBLE_EPSILON);
    
    perf_stats_update(&stats, 920.0, 920.0, -910.0);
    assert_float_equal(stats.pub_avg_fps, 11.0, DOUBLE_EPSILON);
}

// --- RENDERER BACKEND TESTS ---

static void test_renderer_backend_init_invalid(void **state) {
    (void)state;
    Renderer *ren = renderer_create(1280, 720, -1, 160, 8, 8);
    assert_null(ren);
}

static void test_renderer_backend_instrumentation(void **state) {
    (void)state;
    renderer_destroy(NULL);
}

// --- ENGINE REFACTOR TESTS ---

#include "../src/asset_loader.h"

static void test_asset_loader(void **state) {
    (void)state;
    AssetRegistry assets;
    asset_registry_init(&assets);
    
    // Test loading palettes and materials
    asset_loader_load_registry(&assets, "assets");
    
    // Verify palette 1 was loaded
    SDL_Color near_col = palette_sample(&assets.palettes[1], 1.0, 1.0);
    assert_int_equal(near_col.r, 255);
    
    // Verify material 1 was loaded
    assert_int_equal(assets.materials[1].palette_id, 1);
    assert_int_equal(assets.materials[1].glyphs[0], '#');
    
    WorldState world;
    world_init(&world);
    
    Map *map = asset_loader_load_map_data(&world, "assets", 1);
    assert_non_null(map);
    assert_int_equal(map->width, 10);
    assert_int_equal(map->height, 6);
    
    // Verify decals and lights
    assert_true(world.num_decals > 0);
    assert_true(world.num_lights > 0);
    
    map_destroy(map);
}

static void test_config_parsing(void **state) {
    (void)state;
    // We already called config_init_defaults() in main()
    // It should have grid_width 260
    assert_int_equal(config_get()->grid_width, 260);
    
    // Check loading the real config file
    assert_true(config_load_from_file("config.ini"));
    assert_int_equal(config_get()->target_fps, 120);
    assert_float_equal(config_get()->ambient_light, 0.2, DOUBLE_EPSILON);
}

static void test_math_normalize(void **state) {
    (void)state;
    assert_float_equal(normalize_angle(0.0), 0.0, DOUBLE_EPSILON);
    assert_float_equal(normalize_angle(3 * PI), PI, DOUBLE_EPSILON);
    assert_float_equal(normalize_angle(-PI), PI, DOUBLE_EPSILON);
}

static void test_map_creation(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    assert_non_null(m);
    assert_int_equal(m->width, 5);
    assert_int_equal(m->height, 5);
    map_destroy(m);
}

static void test_map_bounds(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    assert_true(map_in_bounds(m, 0, 0));
    assert_true(map_in_bounds(m, 4, 4));
    assert_false(map_in_bounds(m, -1, 0));
    assert_false(map_in_bounds(m, 5, 5));
    map_destroy(m);
}

static void test_map_lookup(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    map_set(m, 2, 2, 1);
    assert_int_equal(map_get(m, 2, 2)->material_id, 1);
    assert_int_equal(map_get(m, 0, 0)->material_id, 0);
    map_destroy(m);
}

static void test_camera_init(void **state) {
    (void)state;
    Camera cam;
    camera_init(&cam, 2.5, 3.5, PI, PI/2);
    assert_float_equal(cam.transform.pos.x, 2.5, DOUBLE_EPSILON);
    assert_float_equal(cam.transform.pos.y, 3.5, DOUBLE_EPSILON);
    assert_float_equal(cam.transform.angle, PI, DOUBLE_EPSILON);
    assert_float_equal(cam.fov, PI/2, DOUBLE_EPSILON);
}

static void test_raycast_hit(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    map_set(m, 4, 2, 1); // Wall directly to the right
    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2); // facing right (angle 0)
    
    RayResult res = raycast_fire(m, &cam, 0.0, 10.0);
    assert_true(res.hit);
    assert_int_equal(res.map_x, 4);
    assert_int_equal(res.map_y, 2);
    assert_float_equal(res.distance, 1.5, DOUBLE_EPSILON); // 4.0 - 2.5
    
    map_destroy(m);
}

static void test_raycast_miss(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2);
    
    RayResult res = raycast_fire(m, &cam, 0.0, 10.0);
    assert_false(res.hit);
    assert_float_equal(res.distance, 10.0, DOUBLE_EPSILON);
    
    map_destroy(m);
}

static void test_raycast_perpendicular_correction(void **state) {
    (void)state;
    Map *m = map_create(10, 10);
    // Wall at x = 5
    for(int y=0; y<10; y++) map_set(m, 5, y, 1);
    
    Camera cam;
    camera_init(&cam, 2.5, 5.5, 0.0, PI/2.0); // Facing right
    
    // Center ray
    double center_angle = 0.0;
    RayResult res_center = raycast_fire(m, &cam, center_angle, 10.0);
    assert_true(res_center.hit);
    double perp_dist_center = res_center.distance * cos(center_angle - cam.transform.angle);
    
    // Edge ray
    double edge_angle = atan(1.0 * tan(cam.fov / 2.0));
    RayResult res_edge = raycast_fire(m, &cam, edge_angle, 10.0);
    assert_true(res_edge.hit);
    double perp_dist_edge = res_edge.distance * cos(edge_angle - cam.transform.angle);
    
    // Perpendicular distance to a straight wall should be identical
    assert_float_equal(perp_dist_center, perp_dist_edge, DOUBLE_EPSILON);
    
    map_destroy(m);
}

static void test_raycast_near_plane_clipping(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    map_set(m, 2, 2, 1);
    Camera cam;
    camera_init(&cam, 1.99, 2.5, 0.0, PI/2); // Very close to the wall at x=2
    
    RayResult res = raycast_fire(m, &cam, 0.0, 10.0);
    assert_true(res.hit);
    assert_float_equal(res.distance, 0.01, DOUBLE_EPSILON);
    
    // Simulate render logic clip clamp
    double perp_dist = res.distance * cos(0.0);
    if (perp_dist < 0.001) perp_dist = 0.001; // Not clamped yet
    int line_height = (int)(10 / perp_dist);
    assert_true(line_height >= 999 && line_height <= 1000); // 10 / 0.01

    // Now push it so close it clamps
    cam.transform.pos.x = 1.99999;
    res = raycast_fire(m, &cam, 0.0, 10.0);
    assert_true(res.hit);
    perp_dist = res.distance * cos(0.0);
    if (perp_dist < 0.001) perp_dist = 0.001; // Clamps!
    line_height = (int)(10 / perp_dist);
    assert_int_equal(line_height, 10000); // 10 / 0.001
    
    map_destroy(m);
}

static void test_raycast_render_output(void **state) {
    (void)state;
    Grid *g = grid_create(10, 10);
    Map *m = map_create(5, 5);
    for(int x=0; x<5; x++) map_set(m, x, 0, 1); // front wall at y=0
    Camera cam;
    camera_init(&cam, 2.5, 2.5, -PI/2, PI/2); // facing up (-y direction)
    
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "#x-.");

    WorldState world;
    world_init(&world);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world);
    
    Cell c;
    // Check ceiling (top row)
    grid_get(g, 5, 0, &c);
    assert_int_equal(c.glyph, ' ');
    assert_int_equal(c.bg.r, 10); // 50 * 0.2 ambient
    
    // Check floor (bottom row)
    grid_get(g, 5, 9, &c);
    assert_int_equal(c.glyph, ' ');
    assert_int_equal(c.bg.r, 6); // 30 * 0.2 ambient
    
    // Check wall (middle)
    grid_get(g, 5, 5, &c);
    assert_true(c.glyph == '#' || c.glyph == 'x' || c.glyph == '+' || c.glyph == '-' || c.glyph == '.');
    assert_int_equal(c.bg.r, 0); // Wall bg is solid black
    
    map_destroy(m);
    grid_destroy(g);
}

int main(void) {
    config_init_defaults();
    
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
        // NEW ENGINE REFACTOR & RAYCAST TESTS
        cmocka_unit_test(test_asset_loader),
        cmocka_unit_test(test_config_parsing),
        cmocka_unit_test(test_math_normalize),
        cmocka_unit_test(test_map_creation),
        cmocka_unit_test(test_map_bounds),
        cmocka_unit_test(test_map_lookup),
        cmocka_unit_test(test_camera_init),
        cmocka_unit_test(test_raycast_hit),
        cmocka_unit_test(test_raycast_miss),
        cmocka_unit_test(test_raycast_perpendicular_correction),
        cmocka_unit_test(test_raycast_near_plane_clipping),
        cmocka_unit_test(test_raycast_render_output),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
