#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <math.h>
#include <string.h>

#include "../src/grid.h"
#include "../src/map.h"
#include "../src/camera.h"
#include "../src/raycast.h"
#include "../src/assets.h"
#include "../src/world.h"
#include "../src/decal.h"

static void test_world_decal_add(void **state) {
    (void)state;
    WorldState world;
    world_init(&world);
    
    Decal d = {
        .surface = DECAL_SURFACE_WALL,
        .map_x = 1, .map_y = 1, .side = 0,
        .text = "TEST",
        .fg = {255, 0, 0, 255}
    };
    
    world_add_decal(&world, d);
    assert_int_equal(world.num_decals, 1);
    assert_string_equal(world.decals[0].text, "TEST");
}

static void test_decal_rendering_wall(void **state) {
    (void)state;
    Grid *g = grid_create(10, 10);
    Map *m = map_create(5, 5);
    map_set(m, 2, 0, 1); // Wall at (2,0)
    
    Camera cam;
    camera_init(&cam, 2.5, 2.5, -PI/2.0, PI/2.0); // Looking at the wall at (2,0)
    
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");

    WorldState world;
    world_init(&world);
    
    Decal d = {
        .surface = DECAL_SURFACE_WALL,
        .map_x = 2, .map_y = 0, .side = 1,
        .u = 0.0, .v = 0.0,
        .width = 1.0, .height = 1.0,
        .text = "D",
        .fg = {255, 0, 0, 255},
        .use_bg = true, .bg = {0, 255, 0, 255}
    };
    world_add_decal(&world, d);

    raycast_render(g, m, &cam, &assets, &world);
    
    // The decal covers the whole wall, so we expect 'D' in the middle of the wall
    Cell c;
    grid_get(g, 5, 5, &c);
    assert_int_equal(c.glyph, 'D');
    assert_int_equal(c.fg.r, 255);
    assert_int_equal(c.bg.g, 255);
    
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_rendering_floor(void **state) {
    (void)state;
    Grid *g = grid_create(10, 10);
    Map *m = map_create(5, 5);
    // No walls needed for floor
    
    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2.0);
    
    AssetRegistry assets;
    asset_registry_init(&assets);

    WorldState world;
    world_init(&world);
    
    Decal d = {
        .surface = DECAL_SURFACE_FLOOR,
        .x = 3.5, .y = 2.5, // 1 unit in front of camera
        .width = 1.0, .height = 1.0,
        .text = "F",
        .fg = {0, 255, 0, 255},
        .use_bg = true, .bg = {0, 0, 255, 255}
    };
    world_add_decal(&world, d);

    // Force a dummy wall hit far away so floorcasting has a perp_dist to work with
    // Actually our raycast_render uses perp_dist from the wall hit.
    // If no hit, ray.distance is max_dist (20.0).
    raycast_render(g, m, &cam, &assets, &world);
    
    // Check some floor pixels. 
    // This is a bit tricky to predict exact pixel, but let's check a range.
    bool found = false;
    for (int y = 5; y < 10; y++) {
        for (int x = 0; x < 10; x++) {
            Cell c;
            grid_get(g, x, y, &c);
            if (c.glyph == 'F') {
                found = true;
                assert_int_equal(c.fg.g, 255);
                assert_int_equal(c.bg.b, 255);
            }
        }
    }
    assert_true(found);
    
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_fisheye_correction(void **state) {
    (void)state;
    Grid *g = grid_create(100, 100);
    Map *m = map_create(10, 10);
    
    Camera cam;
    // Looking along X axis
    camera_init(&cam, 5.0, 5.0, 0.0, PI/2.0);
    
    AssetRegistry assets;
    asset_registry_init(&assets);

    WorldState world;
    world_init(&world);
    
    // Add a very wide, thin decal on the floor exactly 2 units in front
    // This will appear as a straight horizontal line if fisheye is corrected
    Decal d = {
        .surface = DECAL_SURFACE_FLOOR,
        .x = 7.0, .y = 5.0,
        .width = 0.1, .height = 10.0,
        .text = "F",
        .fg = {255, 255, 255, 255},
        .use_bg = false
    };
    world_add_decal(&world, d);

    raycast_render(g, m, &cam, &assets, &world);
    
    // Scan the grid to find the row where 'F' is rendered.
    // Due to fisheye correction, it should be rendered on a single horizontal line.
    int line_y = -1;
    bool is_straight = true;
    for (int x = 0; x < g->width; x++) {
        for (int y = g->height / 2; y < g->height; y++) {
            Cell c;
            grid_get(g, x, y, &c);
            if (c.glyph == 'F') {
                if (line_y == -1) {
                    line_y = y;
                } else if (line_y != y) {
                    // Warped!
                    is_straight = false;
                }
            }
        }
    }
    
    // We should have found it, and it should be straight.
    assert_true(line_y != -1);
    assert_true(is_straight);

    map_destroy(m);
    grid_destroy(g);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_world_decal_add),
        cmocka_unit_test(test_decal_rendering_wall),
        cmocka_unit_test(test_decal_rendering_floor),
        cmocka_unit_test(test_decal_fisheye_correction),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
