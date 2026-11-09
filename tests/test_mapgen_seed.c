#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <string.h>

#include "../src/mapgen_seed.h"
#include "../src/scene_types.h"

static void test_round_trip_preserves_every_parameter(void **state) {
    MapgenParameters source = mapgen_parameters_default();
    MapgenParameters restored;
    char text[MAPGEN_SEED_TEXT_MAX];
    size_t length = 0U;
    (void)state;
    source.seed = UINT64_C(0xDEADBEEF12345678);
    source.width = 33;
    source.height = 21;
    source.solid_material = 4;
    source.room_attempts = 7;
    source.room_min_size = 2;
    source.room_max_size = 5;
    assert_int_equal(mapgen_seed_write_text(&source, text, sizeof(text), &length),
                     MAPGEN_SEED_OK);
    assert_int_equal(length, strlen(text));
    assert_int_equal(mapgen_seed_read_text(text, &restored), MAPGEN_SEED_OK);
    assert_true(restored.seed == source.seed);
    assert_int_equal(restored.width, source.width);
    assert_int_equal(restored.height, source.height);
    assert_int_equal(restored.solid_material, source.solid_material);
    assert_int_equal(restored.room_attempts, source.room_attempts);
    assert_int_equal(restored.room_min_size, source.room_min_size);
    assert_int_equal(restored.room_max_size, source.room_max_size);
}

static void test_text_is_canonical_and_stable(void **state) {
    MapgenParameters parameters = mapgen_parameters_default();
    char first[MAPGEN_SEED_TEXT_MAX];
    char second[MAPGEN_SEED_TEXT_MAX];
    (void)state;
    assert_int_equal(mapgen_seed_write_text(&parameters, first, sizeof(first), NULL),
                     MAPGEN_SEED_OK);
    assert_int_equal(mapgen_seed_write_text(&parameters, second, sizeof(second), NULL),
                     MAPGEN_SEED_OK);
    assert_string_equal(first, second);
    /* Canonical form starts with the version so a reader can reject locally. */
    assert_non_null(strstr(first, "mapgen_version = 1\nseed = "));
}

static void test_write_reports_small_buffers_and_invalid_parameters(void **state) {
    MapgenParameters parameters = mapgen_parameters_default();
    char tiny[16];
    (void)state;
    assert_int_equal(mapgen_seed_write_text(&parameters, tiny, sizeof(tiny), NULL),
                     MAPGEN_SEED_BUFFER_TOO_SMALL);
    assert_int_equal(mapgen_seed_write_text(NULL, tiny, sizeof(tiny), NULL),
                     MAPGEN_SEED_INVALID_ARGUMENT);
    assert_int_equal(mapgen_seed_write_text(&parameters, NULL, 0U, NULL),
                     MAPGEN_SEED_INVALID_ARGUMENT);
    parameters.width = SCENE_MAX_WIDTH + 1;
    assert_int_equal(
        mapgen_seed_write_text(&parameters, tiny, sizeof(tiny), NULL),
        MAPGEN_SEED_INVALID_VALUE);
}

/* The parser is the security-sensitive half: sidecars are user-editable text. */
static void test_reader_rejects_malformed_text(void **state) {
    MapgenParameters parameters;
    (void)state;
    assert_int_equal(mapgen_seed_read_text(NULL, &parameters),
                     MAPGEN_SEED_INVALID_ARGUMENT);
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\n", NULL),
                     MAPGEN_SEED_INVALID_ARGUMENT);
    /* Unknown key. */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\nseed = 1\nwidth = 24\n"
                                           "height = 16\nsolid_material = 1\n"
                                           "room_attempts = 12\nroom_min_size = 3\n"
                                           "room_max_size = 6\nbogus = 1\n",
                                           &parameters),
                     MAPGEN_SEED_MALFORMED);
    /* Duplicate key. */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\nseed = 1\nseed = 2\n"
                                           "width = 24\nheight = 16\nsolid_material = 1\n"
                                           "room_attempts = 12\nroom_min_size = 3\n"
                                           "room_max_size = 6\n",
                                           &parameters),
                     MAPGEN_SEED_MALFORMED);
    /* Missing key: no silent defaulting. */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\nseed = 1\nwidth = 24\n"
                                           "height = 16\nsolid_material = 1\n"
                                           "room_attempts = 12\nroom_min_size = 3\n",
                                           &parameters),
                     MAPGEN_SEED_MALFORMED);
    /* Line without a terminator. */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1", &parameters),
                     MAPGEN_SEED_MALFORMED);
    /* Non-numeric and negative values. */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\nseed = -5\nwidth = 24\n"
                                           "height = 16\nsolid_material = 1\n"
                                           "room_attempts = 12\nroom_min_size = 3\n"
                                           "room_max_size = 6\n",
                                           &parameters),
                     MAPGEN_SEED_MALFORMED);
    /* Two equals signs on one line. */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\nseed = 1 = 2\nwidth = 24\n"
                                           "height = 16\nsolid_material = 1\n"
                                           "room_attempts = 12\nroom_min_size = 3\n"
                                           "room_max_size = 6\n",
                                           &parameters),
                     MAPGEN_SEED_MALFORMED);
}

