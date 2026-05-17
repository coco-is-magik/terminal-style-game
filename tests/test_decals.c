#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../src/grid.h"
#include "../src/map.h"
#include "../src/camera.h"
#include "../src/raycast.h"
#include "../src/assets.h"
#include "../src/world.h"
#include "../src/decal.h"
#include "../src/lighting.h"
#include "../src/config.h"
#include "../src/asset_loader.h"

static int count_grid_glyph(Grid *g, uint8_t glyph) {
    int count = 0;

    for (int i = 0; i < g->width * g->height; i++) {
        if (g->cells[i].glyph == glyph) count++;
    }

    return count;
}

static bool find_grid_glyph(Grid *g, uint8_t glyph, int *out_x, int *out_y) {
    for (int y = 0; y < g->height; y++) {
        for (int x = 0; x < g->width; x++) {
            Cell c;
            grid_get(g, x, y, &c);
            if (c.glyph == glyph) {
                *out_x = x;
                *out_y = y;
                return true;
            }
        }
    }

    return false;
}

static void test_world_decal_add(void **state) {
    (void)state;
    WorldState world;
    world_init(&world);
    
    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_WALL;
    d.map_x = 1; d.map_y = 1; d.side = 0;
    d.pattern_cols = 4; d.pattern_rows = 1;
    d.pattern = malloc(4 * sizeof(PatternCell));
    d.pattern[0] = (PatternCell){'T', 1};
    d.pattern[1] = (PatternCell){'E', 1};
    d.pattern[2] = (PatternCell){'S', 1};
    d.pattern[3] = (PatternCell){'T', 1};
    
    world_add_decal(&world, d);
    assert_int_equal(world.num_decals, 1);
    assert_int_equal(world.decals[0].pattern[0].glyph, 'T');

    world_clear(&world);
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
    
    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_WALL;
    d.x = 2.5; d.y = 1.0; d.z = 0.5;
    d.rotation = PI / 2.0; // Normal +Y
    d.width = 1.0; d.height = 1.0;
    d.depth = 0.1;
    d.pattern_cols = 1; d.pattern_rows = 1;
    d.pattern = malloc(sizeof(PatternCell));
    d.pattern[0] = (PatternCell){'D', 1};

    world_add_decal(&world, d);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world);
    
    int dx, dy;
    assert_true(find_grid_glyph(g, 'D', &dx, &dy));
    assert_int_equal(count_grid_glyph(g, 'D'), 1);
    Cell c;
    grid_get(g, dx, dy, &c);
    assert_int_equal(c.fg.r, 30); // 255 * 0.2 * 0.6 (side 1)
    
    world_clear(&world);
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
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");

    WorldState world;
    world_init(&world);
    
    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_FLOOR;
    d.x = 3.75; d.y = 2.5; d.z = 0.0; // 1.25 units in front of camera
    d.rotation = 0.0;
    d.width = 1.0; d.height = 0.5;
    d.depth = 0.1;
    d.pattern_cols = 1; d.pattern_rows = 1;
    d.pattern = malloc(sizeof(PatternCell));
    d.pattern[0] = (PatternCell){'F', 1};

    world_add_decal(&world, d);

    // Force a dummy wall hit far away so floorcasting has a perp_dist to work with
    // Actually our raycast_render uses perp_dist from the wall hit.
    // If no hit, ray.distance is max_dist (20.0).
    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world);
    
    // Check some floor pixels. 
    // This is a bit tricky to predict exact pixel, but let's check a range.
    Cell c;
    grid_get(g, 5, 9, &c);
    assert_int_equal(c.glyph, 'F');
    assert_int_equal(c.fg.g, 51);
    
    world_clear(&world);
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
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");

    WorldState world;
    world_init(&world);
    
    // Add a very wide, thin decal on the floor exactly 2 units in front
    // This will appear as a straight horizontal line if fisheye is corrected
    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_FLOOR;
    d.x = 7.0; d.y = 5.0; d.z = 0.0;
    d.rotation = 0.0;
    d.width = 0.1; d.height = 10.0;
    d.depth = 0.1;
    d.pattern_cols = 1; d.pattern_rows = 1;
    d.pattern = malloc(sizeof(PatternCell));
    d.pattern[0] = (PatternCell){'F', 1};

    world_add_decal(&world, d);

    lighting_update(m, &world);
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

    world_clear(&world);
    map_destroy(m);
    grid_destroy(g);
}


