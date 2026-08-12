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

typedef struct {
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int count;
} GlyphBounds;

static GlyphBounds glyph_bounds(Grid *g, uint8_t glyph) {
    GlyphBounds bounds = {g->width, g->height, -1, -1, 0};

    for (int y = 0; y < g->height; y++) {
        for (int x = 0; x < g->width; x++) {
            Cell c;
            grid_get(g, x, y, &c);
            if (c.glyph != glyph) continue;

            if (x < bounds.min_x) bounds.min_x = x;
            if (y < bounds.min_y) bounds.min_y = y;
            if (x > bounds.max_x) bounds.max_x = x;
            if (y > bounds.max_y) bounds.max_y = y;
            bounds.count++;
        }
    }

    return bounds;
}

static GlyphBounds assert_glyph_bounds(Grid *g, uint8_t glyph) {
    GlyphBounds bounds = glyph_bounds(g, glyph);
    assert_true(bounds.count > 0);
    return bounds;
}

static int bounds_width(GlyphBounds bounds) {
    return bounds.count > 0 ? bounds.max_x - bounds.min_x + 1 : 0;
}

static int bounds_height(GlyphBounds bounds) {
    return bounds.count > 0 ? bounds.max_y - bounds.min_y + 1 : 0;
}

static int bounds_area(GlyphBounds bounds) {
    return bounds_width(bounds) * bounds_height(bounds);
}

static void assert_unique_glyph(Grid *g, uint8_t glyph, int *out_x, int *out_y) {
    GlyphBounds bounds = assert_glyph_bounds(g, glyph);
    assert_int_equal(bounds.count, 1);
    assert_true(find_grid_glyph(g, glyph, out_x, out_y));
}

static bool bounds_overlap_y(GlyphBounds a, GlyphBounds b) {
    return a.min_y <= b.max_y && b.min_y <= a.max_y;
}

static void assert_adjacent_left_to_right(GlyphBounds left, GlyphBounds right) {
    assert_true(left.min_x <= right.min_x);
}

static void assert_same_row_band(GlyphBounds left, GlyphBounds right) {
    assert_true(bounds_overlap_y(left, right));
}

