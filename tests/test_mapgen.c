#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include "../src/mapgen.h"
#include "../src/scene_types.h"

/* Flood fill from the spawn cell: every open cell must be reachable, or a generated scene
   would contain rooms the player can see and never enter. */
static bool all_open_cells_reachable(const Map *map) {
    size_t count = (size_t)map->width * (size_t)map->height;
    unsigned char *seen = calloc(count, 1U);
    int *queue = calloc(count, sizeof(int));
    size_t head = 0U;
    size_t tail = 0U;
    bool ok = true;
    if (!seen || !queue) {
        free(seen);
        free(queue);
        return false;
    }
    queue[tail++] = 1 * map->width + 1;
    seen[1 * map->width + 1] = 1U;
    while (head < tail) {
        int index = queue[head++];
        int cx = index % map->width;
        int cy = index / map->width;
        const int dx[4] = {1, -1, 0, 0};
        const int dy[4] = {0, 0, 1, -1};
        int d;
        for (d = 0; d < 4; d++) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            int neighbour;
            if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) continue;
            neighbour = ny * map->width + nx;
            if (seen[neighbour]) continue;
            if (map->cells[neighbour].material_id != 0) continue;
            seen[neighbour] = 1U;
            queue[tail++] = neighbour;
        }
    }
    for (size_t i = 0U; i < count; i++) {
        if (map->cells[i].material_id == 0 && !seen[i]) {
            ok = false;
            break;
        }
    }
    free(seen);
    free(queue);
    return ok;
}

static void test_defaults_are_valid(void **state) {
    MapgenParameters parameters = mapgen_parameters_default();
    (void)state;
    assert_true(mapgen_parameters_valid(&parameters));
    assert_int_equal(parameters.width, 24);
    assert_int_equal(parameters.height, 16);
}

static void test_same_seed_produces_identical_maps(void **state) {
    MapgenParameters parameters = mapgen_parameters_default();
    Map *first = map_create(parameters.width, parameters.height);
    Map *second = map_create(parameters.width, parameters.height);
    MapgenStats first_stats;
    MapgenStats second_stats;
    size_t bytes;
    (void)state;
    assert_non_null(first);
    assert_non_null(second);
    bytes = (size_t)parameters.width * (size_t)parameters.height * sizeof(*first->cells);
    assert_true(mapgen_generate(&parameters, first, &first_stats));
    assert_true(mapgen_generate(&parameters, second, &second_stats));
    assert_memory_equal(first->cells, second->cells, bytes);
    assert_int_equal(first_stats.rooms_placed, second_stats.rooms_placed);
    assert_int_equal(first_stats.open_cells, second_stats.open_cells);
    map_destroy(first);
    map_destroy(second);
}

static void test_different_seeds_produce_different_maps(void **state) {
    MapgenParameters parameters = mapgen_parameters_default();
    Map *first = map_create(parameters.width, parameters.height);
    Map *second = map_create(parameters.width, parameters.height);
    size_t bytes;
    (void)state;
    assert_non_null(first);
    assert_non_null(second);
    bytes = (size_t)parameters.width * (size_t)parameters.height * sizeof(*first->cells);
    parameters.seed = UINT64_C(1);
    assert_true(mapgen_generate(&parameters, first, NULL));
    parameters.seed = UINT64_C(2);
    assert_true(mapgen_generate(&parameters, second, NULL));
    assert_int_not_equal(memcmp(first->cells, second->cells, bytes), 0);
    map_destroy(first);
    map_destroy(second);
}

