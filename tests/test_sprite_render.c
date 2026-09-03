#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "../src/math.h"
#include "../src/sprite_render.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GRID_WIDTH 41
#define GRID_HEIGHT 25
#define MAP_WIDTH 8
#define MAP_HEIGHT 5

typedef struct {
    Grid *grid;
    Map *map;
    AssetRegistry assets;
    WorldState world;
    Camera camera;
} Fixture;

static void fixture_init(Fixture *fixture) {
    size_t grid_cells = (size_t)GRID_WIDTH * (size_t)GRID_HEIGHT;
    memset(fixture, 0, sizeof(*fixture));
    fixture->grid = grid_create(GRID_WIDTH, GRID_HEIGHT);
    fixture->map = map_create(MAP_WIDTH, MAP_HEIGHT);
    assert_non_null(fixture->grid);
    assert_non_null(fixture->map);
    assert_true(asset_registry_init(&fixture->assets));
    world_init(&fixture->world);
    camera_init(&fixture->camera, 1.5, 2.5, 0.0, PI / 2.0);
    fixture->camera.z = 0.75;
    grid_clear(fixture->grid, (SDL_Color){0, 0, 0, 255});
    for (size_t i = 0U; i < grid_cells; i++) {
        fixture->grid->world_depths[i] = DBL_MAX;
        fixture->grid->overlay_depths[i] = DBL_MAX;
    }
    for (int x = 0; x < GRID_WIDTH; x++)
        fixture->grid->column_depths[x] = DBL_MAX;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            fixture->map->light_map[(size_t)y * MAP_WIDTH + (size_t)x] =
                (LightLevel){1.0, 1.0, 1.0};
        }
    }
}

static void fixture_destroy(Fixture *fixture) {
    world_clear(&fixture->world);
    asset_registry_clear(&fixture->assets);
    map_destroy(fixture->map);
    grid_destroy(fixture->grid);
}

static void add_material(Fixture *fixture, int id, SDL_Color color) {
    asset_registry_set_palette(&fixture->assets, id, color, color, color);
    asset_registry_set_material(&fixture->assets, id, id, "####");
    (void)snprintf(fixture->assets.material_names[id], MATERIAL_NAME_CAPACITY,
                   "%d", id);
}

static void add_sprite_asset(Fixture *fixture, int id, uint8_t glyph,
                             uint16_t material) {
    SpriteAsset *sprite = &fixture->assets.sprites[id];
    sprite->cols = 1;
    sprite->rows = 1;
    sprite->pattern = malloc(sizeof(*sprite->pattern));
    assert_non_null(sprite->pattern);
    sprite->pattern[0] = (PatternCell){glyph, material};
}

static void render(Fixture *fixture, bool bounded) {
    sprite_render_world_overlays(
        fixture->grid, fixture->map, &fixture->camera, &fixture->assets,
        &fixture->world, NULL, fixture->grid->column_depths, bounded,
        fixture->grid->overlay_depths);
}

static bool find_glyph(const Grid *grid, uint8_t glyph, Cell *found,
                       size_t *found_index) {
    size_t count = (size_t)grid->width * (size_t)grid->height;
    for (size_t i = 0U; i < count; i++) {
        if (grid->cells[i].glyph == glyph) {
            if (found) *found = grid->cells[i];
            if (found_index) *found_index = i;
            return true;
        }
    }
    return false;
}