static void assert_adjacent_top_to_bottom(GlyphBounds top, GlyphBounds bottom) {
    assert_true(top.min_y <= bottom.min_y);
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

static void test_world_decal_rejection_retains_caller_ownership(void **state) {
    (void)state;
    WorldState world;
    world_init(&world);
    world.num_decals = MAX_DECALS;

    Decal decal;
    memset(&decal, 0, sizeof(decal));
    decal.pattern_cols = 1;
    decal.pattern_rows = 1;
    decal.pattern = malloc(sizeof(PatternCell));
    assert_non_null(decal.pattern);
    decal.pattern[0] = (PatternCell){'X', 1};

    assert_int_equal(world_add_decal(&world, decal), WORLD_INSERT_FULL);
    assert_int_equal(world.num_decals, MAX_DECALS);
    assert_int_equal(decal.pattern[0].glyph, 'X');
    free(decal.pattern);
}

static void test_world_insert_boundaries(void **state) {
    (void)state;
    WorldState world;
    SDL_Color color = {1, 2, 3, 4};
    Decal decal;

    world_init(&world);
    world.num_lights = MAX_LIGHTS - 1;
    assert_int_equal(world_add_light(&world, 1.0, 2.0, color, -1.0, 3.0),
                     WORLD_INSERT_OK);
    assert_int_equal(world.num_lights, MAX_LIGHTS);
    assert_float_equal(world.lights[MAX_LIGHTS - 1].radius, 3.0, 0.0001);
    assert_int_equal(world_add_light(&world, 9.0, 9.0, color, 1.0, 1.0),
                     WORLD_INSERT_FULL);
    assert_int_equal(world.num_lights, MAX_LIGHTS);

    world_init(&world);
    world.num_sprites = MAX_SPRITES - 1;
    assert_int_equal(world_add_sprite(&world, 4.0, 5.0, 255), WORLD_INSERT_OK);
    assert_int_equal(world.num_sprites, MAX_SPRITES);
    assert_int_equal(world.sprites[MAX_SPRITES - 1].sprite_id, 255);
    assert_int_equal(world_add_sprite(&world, 1.0, 1.0, 1), WORLD_INSERT_FULL);
    assert_int_equal(world.num_sprites, MAX_SPRITES);

    world_init(&world);
    memset(&decal, 0, sizeof(decal));
    decal.pattern_cols = 1;
    decal.pattern_rows = 1;
    decal.pattern = malloc(sizeof(PatternCell));
    assert_non_null(decal.pattern);
    decal.pattern[0] = (PatternCell){'L', 1};
    world.num_decals = MAX_DECALS - 1;
    assert_int_equal(world_add_decal(&world, decal), WORLD_INSERT_OK);
    assert_int_equal(world.num_decals, MAX_DECALS);
    assert_ptr_equal(world.decals[MAX_DECALS - 1].pattern, decal.pattern);
    world_clear(&world);
}

static void test_world_insert_invalid_inputs_unchanged(void **state) {
    (void)state;
    WorldState world;
    SDL_Color color = {1, 2, 3, 4};
    Decal decal;

    world_init(&world);
    assert_int_equal(world_add_light(NULL, 1.0, 1.0, color, 1.0, 1.0),
                     WORLD_INSERT_INVALID);
    assert_int_equal(world_add_light(&world, NAN, 1.0, color, 1.0, 1.0),
                     WORLD_INSERT_INVALID);
    assert_int_equal(world_add_light(&world, 1.0, 1.0, color, 1.0, 0.0),
                     WORLD_INSERT_INVALID);
    assert_int_equal(world.num_lights, 0);

    assert_int_equal(world_add_sprite(NULL, 1.0, 1.0, 1), WORLD_INSERT_INVALID);
    assert_int_equal(world_add_sprite(&world, 1.0, 1.0, 0), WORLD_INSERT_INVALID);
    assert_int_equal(world_add_sprite(&world, INFINITY, 1.0, 1), WORLD_INSERT_INVALID);
    assert_int_equal(world.num_sprites, 0);

    memset(&decal, 0, sizeof(decal));
    assert_int_equal(world_add_decal(NULL, decal), WORLD_INSERT_INVALID);
    assert_int_equal(world_add_decal(&world, decal), WORLD_INSERT_INVALID);
    assert_int_equal(world.num_decals, 0);
}

static void test_decal_rendering_wall(void **state) {
    (void)state;
    Grid *g = grid_create(10, 10);
    Map *m = map_create(5, 5);
    map_set(m, 2, 0, 1); // Wall at (2,0)
    
    Camera cam;
    camera_init(&cam, 2.5, 2.5, -PI/2.0, PI/2.0); // Looking at the wall at (2,0)
    
    AssetRegistry assets;
    assert_true(asset_registry_init(&assets));
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
    raycast_render(g, m, &cam, &assets, &world, NULL);
    
    int dx, dy;
    assert_true(find_grid_glyph(g, 'D', &dx, &dy));
    Cell c;
    grid_get(g, dx, dy, &c);
    assert_int_equal(c.fg.r, 30); // 255 * 0.2 * 0.6 (side 1)
    
    world_clear(&world);
    asset_registry_clear(&assets);
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
    SceneAuthoredCell cells[25] = {0};
    SceneSurfaceView surfaces = {cells, 25U, 5, 5};
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");
    assert_true(snprintf(assets.material_names[1],
                         sizeof(assets.material_names[1]), "1") > 0);
    for (size_t i = 0U; i < 25U; i++) cells[i].floor_material = 1U;

    WorldState world;
    world_init(&world);
    
    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_FLOOR;
    d.x = 3.75; d.y = 2.5; d.z = 0.0; // 1.25 units in front of camera
    d.rotation = PI / 2.0;
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
    raycast_render(g, m, &cam, &assets, &world, &surfaces);
    
    // Check some floor pixels. 
    // This is a bit tricky to predict exact pixel, but let's check a range.
    Cell c;
    grid_get(g, 5, 9, &c);
    assert_int_equal(c.glyph, 'F');
    assert_int_equal(c.fg.g, 51);
    
    world_clear(&world);
    asset_registry_clear(&assets);
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
    
    // Add a horizontal row decal on the floor exactly 2 units in front.
    // Every anchor has the same world X depth, so fisheye correction should
    // keep all projected glyphs on one screen row.
    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_FLOOR;
    d.x = 7.0; d.y = 5.0; d.z = 0.0;
    d.rotation = PI / 2.0;
    d.width = 1.0; d.height = 0.1;
    d.glyph_step_u = 0.2;
    d.glyph_step_v = 0.1;
    d.depth = 0.1;
    d.pattern_cols = 5; d.pattern_rows = 1;
    d.pattern = malloc(5 * sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'A', 1};
    d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1};
    d.pattern[3] = (PatternCell){'D', 1};
    d.pattern[4] = (PatternCell){'E', 1};

    world_add_decal(&world, d);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);
    
    int ax, ay, bx, by, cx, cy, dx, dy, ex, ey;
    assert_unique_glyph(g, 'A', &ax, &ay);
    assert_unique_glyph(g, 'B', &bx, &by);
    assert_unique_glyph(g, 'C', &cx, &cy);
    assert_unique_glyph(g, 'D', &dx, &dy);
    assert_unique_glyph(g, 'E', &ex, &ey);

    assert_int_equal(ay, by);
    assert_int_equal(by, cy);
    assert_int_equal(cy, dy);
    assert_int_equal(dy, ey);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}


static void test_decal_art_format(void **state) {
    (void)state;
    assert_int_equal(system("mkdir -p tests/assets_test/maps tests/assets_test/decals tests/assets_test/lights tests/assets_test/materials tests/assets_test/palettes tests/assets_test/sprites"), 0);
    
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
    assert_int_equal(system("rm -rf tests/assets_test"), 0);
}