static void test_decal_art_format(void **state) {
    (void)state;
    system("mkdir -p tests/assets_test/maps tests/assets_test/decals tests/assets_test/lights tests/assets_test/materials tests/assets_test/palettes tests/assets_test/sprites");
    
    FILE *fmap = fopen("tests/assets_test/maps/1.txt", "w");
    fprintf(fmap, "width=3\nheight=3\ndata=\n###\n###\n###\n");
    fclose(fmap);
    
    FILE *fdecal = fopen("tests/assets_test/decals/1.txt", "w");
    fprintf(fdecal, "surface=0\npattern_cols=5\npattern_rows=2\nart=\nHELLO\nWORLD\n");
    fclose(fdecal);

    WorldState world;
    world_init(&world);
    
    Map *m = asset_loader_load_map_data(&world, "tests/assets_test", 1);
    assert_non_null(m);
    assert_int_equal(world.num_decals, 1);
    assert_int_equal(world.decals[0].pattern_cols, 5);
    assert_int_equal(world.decals[0].pattern_rows, 2);
    assert_int_equal(world.decals[0].pattern[0].glyph, 'H');
    assert_int_equal(world.decals[0].pattern[4].glyph, 'O');
    assert_int_equal(world.decals[0].pattern[5].glyph, 'W');
    assert_int_equal(world.decals[0].pattern[9].glyph, 'D');
    
    world_clear(&world);
    map_destroy(m);
    system("rm -rf tests/assets_test");
}

static void test_decal_art_format_failure(void **state) {
    (void)state;
    system("mkdir -p tests/assets_test/maps tests/assets_test/decals tests/assets_test/lights tests/assets_test/materials tests/assets_test/palettes tests/assets_test/sprites");
    
    FILE *fmap = fopen("tests/assets_test/maps/1.txt", "w");
    fprintf(fmap, "width=3\nheight=3\ndata=\n###\n###\n###\n");
    fclose(fmap);
    
    FILE *fdecal = fopen("tests/assets_test/decals/1.txt", "w");
    fprintf(fdecal, "surface=0\npattern_cols=5\npattern_rows=2\nart=\nHEL\nWOR\n");
    fclose(fdecal);

    WorldState world;
    world_init(&world);
    
    Map *m = asset_loader_load_map_data(&world, "tests/assets_test", 1);
    assert_non_null(m);
    assert_int_equal(world.num_decals, 1);
    assert_int_equal(world.decals[0].pattern_cols, 5);
    assert_int_equal(world.decals[0].pattern_rows, 2);
    assert_int_equal(world.decals[0].pattern[0].glyph, '!');
    
    world_clear(&world);
    map_destroy(m);
    system("rm -rf tests/assets_test");
}