static void test_visible_sprite_is_projected_and_lit(void **state) {
    Fixture fixture;
    Cell cell = {0};
    size_t index = 0U;
    (void)state;
    fixture_init(&fixture);
    add_material(&fixture, 1, (SDL_Color){200, 100, 50, 255});
    add_sprite_asset(&fixture, 1, 'S', 1U);
    fixture.map->light_map[2U * MAP_WIDTH + 3U] =
        (LightLevel){0.5, 0.25, 0.0};
    assert_int_equal(world_add_sprite(&fixture.world, 3.5, 2.5, 1),
                     WORLD_INSERT_OK);
    render(&fixture, true);
    assert_true(find_glyph(fixture.grid, 'S', &cell, &index));
    assert_int_equal(cell.fg.r, 100U);
    assert_int_equal(cell.fg.g, 25U);
    assert_int_equal(cell.fg.b, 0U);
    assert_true(fixture.grid->overlay_depths[index] < DBL_MAX);
    assert_true(fixture.grid->world_depths[index] == DBL_MAX);
    fixture_destroy(&fixture);
}

static void test_world_and_column_depth_occlude_sprite(void **state) {
    Fixture fixture;
    size_t count = (size_t)GRID_WIDTH * (size_t)GRID_HEIGHT;
    (void)state;
    fixture_init(&fixture);
    add_material(&fixture, 1, (SDL_Color){255, 255, 255, 255});
    add_sprite_asset(&fixture, 1, 'S', 1U);
    assert_int_equal(world_add_sprite(&fixture.world, 3.5, 2.5, 1),
                     WORLD_INSERT_OK);
    for (size_t i = 0U; i < count; i++) fixture.grid->world_depths[i] = 1.0;
    render(&fixture, true);
    assert_false(find_glyph(fixture.grid, 'S', NULL, NULL));
    for (size_t i = 0U; i < count; i++) fixture.grid->world_depths[i] = DBL_MAX;
    for (int x = 0; x < GRID_WIDTH; x++) fixture.grid->column_depths[x] = 1.0;
    render(&fixture, false);
    assert_false(find_glyph(fixture.grid, 'S', NULL, NULL));
    fixture_destroy(&fixture);
}

static void test_nearest_sprite_wins_independent_of_array_order(void **state) {
    Fixture fixture;
    (void)state;
    fixture_init(&fixture);
    add_material(&fixture, 1, (SDL_Color){255, 0, 0, 255});
    add_material(&fixture, 2, (SDL_Color){0, 255, 0, 255});
    add_sprite_asset(&fixture, 1, 'N', 1U);
    add_sprite_asset(&fixture, 2, 'F', 2U);
    assert_int_equal(world_add_sprite(&fixture.world, 5.5, 2.5, 2),
                     WORLD_INSERT_OK);
    assert_int_equal(world_add_sprite(&fixture.world, 3.5, 2.5, 1),
                     WORLD_INSERT_OK);
    render(&fixture, true);
    assert_true(find_glyph(fixture.grid, 'N', NULL, NULL));
    assert_false(find_glyph(fixture.grid, 'F', NULL, NULL));
    fixture_destroy(&fixture);

    fixture_init(&fixture);
    add_material(&fixture, 1, (SDL_Color){255, 0, 0, 255});
    add_material(&fixture, 2, (SDL_Color){0, 255, 0, 255});
    add_sprite_asset(&fixture, 1, 'N', 1U);
    add_sprite_asset(&fixture, 2, 'F', 2U);
    assert_int_equal(world_add_sprite(&fixture.world, 3.5, 2.5, 1),
                     WORLD_INSERT_OK);
    assert_int_equal(world_add_sprite(&fixture.world, 5.5, 2.5, 2),
                     WORLD_INSERT_OK);
    render(&fixture, true);
    assert_true(find_glyph(fixture.grid, 'N', NULL, NULL));
    assert_false(find_glyph(fixture.grid, 'F', NULL, NULL));
    fixture_destroy(&fixture);
}