static void test_border_stays_solid_and_spawn_is_open(void **state) {
    MapgenParameters parameters = mapgen_parameters_default();
    Map *map = map_create(parameters.width, parameters.height);
    (void)state;
    assert_non_null(map);
    for (uint64_t seed = 0U; seed < 16U; seed++) {
        MapgenStats stats;
        parameters.seed = seed;
        assert_true(mapgen_generate(&parameters, map, &stats));
        for (int x = 0; x < map->width; x++) {
            assert_int_equal(map->cells[x].material_id, parameters.solid_material);
            assert_int_equal(
                map->cells[(map->height - 1) * map->width + x].material_id,
                parameters.solid_material);
        }
        for (int y = 0; y < map->height; y++) {
            assert_int_equal(map->cells[y * map->width].material_id,
                             parameters.solid_material);
            assert_int_equal(
                map->cells[y * map->width + map->width - 1].material_id,
                parameters.solid_material);
        }
        assert_int_equal(map->cells[1 * map->width + 1].material_id, 0);
        assert_true(stats.open_cells > 0U);
        assert_int_equal(stats.open_cells + stats.solid_cells,
                         (size_t)map->width * (size_t)map->height);
        assert_int_equal(stats.rooms_placed, parameters.room_attempts);
        assert_true(all_open_cells_reachable(map));
    }
    map_destroy(map);
}

static void test_zero_seed_is_a_valid_seed(void **state) {
    MapgenParameters parameters = mapgen_parameters_default();
    Map *map = map_create(parameters.width, parameters.height);
    MapgenStats stats;
    (void)state;
    assert_non_null(map);
    parameters.seed = 0U;
    assert_true(mapgen_generate(&parameters, map, &stats));
    assert_true(stats.open_cells > 0U);
    map_destroy(map);
}

static void test_parameters_reject_unusable_requests(void **state) {
    MapgenParameters parameters;
    (void)state;
    assert_false(mapgen_parameters_valid(NULL));

    parameters = mapgen_parameters_default();
    parameters.width = MAPGEN_MIN_DIMENSION - 1;
    assert_false(mapgen_parameters_valid(&parameters));

    parameters = mapgen_parameters_default();
    parameters.width = SCENE_MAX_WIDTH + 1;
    assert_false(mapgen_parameters_valid(&parameters));

    parameters = mapgen_parameters_default();
    parameters.height = SCENE_MAX_HEIGHT + 1;
    assert_false(mapgen_parameters_valid(&parameters));

    parameters = mapgen_parameters_default();
    parameters.solid_material = 0;
    assert_false(mapgen_parameters_valid(&parameters));

    parameters = mapgen_parameters_default();
    parameters.room_attempts = 0;
    assert_false(mapgen_parameters_valid(&parameters));

    parameters = mapgen_parameters_default();
    parameters.room_attempts = MAPGEN_MAX_ROOMS + 1;
    assert_false(mapgen_parameters_valid(&parameters));

    parameters = mapgen_parameters_default();
    parameters.room_min_size = 1;
    assert_false(mapgen_parameters_valid(&parameters));

    parameters = mapgen_parameters_default();
    parameters.room_max_size = parameters.room_min_size - 1;
    assert_false(mapgen_parameters_valid(&parameters));

    /* A room that cannot fit inside the border is refused, not clamped. */
    parameters = mapgen_parameters_default();
    parameters.width = MAPGEN_MIN_DIMENSION;
    parameters.height = MAPGEN_MIN_DIMENSION;
    parameters.room_max_size = MAPGEN_MIN_DIMENSION - 1;
    assert_false(mapgen_parameters_valid(&parameters));
}

static void test_generate_rejects_mismatched_or_empty_targets(void **state) {
    MapgenParameters parameters = mapgen_parameters_default();
    Map *smaller = map_create(parameters.width - 1, parameters.height);
    (void)state;
    assert_non_null(smaller);
    assert_false(mapgen_generate(&parameters, NULL, NULL));
    assert_false(mapgen_generate(NULL, smaller, NULL));
    /* Dimensions are the caller's contract: a map that does not match the parameters is
       refused rather than generated into and silently truncated. */
    assert_false(mapgen_generate(&parameters, smaller, NULL));
    map_destroy(smaller);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_defaults_are_valid),
        cmocka_unit_test(test_same_seed_produces_identical_maps),
        cmocka_unit_test(test_different_seeds_produce_different_maps),
        cmocka_unit_test(test_border_stays_solid_and_spawn_is_open),
        cmocka_unit_test(test_zero_seed_is_a_valid_seed),
        cmocka_unit_test(test_parameters_reject_unusable_requests),
        cmocka_unit_test(test_generate_rejects_mismatched_or_empty_targets)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