static void test_decal_floor_continuous_sampling(void **state) {
    (void)state;
    Grid *g = grid_create(20, 20);
    Map *m = map_create(5, 5);
    
    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2.0); // Looking +X
    
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");

    WorldState world;
    world_init(&world);
    
    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_FLOOR;
    d.x = 4.0; d.y = 2.5; d.z = 0.0;
    d.rotation = 0.0;
    d.width = 1.0; d.height = 1.0;
    d.depth = 0.1;
    d.pattern_cols = 2; d.pattern_rows = 2;
    d.pattern = malloc(4 * sizeof(PatternCell));
    d.pattern[0] = (PatternCell){'A', 1}; d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1}; d.pattern[3] = (PatternCell){'D', 1};

    world_add_decal(&world, d);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world);
    
    int count_A = count_grid_glyph(g, 'A');
    int count_B = count_grid_glyph(g, 'B');
    int count_C = count_grid_glyph(g, 'C');
    int count_D = count_grid_glyph(g, 'D');
    int total = count_A + count_B + count_C + count_D;
    
    // Floor decals should be sampled from surface-local UVs at every rendered
    // floor point, not projected as one isolated screen glyph per source cell.
    assert_int_equal(count_A, 1);
    assert_int_equal(count_B, 1);
    assert_int_equal(count_C, 1);
    assert_int_equal(count_D, 1);
    assert_int_equal(total, d.pattern_cols * d.pattern_rows);

    world_clear(&world);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_ceiling_continuous_sampling(void **state) {
    (void)state;
    Grid *g = grid_create(20, 20);
    Map *m = map_create(5, 5);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2.0); // Looking +X

    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");

    WorldState world;
    world_init(&world);

    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_CEILING;
    d.x = 4.0; d.y = 2.5; d.z = 0.0; // world_add_decal moves ceiling decals to z=1
    d.rotation = 0.0;
    d.width = 1.0; d.height = 1.0;
    d.depth = 0.1;
    d.pattern_cols = 2; d.pattern_rows = 2;
    d.pattern = malloc(4 * sizeof(PatternCell));
    d.pattern[0] = (PatternCell){'A', 1}; d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1}; d.pattern[3] = (PatternCell){'D', 1};

    world_add_decal(&world, d);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world);

    int count_A = count_grid_glyph(g, 'A');
    int count_B = count_grid_glyph(g, 'B');
    int count_C = count_grid_glyph(g, 'C');
    int count_D = count_grid_glyph(g, 'D');
    int total = count_A + count_B + count_C + count_D;

    assert_int_equal(count_A, 1);
    assert_int_equal(count_B, 1);
    assert_int_equal(count_C, 1);
    assert_int_equal(count_D, 1);
    assert_int_equal(total, d.pattern_cols * d.pattern_rows);

    world_clear(&world);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_wall_authoritative_dimensions(void **state) {
    (void)state;
    Grid *g = grid_create(80, 80);
    Map *m = map_create(8, 5);
    map_set(m, 4, 2, 1);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2.0);

    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");

    WorldState world;
    world_init(&world);

    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_WALL;
    d.x = 4.0; d.y = 2.5; d.z = 0.5;
    d.rotation = PI;
    d.width = 1.0; d.height = 1.0;
    d.depth = 0.1;
    d.pattern_cols = 4; d.pattern_rows = 1;
    d.pattern = malloc(4 * sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'A', 1};
    d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1};
    d.pattern[3] = (PatternCell){'D', 1};

    world_add_decal(&world, d);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world);

    int ax, ay, bx, by, cx, cy, dx, dy;
    assert_true(find_grid_glyph(g, 'A', &ax, &ay));
    assert_true(find_grid_glyph(g, 'B', &bx, &by));
    assert_true(find_grid_glyph(g, 'C', &cx, &cy));
    assert_true(find_grid_glyph(g, 'D', &dx, &dy));
    assert_int_equal(count_grid_glyph(g, 'A'), 1);
    assert_int_equal(count_grid_glyph(g, 'B'), 1);
    assert_int_equal(count_grid_glyph(g, 'C'), 1);
    assert_int_equal(count_grid_glyph(g, 'D'), 1);

    assert_true(ay == by && by == cy && cy == dy);
    assert_true((ax < bx && bx < cx && cx < dx) ||
                (dx < cx && cx < bx && bx < ax));

    world_clear(&world);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_wall_backface_rejected(void **state) {
    (void)state;
    Grid *g = grid_create(20, 20);
    Map *m = map_create(5, 5);
    map_set(m, 3, 2, 1);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2.0);

    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");

    WorldState world;
    world_init(&world);

    Decal back;
    memset(&back, 0, sizeof(Decal));
    back.surface = DECAL_SURFACE_WALL;
    back.x = 3.0; back.y = 2.5; back.z = 0.5;
    back.rotation = 0.0;
    back.width = 1.0; back.height = 1.0;
    back.depth = 0.1;
    back.pattern_cols = 1; back.pattern_rows = 1;
    back.pattern = malloc(sizeof(PatternCell));
    assert_non_null(back.pattern);
    back.pattern[0] = (PatternCell){'B', 1};
    world_add_decal(&world, back);

    Decal front;
    memset(&front, 0, sizeof(Decal));
    front.surface = DECAL_SURFACE_WALL;
    front.x = 3.0; front.y = 2.5; front.z = 0.5;
    front.rotation = PI;
    front.width = 1.0; front.height = 1.0;
    front.depth = 0.1;
    front.pattern_cols = 1; front.pattern_rows = 1;
    front.pattern = malloc(sizeof(PatternCell));
    assert_non_null(front.pattern);
    front.pattern[0] = (PatternCell){'F', 1};
    world_add_decal(&world, front);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world);

    Cell c;
    grid_get(g, 10, 10, &c);
    assert_int_equal(c.glyph, 'F');
    assert_int_equal(count_grid_glyph(g, 'B'), 0);

    world_clear(&world);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_wall_orientation(void **state) {
    (void)state;
    Grid *g = grid_create(10, 20);
    Map *m = map_create(5, 5);
    map_set(m, 3, 2, 1); // Wall in front of camera
    
    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2.0); // Looking +X at wall at x=3
    
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");

    WorldState world;
    world_init(&world);
    
    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_WALL;
    d.x = 3.0; d.y = 2.5; d.z = 0.5;
    d.rotation = PI; // Face -X
    d.width = 1.0; d.height = 0.8;
    d.depth = 0.1;
    d.pattern_cols = 1; d.pattern_rows = 2;
    d.pattern = malloc(2 * sizeof(PatternCell));
    d.pattern[0] = (PatternCell){'T', 1}; // Top
    d.pattern[1] = (PatternCell){'B', 1}; // Bottom

    world_add_decal(&world, d);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world);
    
    int t_x = -1, t_y = -1, b_x = -1, b_y = -1;
    for(int y=0; y<g->height; y++) {
        Cell c;
        grid_get(g, 5, y, &c);
        if(c.glyph == 'T') t_y = y;
        if(c.glyph == 'B') b_y = y;
    }

    if (t_y == -1) assert_true(find_grid_glyph(g, 'T', &t_x, &t_y));
    if (b_y == -1) assert_true(find_grid_glyph(g, 'B', &b_x, &b_y));
    
    assert_true(t_y != -1);
    assert_true(b_y != -1);
    assert_true(t_y < b_y); // T should be above B (lower Y value in screen space)

    world_clear(&world);
    map_destroy(m);
    grid_destroy(g);
}

int main(void) {

    config_init_defaults();
    
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_world_decal_add),
        cmocka_unit_test(test_decal_rendering_wall),
        cmocka_unit_test(test_decal_rendering_floor),
        cmocka_unit_test(test_decal_fisheye_correction),
        cmocka_unit_test(test_decal_art_format),
        cmocka_unit_test(test_decal_art_format_failure),
        cmocka_unit_test(test_decal_floor_continuous_sampling),
        cmocka_unit_test(test_decal_ceiling_continuous_sampling),
        cmocka_unit_test(test_decal_wall_authoritative_dimensions),
        cmocka_unit_test(test_decal_wall_backface_rejected),
        cmocka_unit_test(test_decal_wall_orientation),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}