static void test_reader_rejects_versions_and_values_it_cannot_honour(void **state) {
    MapgenParameters parameters;
    MapgenParameters untouched = mapgen_parameters_default();
    (void)state;
    untouched.seed = UINT64_C(42);
    parameters = untouched;
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 2\nseed = 1\nwidth = 24\n"
                                           "height = 16\nsolid_material = 1\n"
                                           "room_attempts = 12\nroom_min_size = 3\n"
                                           "room_max_size = 6\n",
                                           &parameters),
                     MAPGEN_SEED_UNSUPPORTED_VERSION);
    /* A refusal leaves the caller's parameters alone. */
    assert_true(parameters.seed == untouched.seed);

    /* Over the format ceiling. */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\nseed = 1\n"
                                           "width = 100000\nheight = 16\n"
                                           "solid_material = 1\nroom_attempts = 12\n"
                                           "room_min_size = 3\nroom_max_size = 6\n",
                                           &parameters),
                     MAPGEN_SEED_INVALID_VALUE);
    /* Internally inconsistent: no room can fit the border. */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\nseed = 1\nwidth = 5\n"
                                           "height = 5\nsolid_material = 1\n"
                                           "room_attempts = 12\nroom_min_size = 2\n"
                                           "room_max_size = 4\n",
                                           &parameters),
                     MAPGEN_SEED_INVALID_VALUE);
    /* Material 0 would not be solid rock. */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\nseed = 1\nwidth = 24\n"
                                           "height = 16\nsolid_material = 0\n"
                                           "room_attempts = 12\nroom_min_size = 3\n"
                                           "room_max_size = 6\n",
                                           &parameters),
                     MAPGEN_SEED_INVALID_VALUE);
    /* Absurd counts stay refusals rather than wrapping into "valid". */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\nseed = 1\nwidth = 24\n"
                                           "height = 16\nsolid_material = 1\n"
                                           "room_attempts = 999999999999\n"
                                           "room_min_size = 3\nroom_max_size = 6\n",
                                           &parameters),
                     MAPGEN_SEED_INVALID_VALUE);
}

static void test_reader_tolerates_hand_edited_padding(void **state) {
    MapgenParameters parameters;
    (void)state;
    /* Aligned columns, CRLF, and a blank line: a person edited this, so accept it. */
    assert_int_equal(mapgen_seed_read_text("mapgen_version = 1\r\n"
                                           "seed           = 9\r\n"
                                           "width          = 24\r\n"
                                           "height         = 16\r\n"
                                           "solid_material = 1\r\n"
                                           "room_attempts  = 12\r\n"
                                           "room_min_size  = 3\r\n"
                                           "room_max_size  = 6\r\n"
                                           "\n",
                                           &parameters),
                     MAPGEN_SEED_OK);
    assert_true(parameters.seed == UINT64_C(9));
    assert_int_equal(parameters.room_attempts, 12);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_round_trip_preserves_every_parameter),
        cmocka_unit_test(test_text_is_canonical_and_stable),
        cmocka_unit_test(test_write_reports_small_buffers_and_invalid_parameters),
        cmocka_unit_test(test_reader_rejects_malformed_text),
        cmocka_unit_test(test_reader_rejects_versions_and_values_it_cannot_honour),
        cmocka_unit_test(test_reader_tolerates_hand_edited_padding)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