static void test_missing_transparent_and_invalid_assets_are_noops(void **state) {
    Fixture fixture;
    Cell *before;
    size_t bytes = (size_t)GRID_WIDTH * (size_t)GRID_HEIGHT * sizeof(Cell);
    (void)state;
    fixture_init(&fixture);
    before = malloc(bytes);
    assert_non_null(before);
    memcpy(before, fixture.grid->cells, bytes);
    assert_int_equal(world_add_sprite(&fixture.world, 3.5, 2.5, 1),
                     WORLD_INSERT_OK);
    fixture.assets.sprites[2].cols = 1;
    fixture.assets.sprites[2].rows = 1;
    fixture.assets.sprites[2].pattern = malloc(sizeof(PatternCell));
    assert_non_null(fixture.assets.sprites[2].pattern);
    fixture.assets.sprites[2].pattern[0] = (PatternCell){' ', 1U};
    assert_int_equal(world_add_sprite(&fixture.world, 3.5, 2.5, 2),
                     WORLD_INSERT_OK);
    add_sprite_asset(&fixture, 3, 'X', 65535U);
    assert_int_equal(world_add_sprite(&fixture.world, 3.5, 2.5, 3),
                     WORLD_INSERT_OK);
    render(&fixture, true);
    assert_memory_equal(fixture.grid->cells, before, bytes);
    free(before);
    fixture_destroy(&fixture);
}

static void test_sprite_respects_existing_decal_depth_frontier(void **state) {
    Fixture fixture;
    size_t center_index = (size_t)(GRID_HEIGHT / 2) * GRID_WIDTH +
                          (size_t)(GRID_WIDTH / 2);
    (void)state;
    fixture_init(&fixture);
    add_material(&fixture, 1, (SDL_Color){255, 255, 255, 255});
    add_sprite_asset(&fixture, 1, 'S', 1U);
    assert_int_equal(world_add_sprite(&fixture.world, 3.5, 2.5, 1),
                     WORLD_INSERT_OK);
    fixture.grid->cells[center_index].glyph = 'D';
    fixture.grid->overlay_depths[center_index] = 1.0;
    render(&fixture, true);
    assert_int_equal(fixture.grid->cells[center_index].glyph, 'D');

    fixture.grid->overlay_depths[center_index] = 3.0;
    render(&fixture, true);
    assert_int_equal(fixture.grid->cells[center_index].glyph, 'S');
    fixture_destroy(&fixture);
}

static void test_animated_sprite_renders_selected_runtime_frame(void **state) {
    Fixture fixture;
    SpriteAnimationAsset *animation;
    (void)state;
    fixture_init(&fixture);
    add_material(&fixture, 1, (SDL_Color){255, 255, 255, 255});
    animation = &fixture.assets.sprite_animations[7];
    animation->frames = calloc(2U, sizeof(*animation->frames));
    assert_non_null(animation->frames);
    animation->frame_count = 2U;
    animation->frames_per_second = 8.0;
    animation->loop = true;
    for (size_t i = 0U; i < 2U; i++) {
        animation->frames[i].cols = 1;
        animation->frames[i].rows = 1;
        animation->frames[i].pattern = malloc(sizeof(PatternCell));
        assert_non_null(animation->frames[i].pattern);
        animation->frames[i].pattern[0] =
            (PatternCell){i == 0U ? (uint8_t)'A' : (uint8_t)'B', UINT16_C(1)};
    }
    assert_int_equal(world_add_sprite(&fixture.world, 3.5, 2.5, 7),
                     WORLD_INSERT_OK);
    fixture.world.sprites[0].animation_frame = 1U;
    render(&fixture, true);
    assert_true(find_glyph(fixture.grid, 'B', NULL, NULL));
    assert_false(find_glyph(fixture.grid, 'A', NULL, NULL));
    fixture_destroy(&fixture);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_visible_sprite_is_projected_and_lit),
        cmocka_unit_test(test_world_and_column_depth_occlude_sprite),
        cmocka_unit_test(test_nearest_sprite_wins_independent_of_array_order),
        cmocka_unit_test(test_missing_transparent_and_invalid_assets_are_noops),
        cmocka_unit_test(test_sprite_respects_existing_decal_depth_frontier),
        cmocka_unit_test(test_animated_sprite_renders_selected_runtime_frame)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}