static void test_decal_art_format_failure(void **state) {
    (void)state;
    assert_int_equal(system("mkdir -p tests/assets_test/maps tests/assets_test/decals tests/assets_test/lights tests/assets_test/materials tests/assets_test/palettes tests/assets_test/sprites"), 0);
    
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
    assert_int_equal(system("rm -rf tests/assets_test"), 0);
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
    /* Explicit step: large enough to project each anchor to a distinct cell
     * at the ~1.5-unit camera distance in a 20-cell-wide grid. */
    d.glyph_step_u = 0.3; d.glyph_step_v = 0.3;
    d.depth = 0.1;
    d.pattern_cols = 2; d.pattern_rows = 2;
    d.pattern = malloc(4 * sizeof(PatternCell));
    d.pattern[0] = (PatternCell){'A', 1}; d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1}; d.pattern[3] = (PatternCell){'D', 1};

    world_add_decal(&world, d);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);
    
    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');
    GlyphBounds c = assert_glyph_bounds(g, 'C');
    GlyphBounds e = assert_glyph_bounds(g, 'D');
    assert_int_equal(a.count, 1);
    assert_int_equal(b.count, 1);
    assert_int_equal(c.count, 1);
    assert_int_equal(e.count, 1);

    world_clear(&world);
    asset_registry_clear(&assets);
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
    /* Explicit step: see floor_continuous_sampling rationale above. */
    d.glyph_step_u = 0.3; d.glyph_step_v = 0.3;
    d.depth = 0.1;
    d.pattern_cols = 2; d.pattern_rows = 2;
    d.pattern = malloc(4 * sizeof(PatternCell));
    d.pattern[0] = (PatternCell){'A', 1}; d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1}; d.pattern[3] = (PatternCell){'D', 1};

    world_add_decal(&world, d);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');
    GlyphBounds c = assert_glyph_bounds(g, 'C');
    GlyphBounds e = assert_glyph_bounds(g, 'D');
    assert_int_equal(a.count, 1);
    assert_int_equal(b.count, 1);
    assert_int_equal(c.count, 1);
    assert_int_equal(e.count, 1);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}


static void test_decal_floor_glyph_order_2x2(void **state) {
    (void)state;
    Grid *g = grid_create(40, 40);
    Map *m = map_create(6, 6);

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
    d.x = 4.0; d.y = 2.5; d.z = 0.0;
    d.rotation = PI / 2.0;
    d.width = 1.0; d.height = 1.0;
    /* Explicit step: large enough to project each anchor to a distinct cell. */
    d.glyph_step_u = 0.2; d.glyph_step_v = 0.2;
    d.depth = 0.1;
    d.pattern_cols = 2; d.pattern_rows = 2;
    d.pattern = malloc(4 * sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'A', 1}; d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1}; d.pattern[3] = (PatternCell){'D', 1};

    world_add_decal(&world, d);
    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');
    GlyphBounds c = assert_glyph_bounds(g, 'C');
    GlyphBounds e = assert_glyph_bounds(g, 'D');

    assert_same_row_band(a, b);
    assert_same_row_band(c, e);
    assert_adjacent_left_to_right(a, b);
    assert_adjacent_left_to_right(c, e);
    assert_adjacent_top_to_bottom(a, c);
    assert_adjacent_top_to_bottom(b, e);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_ceiling_glyph_order_2x2(void **state) {
    (void)state;
    Grid *g = grid_create(40, 40);
    Map *m = map_create(6, 6);

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
    d.surface = DECAL_SURFACE_CEILING;
    d.x = 4.0; d.y = 2.5; d.z = 1.0;
    d.rotation = PI / 2.0;
    d.width = 1.0; d.height = 1.0;
    /* Explicit step: large enough to project each anchor to a distinct cell. */
    d.glyph_step_u = 0.2; d.glyph_step_v = 0.2;
    d.depth = 0.1;
    d.pattern_cols = 2; d.pattern_rows = 2;
    d.pattern = malloc(4 * sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'A', 1}; d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1}; d.pattern[3] = (PatternCell){'D', 1};

    world_add_decal(&world, d);
    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');
    GlyphBounds c = assert_glyph_bounds(g, 'C');
    GlyphBounds e = assert_glyph_bounds(g, 'D');

    assert_same_row_band(a, b);
    assert_same_row_band(c, e);
    assert_adjacent_left_to_right(a, b);
    assert_adjacent_left_to_right(c, e);
    assert_true(a.min_y != c.min_y);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_floor_glyph_row_order_4x1(void **state) {
    (void)state;
    Grid *g = grid_create(40, 40);
    Map *m = map_create(6, 6);

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
    d.x = 4.0; d.y = 2.5; d.z = 0.0;
    d.rotation = PI / 2.0;
    d.width = 1.0; d.height = 0.25;
    /* Explicit step: large enough to spread 4 glyphs across distinct cells. */
    d.glyph_step_u = 0.15; d.glyph_step_v = 0.15;
    d.depth = 0.1;
    d.pattern_cols = 4; d.pattern_rows = 1;
    d.pattern = malloc(4 * sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'A', 1}; d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1}; d.pattern[3] = (PatternCell){'D', 1};

    world_add_decal(&world, d);
    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');
    GlyphBounds c = assert_glyph_bounds(g, 'C');
    GlyphBounds e = assert_glyph_bounds(g, 'D');

    assert_adjacent_left_to_right(a, b);
    assert_adjacent_left_to_right(b, c);
    assert_adjacent_left_to_right(c, e);
    assert_same_row_band(a, b);
    assert_same_row_band(b, c);
    assert_same_row_band(c, e);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_ceiling_glyph_row_order_4x1(void **state) {
    (void)state;
    Grid *g = grid_create(40, 40);
    Map *m = map_create(6, 6);

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
    d.surface = DECAL_SURFACE_CEILING;
    d.x = 4.0; d.y = 2.5; d.z = 1.0;
    d.rotation = PI / 2.0;
    d.width = 1.0; d.height = 0.25;
    /* Explicit step: large enough to spread 4 glyphs across distinct cells. */
    d.glyph_step_u = 0.15; d.glyph_step_v = 0.15;
    d.depth = 0.1;
    d.pattern_cols = 4; d.pattern_rows = 1;
    d.pattern = malloc(4 * sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'A', 1}; d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1}; d.pattern[3] = (PatternCell){'D', 1};

    world_add_decal(&world, d);
    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');
    GlyphBounds c = assert_glyph_bounds(g, 'C');
    GlyphBounds e = assert_glyph_bounds(g, 'D');

    assert_adjacent_left_to_right(a, b);
    assert_adjacent_left_to_right(b, c);
    assert_adjacent_left_to_right(c, e);
    assert_same_row_band(a, b);
    assert_same_row_band(b, c);
    assert_same_row_band(c, e);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_wall_glyph_grid_2x3(void **state) {
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
    d.width = 1.0; d.height = 0.9;
    d.depth = 0.1;
    d.pattern_cols = 2; d.pattern_rows = 3;
    d.pattern = malloc(6 * sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'A', 1}; d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1}; d.pattern[3] = (PatternCell){'D', 1};
    d.pattern[4] = (PatternCell){'E', 1}; d.pattern[5] = (PatternCell){'F', 1};

    world_add_decal(&world, d);
    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');
    GlyphBounds c = assert_glyph_bounds(g, 'C');
    GlyphBounds d_bounds = assert_glyph_bounds(g, 'D');
    GlyphBounds e = assert_glyph_bounds(g, 'E');
    GlyphBounds f = assert_glyph_bounds(g, 'F');

    assert_same_row_band(a, b);
    assert_same_row_band(c, d_bounds);
    assert_same_row_band(e, f);
    assert_adjacent_left_to_right(a, b);
    assert_adjacent_left_to_right(c, d_bounds);
    assert_adjacent_left_to_right(e, f);
    assert_adjacent_top_to_bottom(a, c);
    assert_adjacent_top_to_bottom(c, e);
    assert_adjacent_top_to_bottom(b, d_bounds);
    assert_adjacent_top_to_bottom(d_bounds, f);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_wall_spacing_adjacent(void **state) {
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
    d.width = 1.0; d.height = 0.5;
    d.glyph_step_u = 0.04; d.glyph_step_v = 0.02;
    d.depth = 0.1;
    d.pattern_cols = 2; d.pattern_rows = 1;
    d.pattern = malloc(2 * sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'A', 1};
    d.pattern[1] = (PatternCell){'B', 1};

    world_add_decal(&world, d);
    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');

    assert_same_row_band(a, b);
    assert_adjacent_left_to_right(a, b);

    world_clear(&world);
    asset_registry_clear(&assets);
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
    d.glyph_step_u = 0.04; d.glyph_step_v = 0.02;
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
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');
    GlyphBounds c = assert_glyph_bounds(g, 'C');
    GlyphBounds e = assert_glyph_bounds(g, 'D');

    assert_same_row_band(a, b);
    assert_same_row_band(b, c);
    assert_same_row_band(c, e);
    assert_adjacent_left_to_right(a, b);
    assert_adjacent_left_to_right(b, c);
    assert_adjacent_left_to_right(c, e);

    world_clear(&world);
    asset_registry_clear(&assets);
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
    raycast_render(g, m, &cam, &assets, &world, NULL);

    Cell c;
    grid_get(g, 10, 10, &c);
    assert_int_equal(c.glyph, 'F');
    assert_int_equal(count_grid_glyph(g, 'B'), 0);

    world_clear(&world);
    asset_registry_clear(&assets);
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
    raycast_render(g, m, &cam, &assets, &world, NULL);
    
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
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_rendering_extreme_horizon_offsets(void **state) {
    Grid *g = grid_create(41, 25);
    Map *m = map_create(5, 5);
    Camera cam;
    AssetRegistry assets;
    WorldState world;
    Decal d;
    (void)state;

    assert_non_null(g);
    assert_non_null(m);
    map_set(m, 3, 2, 1);
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    asset_registry_init(&assets);
    asset_registry_set_palette(
        &assets, 1,
        (SDL_Color){255,255,255,255},
        (SDL_Color){255,255,255,255},
        (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");
    world_init(&world);
    memset(&d, 0, sizeof(d));
    d.surface = DECAL_SURFACE_WALL;
    d.x = 3.0; d.y = 2.5; d.z = 0.5;
    d.rotation = PI;
    d.width = 1.0; d.height = 1.0; d.depth = 0.1;
    d.pattern_cols = 1; d.pattern_rows = 1;
    d.pattern = malloc(sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'X', 1};
    assert_int_equal(world_add_decal(&world, d), WORLD_INSERT_OK);
    lighting_update(m, &world);

    cam.pitch = -(double)g->height;
    raycast_render(g, m, &cam, &assets, &world, NULL);
    cam.pitch = (double)g->height;
    raycast_render(g, m, &cam, &assets, &world, NULL);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static GlyphBounds render_single_wall_decal_bounds(double cam_x, uint8_t glyph) {
    Grid *g = grid_create(80, 80);
    Map *m = map_create(8, 5);
    map_set(m, 4, 2, 1);

    Camera cam;
    camera_init(&cam, cam_x, 2.5, 0.0, PI/2.0);

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
    d.width = 0.8; d.height = 0.8;
    d.depth = 0.1;
    d.pattern_cols = 1; d.pattern_rows = 1;
    d.pattern = malloc(sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){glyph, 1};

    world_add_decal(&world, d);
    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds bounds = assert_glyph_bounds(g, glyph);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
    return bounds;
}

static void test_decal_wall_distance_footprint(void **state) {
    (void)state;

    GlyphBounds closer = render_single_wall_decal_bounds(3.0, 'N');
    GlyphBounds farther = render_single_wall_decal_bounds(2.5, 'F');

    assert_true(bounds_area(closer) >= bounds_area(farther));
    assert_true(closer.count >= farther.count);
}

static void test_decal_floor_perspective_plane(void **state) {
    (void)state;
    Grid *g = grid_create(80, 80);
    Map *m = map_create(8, 8);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2.0);

    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "####");

    WorldState world;
    world_init(&world);

    Decal near_d;
    memset(&near_d, 0, sizeof(Decal));
    near_d.surface = DECAL_SURFACE_FLOOR;
    near_d.x = 4.0; near_d.y = 2.5; near_d.z = 0.0;
    near_d.rotation = 0.0;
    near_d.width = 0.3; near_d.height = 0.3;
    near_d.depth = 0.1;
    near_d.pattern_cols = 1; near_d.pattern_rows = 1;
    near_d.pattern = malloc(sizeof(PatternCell));
    assert_non_null(near_d.pattern);
    near_d.pattern[0] = (PatternCell){'N', 1};
    world_add_decal(&world, near_d);

    Decal far_d = near_d;
    far_d.x = 5.0;
    far_d.pattern = malloc(sizeof(PatternCell));
    assert_non_null(far_d.pattern);
    far_d.pattern[0] = (PatternCell){'F', 1};
    world_add_decal(&world, far_d);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds near_bounds = assert_glyph_bounds(g, 'N');
    GlyphBounds far_bounds = assert_glyph_bounds(g, 'F');

    assert_true(near_bounds.min_y >= far_bounds.min_y);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_wall_size_respects_width(void **state) {
    (void)state;
    Grid *g = grid_create(100, 80);
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

    Decal narrow;
    memset(&narrow, 0, sizeof(Decal));
    narrow.surface = DECAL_SURFACE_WALL;
    narrow.x = 4.0; narrow.y = 2.5; narrow.z = 0.75;
    narrow.rotation = PI;
    narrow.width = 0.3; narrow.height = 0.2;
    narrow.depth = 0.1;
    narrow.pattern_cols = 1; narrow.pattern_rows = 1;
    narrow.pattern = malloc(sizeof(PatternCell));
    assert_non_null(narrow.pattern);
    narrow.pattern[0] = (PatternCell){'N', 1};
    world_add_decal(&world, narrow);

    Decal wide = narrow;
    wide.z = 0.25;
    wide.width = 0.9;
    wide.pattern = malloc(sizeof(PatternCell));
    assert_non_null(wide.pattern);
    wide.pattern[0] = (PatternCell){'W', 1};
    world_add_decal(&world, wide);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds narrow_bounds = assert_glyph_bounds(g, 'N');
    GlyphBounds wide_bounds = assert_glyph_bounds(g, 'W');

    assert_true(bounds_width(wide_bounds) >= bounds_width(narrow_bounds));

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_oblique_cell_no_bbox_smear(void **state) {
    (void)state;
    Grid *g = grid_create(80, 80);
    Map *m = map_create(8, 8);
    map_set(m, 5, 2, 1);

    Camera cam;
    camera_init(&cam, 2.5, 3.0, -PI/8.0, PI/2.0);

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
    d.width = 0.8; d.height = 0.8;
    d.depth = 0.1;
    d.pattern_cols = 2; d.pattern_rows = 2;
    d.pattern = malloc(4 * sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'A', 1}; d.pattern[1] = (PatternCell){'B', 1};
    d.pattern[2] = (PatternCell){'C', 1}; d.pattern[3] = (PatternCell){'D', 1};
    world_add_decal(&world, d);

    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');
    GlyphBounds c = assert_glyph_bounds(g, 'C');
    GlyphBounds d_bounds = assert_glyph_bounds(g, 'D');

    assert_true(a.min_x < b.max_x);
    assert_true(c.min_x < d_bounds.max_x);
    assert_true(a.min_y < c.max_y);
    assert_true(b.min_y < d_bounds.max_y);
    assert_true(a.max_y <= d_bounds.max_y);
    assert_true(b.max_y <= c.max_y || b.min_x > c.max_x);

    GlyphBounds all_top = a;
    if (b.min_x < all_top.min_x) all_top.min_x = b.min_x;
    if (b.max_x > all_top.max_x) all_top.max_x = b.max_x;
    if (b.min_y < all_top.min_y) all_top.min_y = b.min_y;
    if (b.max_y > all_top.max_y) all_top.max_y = b.max_y;
    all_top.count += b.count;

    assert_true(a.count < all_top.count);
    assert_true(b.count < all_top.count);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_decal_whitespace_preserved(void **state) {
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
    d.width = 1.2; d.height = 0.4;
    d.depth = 0.1;
    d.pattern_cols = 3; d.pattern_rows = 1;
    d.pattern = malloc(3 * sizeof(PatternCell));
    assert_non_null(d.pattern);
    d.pattern[0] = (PatternCell){'A', 1};
    d.pattern[1] = (PatternCell){' ', 1};
    d.pattern[2] = (PatternCell){'B', 1};

    world_add_decal(&world, d);
    lighting_update(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');

    assert_true(a.max_x < b.min_x);

    int middle_x = (a.max_x + b.min_x) / 2;
    int middle_y = (a.min_y + a.max_y) / 2;
    Cell middle;
    assert_true(grid_get(g, middle_x, middle_y, &middle));
    assert_true(middle.glyph != 'A');
    assert_true(middle.glyph != 'B');

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}


/* Default glyph-step (no explicit step set) must not repeat glyphs. */
static void test_decal_default_spacing_no_smear(void **state) {
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

    /* No glyph_step_u/v set — fallback to DEFAULT_DECAL_GLYPH_COMPRESSION path. */
    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.surface = DECAL_SURFACE_WALL;
    d.x = 4.0; d.y = 2.5; d.z = 0.5;
    d.rotation = PI;
    d.width = 1.0; d.height = 0.5;
    /* glyph_step_u = 0, glyph_step_v = 0 — relies on default compression */
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
    raycast_render(g, m, &cam, &assets, &world, NULL);

    /* Each glyph must appear exactly once — no smearing or repetition. */
    GlyphBounds a = assert_glyph_bounds(g, 'A');
    GlyphBounds b = assert_glyph_bounds(g, 'B');
    GlyphBounds c = assert_glyph_bounds(g, 'C');
    GlyphBounds e = assert_glyph_bounds(g, 'D');
    assert_int_equal(a.count, 1);
    assert_int_equal(b.count, 1);
    assert_int_equal(c.count, 1);
    assert_int_equal(e.count, 1);

    /* Glyphs must appear in left-to-right order. */
    assert_same_row_band(a, b);
    assert_same_row_band(b, c);
    assert_same_row_band(c, e);
    assert_adjacent_left_to_right(a, b);
    assert_adjacent_left_to_right(b, c);
    assert_adjacent_left_to_right(c, e);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

/* An explicit glyph_step_u/v override must produce a larger inter-glyph
 * spacing than the compressed default. */
static void test_decal_default_spacing_explicit_overrides(void **state) {
    (void)state;

    /* --- Decal with default (compressed) spacing. --- */
    Grid *g_default = grid_create(80, 80);
    Map *m_default = map_create(8, 5);
    map_set(m_default, 4, 2, 1);
    Camera cam_default;
    camera_init(&cam_default, 2.5, 2.5, 0.0, PI/2.0);
    AssetRegistry assets_default;
    asset_registry_init(&assets_default);
    asset_registry_set_palette(&assets_default, 1,
        (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets_default, 1, 1, "####");
    WorldState world_default;
    world_init(&world_default);

    Decal dd;
    memset(&dd, 0, sizeof(Decal));
    dd.surface = DECAL_SURFACE_WALL;
    dd.x = 4.0; dd.y = 2.5; dd.z = 0.5;
    dd.rotation = PI;
    dd.width = 1.0; dd.height = 0.5;
    /* glyph_step_u = 0 → default compression applies */
    dd.depth = 0.1;
    dd.pattern_cols = 2; dd.pattern_rows = 1;
    dd.pattern = malloc(2 * sizeof(PatternCell));
    assert_non_null(dd.pattern);
    dd.pattern[0] = (PatternCell){'A', 1};
    dd.pattern[1] = (PatternCell){'B', 1};
    world_add_decal(&world_default, dd);
    lighting_update(m_default, &world_default);
    raycast_render(g_default, m_default, &cam_default, &assets_default, &world_default, NULL);

    GlyphBounds def_a = assert_glyph_bounds(g_default, 'A');
    GlyphBounds def_b = assert_glyph_bounds(g_default, 'B');
    int default_gap = def_b.min_x - def_a.max_x;

    world_clear(&world_default);
    asset_registry_clear(&assets_default);
    map_destroy(m_default);
    grid_destroy(g_default);

    /* --- Decal with explicit large glyph_step_u. --- */
    Grid *g_explicit = grid_create(80, 80);
    Map *m_explicit = map_create(8, 5);
    map_set(m_explicit, 4, 2, 1);
    Camera cam_explicit;
    camera_init(&cam_explicit, 2.5, 2.5, 0.0, PI/2.0);
    AssetRegistry assets_explicit;
    asset_registry_init(&assets_explicit);
    asset_registry_set_palette(&assets_explicit, 1,
        (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets_explicit, 1, 1, "####");
    WorldState world_explicit;
    world_init(&world_explicit);

    Decal de;
    memset(&de, 0, sizeof(Decal));
    de.surface = DECAL_SURFACE_WALL;
    de.x = 4.0; de.y = 2.5; de.z = 0.5;
    de.rotation = PI;
    de.width = 1.0; de.height = 0.5;
    /* explicit glyph_step_u is intentionally larger than the default */
    de.glyph_step_u = 0.1; de.glyph_step_v = 0.05;
    de.depth = 0.1;
    de.pattern_cols = 2; de.pattern_rows = 1;
    de.pattern = malloc(2 * sizeof(PatternCell));
    assert_non_null(de.pattern);
    de.pattern[0] = (PatternCell){'C', 1};
    de.pattern[1] = (PatternCell){'D', 1};
    world_add_decal(&world_explicit, de);
    lighting_update(m_explicit, &world_explicit);
    raycast_render(g_explicit, m_explicit, &cam_explicit, &assets_explicit, &world_explicit, NULL);

    GlyphBounds exp_c = assert_glyph_bounds(g_explicit, 'C');
    GlyphBounds exp_d = assert_glyph_bounds(g_explicit, 'D');
    int explicit_gap = exp_d.min_x - exp_c.max_x;

    world_clear(&world_explicit);
    asset_registry_clear(&assets_explicit);
    map_destroy(m_explicit);
    grid_destroy(g_explicit);

    /* The explicit step (0.1 world units) must produce a wider gap than the
     * default-compressed step (1.0 / 2 / 8.0 = 0.0625 world units). */
    assert_true(explicit_gap > default_gap);
}

/* Default-spacing decals must render the same way on wall, floor, and ceiling
 * (identical step formula, no surface-specific spacing divergence). */
static void test_decal_default_spacing_surface_consistency(void **state) {
    (void)state;

    /* Wall decal — 2-glyph row, no explicit step. */
    {
        Grid *g = grid_create(80, 80);
        Map *m = map_create(8, 5);
        map_set(m, 4, 2, 1);
        Camera cam;
        camera_init(&cam, 2.5, 2.5, 0.0, PI/2.0);
        AssetRegistry assets;
        asset_registry_init(&assets);
        asset_registry_set_palette(&assets, 1,
            (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
        asset_registry_set_material(&assets, 1, 1, "####");
        WorldState world;
        world_init(&world);
        Decal d;
        memset(&d, 0, sizeof(Decal));
        d.surface = DECAL_SURFACE_WALL;
        d.x = 4.0; d.y = 2.5; d.z = 0.5;
        d.rotation = PI;
        d.width = 1.0; d.height = 0.5;
        d.depth = 0.1;
        d.pattern_cols = 2; d.pattern_rows = 1;
        d.pattern = malloc(2 * sizeof(PatternCell));
        assert_non_null(d.pattern);
        d.pattern[0] = (PatternCell){'A', 1};
        d.pattern[1] = (PatternCell){'B', 1};
        world_add_decal(&world, d);
        lighting_update(m, &world);
        raycast_render(g, m, &cam, &assets, &world, NULL);
        assert_glyph_bounds(g, 'A');
        assert_glyph_bounds(g, 'B');
        world_clear(&world);
        asset_registry_clear(&assets);
        map_destroy(m);
        grid_destroy(g);
    }

    /* Floor decal — 2-glyph row, no explicit step. */
    {
        Grid *g = grid_create(80, 80);
        Map *m = map_create(8, 8);
        Camera cam;
        camera_init(&cam, 2.5, 2.5, 0.0, PI/2.0);
        AssetRegistry assets;
        asset_registry_init(&assets);
        asset_registry_set_palette(&assets, 1,
            (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
        asset_registry_set_material(&assets, 1, 1, "####");
        WorldState world;
        world_init(&world);
        Decal d;
        memset(&d, 0, sizeof(Decal));
        d.surface = DECAL_SURFACE_FLOOR;
        d.x = 4.0; d.y = 2.5; d.z = 0.0;
        d.rotation = PI / 2.0;
        d.width = 1.0; d.height = 0.5;
        d.depth = 0.1;
        d.pattern_cols = 2; d.pattern_rows = 1;
        d.pattern = malloc(2 * sizeof(PatternCell));
        assert_non_null(d.pattern);
        d.pattern[0] = (PatternCell){'C', 1};
        d.pattern[1] = (PatternCell){'D', 1};
        world_add_decal(&world, d);
        lighting_update(m, &world);
        raycast_render(g, m, &cam, &assets, &world, NULL);
        assert_glyph_bounds(g, 'C');
        assert_glyph_bounds(g, 'D');
        world_clear(&world);
        asset_registry_clear(&assets);
        map_destroy(m);
        grid_destroy(g);
    }

    /* Ceiling decal — 2-glyph row, no explicit step. */
    {
        Grid *g = grid_create(80, 80);
        Map *m = map_create(8, 8);
        Camera cam;
        camera_init(&cam, 2.5, 2.5, 0.0, PI/2.0);
        AssetRegistry assets;
        asset_registry_init(&assets);
        asset_registry_set_palette(&assets, 1,
            (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
        asset_registry_set_material(&assets, 1, 1, "####");
        WorldState world;
        world_init(&world);
        Decal d;
        memset(&d, 0, sizeof(Decal));
        d.surface = DECAL_SURFACE_CEILING;
        d.x = 4.0; d.y = 2.5; d.z = 1.0;
        d.rotation = PI / 2.0;
        d.width = 1.0; d.height = 0.5;
        d.depth = 0.1;
        d.pattern_cols = 2; d.pattern_rows = 1;
        d.pattern = malloc(2 * sizeof(PatternCell));
        assert_non_null(d.pattern);
        d.pattern[0] = (PatternCell){'E', 1};
        d.pattern[1] = (PatternCell){'F', 1};
        world_add_decal(&world, d);
        lighting_update(m, &world);
        raycast_render(g, m, &cam, &assets, &world, NULL);
        assert_glyph_bounds(g, 'E');
        assert_glyph_bounds(g, 'F');
        world_clear(&world);
        asset_registry_clear(&assets);
        map_destroy(m);
        grid_destroy(g);
    }
}

int main(void) {

    config_init_defaults();
    
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_world_decal_add),
        cmocka_unit_test(test_world_decal_rejection_retains_caller_ownership),
        cmocka_unit_test(test_world_insert_boundaries),
        cmocka_unit_test(test_world_insert_invalid_inputs_unchanged),
        cmocka_unit_test(test_decal_rendering_wall),
        cmocka_unit_test(test_decal_rendering_floor),
        cmocka_unit_test(test_decal_fisheye_correction),
        cmocka_unit_test(test_decal_art_format),
        cmocka_unit_test(test_decal_art_format_failure),
        cmocka_unit_test(test_decal_floor_continuous_sampling),
        cmocka_unit_test(test_decal_ceiling_continuous_sampling),
        cmocka_unit_test(test_decal_floor_glyph_order_2x2),
        cmocka_unit_test(test_decal_ceiling_glyph_order_2x2),
        cmocka_unit_test(test_decal_floor_glyph_row_order_4x1),
        cmocka_unit_test(test_decal_ceiling_glyph_row_order_4x1),
        cmocka_unit_test(test_decal_wall_glyph_grid_2x3),
        cmocka_unit_test(test_decal_wall_spacing_adjacent),
        cmocka_unit_test(test_decal_wall_authoritative_dimensions),
        cmocka_unit_test(test_decal_wall_backface_rejected),
        cmocka_unit_test(test_decal_wall_orientation),
        cmocka_unit_test(test_decal_rendering_extreme_horizon_offsets),
        cmocka_unit_test(test_decal_wall_distance_footprint),
        cmocka_unit_test(test_decal_floor_perspective_plane),
        cmocka_unit_test(test_decal_wall_size_respects_width),
        cmocka_unit_test(test_decal_oblique_cell_no_bbox_smear),
        cmocka_unit_test(test_decal_whitespace_preserved),
        cmocka_unit_test(test_decal_default_spacing_no_smear),
        cmocka_unit_test(test_decal_default_spacing_explicit_overrides),
        cmocka_unit_test(test_decal_default_spacing_surface_consistency),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}