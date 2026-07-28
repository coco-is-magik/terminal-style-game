/**
 * test_decal_io.c — Tests for decal_io.h (load, save, free)
 *
 * Covers:
 *   - Round-trip identity (save then load preserves all fields)
 *   - File format contains required keys (engine compatibility)
 *   - Loading an existing engine decal asset file succeeds
 *   - Save to invalid path returns -1
 *   - Load from missing file returns NULL
 *   - decal_free(NULL) is safe
 *   - Art-mode decal file loads correctly via decal_load_from_file
 *   - Load → save → load identity on a real engine asset file
 *   - Structured save output is readable by the engine's batch loader
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "../src/decal.h"
#include "../src/decal_io.h"
#include "../src/assets.h"
#include "../src/world.h"
#include "../src/map.h"
#include "../src/asset_loader.h"

/* Temporary file written and removed within each test */
#define TMP_PATH "tests/tmp_decal_io_test.txt"

/* ===================================================================
 *  Helpers
 * =================================================================== */

/**
 * make_test_decal() — Build a small 3×2 decal with known field values.
 *
 * Caller owns the returned pointer; free with decal_free().
 */
static Decal *make_test_decal(void) {
    Decal *d = calloc(1, sizeof(Decal));
    if (!d) return NULL;

    d->surface      = DECAL_SURFACE_WALL;
    d->x            = 1.5;
    d->y            = 2.5;
    d->z            = 0.5;
    d->map_x        = 3;
    d->map_y        = 7;
    d->side         = 1;
    d->u            = 0.25;
    d->v            = 0.5;
    d->width        = 1.0;
    d->height       = 0.75;
    d->glyph_step_u = 0.0625;
    d->glyph_step_v = 0.0625;
    d->depth        = 0.001;
    d->rotation     = 0.0;
    d->pattern_cols = 3;
    d->pattern_rows = 2;

    d->pattern = calloc(6, sizeof(PatternCell));
    if (!d->pattern) { free(d); return NULL; }

    d->pattern[0] = (PatternCell){'A', 1};
    d->pattern[1] = (PatternCell){'B', 2};
    d->pattern[2] = (PatternCell){'C', 3};
    d->pattern[3] = (PatternCell){'D', 3};
    d->pattern[4] = (PatternCell){'E', 2};
    d->pattern[5] = (PatternCell){'F', 1};

    return d;
}

/* ===================================================================
 *  Tests
 * =================================================================== */

/**
 * test_round_trip — Save a decal, load it back, verify all fields match.
 *
 * Checks surface enum, integer fields (map_x, map_y, side, dims), and
 * every pattern cell's glyph and material_id.
 * Floating-point fields (x, y, z, u, v, ...) are not compared for exact
 * equality because %f has finite precision; integer and enum fields suffice.
 */
static void test_round_trip(void **state) {
    (void)state;

    Decal *orig = make_test_decal();
    assert_non_null(orig);

    assert_int_equal(decal_save_to_file(TMP_PATH, orig), 0);

    Decal *back = decal_load_from_file(TMP_PATH);
    assert_non_null(back);

    assert_int_equal((int)back->surface,      (int)orig->surface);
    assert_int_equal(back->map_x,             orig->map_x);
    assert_int_equal(back->map_y,             orig->map_y);
    assert_int_equal(back->side,              orig->side);
    assert_int_equal(back->pattern_cols,      orig->pattern_cols);
    assert_int_equal(back->pattern_rows,      orig->pattern_rows);

    int cells = orig->pattern_cols * orig->pattern_rows;
    for (int i = 0; i < cells; i++) {
        assert_int_equal(back->pattern[i].glyph,       orig->pattern[i].glyph);
        assert_int_equal(back->pattern[i].material_id, orig->pattern[i].material_id);
    }

    decal_free(orig);
    decal_free(back);
    remove(TMP_PATH);
}

/**
 * test_save_format_keys — Verify the saved file contains the required
 * key-value tokens that the engine's load_decal() parser expects.
 */
static void test_save_format_keys(void **state) {
    (void)state;

    Decal *d = calloc(1, sizeof(Decal));
    assert_non_null(d);
    d->surface       = DECAL_SURFACE_FLOOR;
    d->pattern_cols  = 2;
    d->pattern_rows  = 1;
    d->pattern = calloc(2, sizeof(PatternCell));
    assert_non_null(d->pattern);
    d->pattern[0] = (PatternCell){'X', 1};
    d->pattern[1] = (PatternCell){'Y', 2};

    assert_int_equal(decal_save_to_file(TMP_PATH, d), 0);

    /* Scan the raw file for required keys */
    FILE *f = fopen(TMP_PATH, "r");
    assert_non_null(f);

    int found_surface      = 0;
    int found_pattern_cols = 0;
    int found_pattern_rows = 0;
    int found_pattern_0    = 0;
    int found_material_0   = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "surface=",      8)  == 0) found_surface      = 1;
        if (strncmp(line, "pattern_cols=", 13) == 0) found_pattern_cols = 1;
        if (strncmp(line, "pattern_rows=", 13) == 0) found_pattern_rows = 1;
        if (strncmp(line, "pattern_0=",   10)  == 0) found_pattern_0    = 1;
        if (strncmp(line, "material_0=",  11)  == 0) found_material_0   = 1;
    }
    fclose(f);

    assert_true(found_surface);
    assert_true(found_pattern_cols);
    assert_true(found_pattern_rows);
    assert_true(found_pattern_0);
    assert_true(found_material_0);

    decal_free(d);
    remove(TMP_PATH);
}

/**
 * test_save_pattern_content — Verify that glyphs and material IDs written
 * to the file are correct (not just that the keys exist).
 */
static void test_save_pattern_content(void **state) {
    (void)state;

    Decal *d = calloc(1, sizeof(Decal));
    assert_non_null(d);
    d->pattern_cols = 3;
    d->pattern_rows = 1;
    d->pattern = calloc(3, sizeof(PatternCell));
    assert_non_null(d->pattern);
    d->pattern[0] = (PatternCell){'H', 5};
    d->pattern[1] = (PatternCell){'i', 6};
    d->pattern[2] = (PatternCell){'!', 7};

    assert_int_equal(decal_save_to_file(TMP_PATH, d), 0);

    /* Check that pattern_0 line contains exactly "Hi!" */
    FILE *f = fopen(TMP_PATH, "r");
    assert_non_null(f);

    int found = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "pattern_0=", 10) == 0) {
            /* Value is the rest of the line minus trailing newline */
            char *val = line + 10;
            int len = (int)strlen(val);
            while (len > 0 && (val[len-1] == '\n' || val[len-1] == '\r')) {
                val[--len] = '\0';
            }
            assert_string_equal(val, "Hi!");
            found = 1;
        }
    }
    fclose(f);
    assert_true(found);

    decal_free(d);
    remove(TMP_PATH);
}

/**
 * test_load_engine_asset — Load one of the existing engine decal files.
 * Verifies that our loader handles the real-world format without errors.
 */
static void test_load_engine_asset(void **state) {
    (void)state;

    Decal *d = decal_load_from_file("assets/decals/1.txt");
    assert_non_null(d);
    assert_true(d->pattern_cols > 0);
    assert_true(d->pattern_rows > 0);
    assert_non_null(d->pattern);

    /* assets/decals/1.txt is a 15×1 structured decal starting with 'T' */
    assert_int_equal(d->pattern_cols, 15);
    assert_int_equal(d->pattern_rows, 1);
    assert_int_equal(d->pattern[0].glyph, 'T');

    decal_free(d);
}

/**
 * test_load_art_mode — Write an art-mode decal file and load it.
 * Verifies our loader handles the art= header format correctly.
 */
static void test_load_art_mode(void **state) {
    (void)state;

    /* Write an art-mode file by hand */
    FILE *f = fopen(TMP_PATH, "w");
    assert_non_null(f);
    fprintf(f, "surface=1\n");
    fprintf(f, "pattern_cols=5\n");
    fprintf(f, "pattern_rows=2\n");
    fprintf(f, "default_material=3\n");
    fprintf(f, "art=\n");
    fprintf(f, "HELLO\n");
    fprintf(f, "WORLD\n");
    fclose(f);

    Decal *d = decal_load_from_file(TMP_PATH);
    assert_non_null(d);
    assert_int_equal((int)d->surface,  1);   /* DECAL_SURFACE_FLOOR */
    assert_int_equal(d->pattern_cols,  5);
    assert_int_equal(d->pattern_rows,  2);
    assert_int_equal(d->pattern[0].glyph, 'H');
    assert_int_equal(d->pattern[4].glyph, 'O');
    assert_int_equal(d->pattern[5].glyph, 'W');
    assert_int_equal(d->pattern[9].glyph, 'D');
    /* All cells should use the default_material */
    assert_int_equal(d->pattern[0].material_id, 3);

    decal_free(d);
    remove(TMP_PATH);
}

/**
 * test_save_invalid_path — Save to a nonexistent directory returns -1.
 */
static void test_save_invalid_path(void **state) {
    (void)state;

    Decal *d = calloc(1, sizeof(Decal));
    assert_non_null(d);
    d->pattern_cols = 1;
    d->pattern_rows = 1;
    d->pattern = calloc(1, sizeof(PatternCell));
    assert_non_null(d->pattern);

    int ret = decal_save_to_file("/no_such_dir/test_decal.txt", d);
    assert_int_equal(ret, -1);

    decal_free(d);
}

/**
 * test_load_missing_file — Load a nonexistent file returns NULL.
 */
static void test_load_missing_file(void **state) {
    (void)state;
    Decal *d = decal_load_from_file("assets/decals/this_file_does_not_exist.txt");
    assert_null(d);
}

static void test_load_rejects_invalid_dimensions(void **state) {
    (void)state;
    FILE *file = fopen(TMP_PATH, "w");
    assert_non_null(file);
    assert_true(fputs("pattern_cols=256\npattern_rows=1\npattern_0=X\n", file) >= 0);
    assert_int_equal(fclose(file), 0);
    assert_null(decal_load_from_file(TMP_PATH));
    assert_int_equal(remove(TMP_PATH), 0);

    file = fopen(TMP_PATH, "w");
    assert_non_null(file);
    assert_true(fputs("pattern_cols=1\npattern_rows=-1\npattern_0=X\n", file) >= 0);
    assert_int_equal(fclose(file), 0);
    assert_null(decal_load_from_file(TMP_PATH));
    assert_int_equal(remove(TMP_PATH), 0);
}

static void assert_dimension_file_rejected(const char *cols, const char *rows,
                                           const char *suffix) {
    FILE *file = fopen(TMP_PATH, "w");
    assert_non_null(file);
    assert_true(fprintf(file, "pattern_cols=%s\npattern_rows=%s\n%s",
                        cols, rows, suffix) > 0);
    assert_int_equal(fclose(file), 0);
    assert_null(decal_load_from_file(TMP_PATH));
    assert_int_equal(remove(TMP_PATH), 0);
}

static void test_load_dimension_boundaries(void **state) {
    (void)state;
    Decal *decal;
    FILE *file = fopen(TMP_PATH, "w");
    assert_non_null(file);
    assert_true(fputs("pattern_cols=255\npattern_rows=1\nart=\n", file) >= 0);
    for (int i = 0; i < 255; i++) assert_int_not_equal(fputc('X', file), EOF);
    assert_int_not_equal(fputc('\n', file), EOF);
    assert_int_equal(fclose(file), 0);
    decal = decal_load_from_file(TMP_PATH);
    assert_non_null(decal);
    assert_int_equal(decal->pattern_cols, 255);
    decal_free(decal);
    assert_int_equal(remove(TMP_PATH), 0);

    assert_dimension_file_rejected("0", "1", "pattern_0=X\n");
    assert_dimension_file_rejected("abc", "1", "pattern_0=X\n");
    assert_dimension_file_rejected("1x", "1", "pattern_0=X\n");
    assert_dimension_file_rejected("999999999999999999999", "1", "pattern_0=X\n");
    assert_dimension_file_rejected("1", "65", "art=\nX\n");
    assert_dimension_file_rejected("1", "0", "art=\nX\n");
}

static void test_all_checked_in_decals_load(void **state) {
    (void)state;
    DIR *directory = opendir("assets/decals");
    struct dirent *entry;
    int loaded = 0;
    assert_non_null(directory);

    while ((entry = readdir(directory)) != NULL) {
        char path[512];
        size_t length = strlen(entry->d_name);
        Decal *decal;
        if (length < 5 || strcmp(entry->d_name + length - 4, ".txt") != 0) continue;
        assert_true(snprintf(path, sizeof(path), "assets/decals/%s", entry->d_name) > 0);
        decal = decal_load_from_file(path);
        assert_non_null(decal);
        assert_non_null(decal->pattern);
        decal_free(decal);
        loaded++;
    }
    assert_int_equal(closedir(directory), 0);
    assert_true(loaded > 0);
}

/**
 * test_free_null — decal_free(NULL) must not crash.
 */
static void test_free_null(void **state) {
    (void)state;
    decal_free(NULL);  /* Must return silently */
}

/**
 * test_multirow_materials — A 2×3 decal with mixed material IDs round-trips
 * correctly.  This catches row-index-off-by-one bugs in the material writer.
 */
static void test_multirow_materials(void **state) {
    (void)state;

    Decal *d = calloc(1, sizeof(Decal));
    assert_non_null(d);
    d->surface      = DECAL_SURFACE_CEILING;
    d->pattern_cols = 2;
    d->pattern_rows = 3;
    d->pattern = calloc(6, sizeof(PatternCell));
    assert_non_null(d->pattern);

    static const uint8_t glyphs[6]    = {'1','2','3','4','5','6'};
    static const uint8_t materials[6] = {  1,  2,  3,  4,  5,  6};
    for (int i = 0; i < 6; i++) {
        d->pattern[i].glyph       = glyphs[i];
        d->pattern[i].material_id = materials[i];
    }

    assert_int_equal(decal_save_to_file(TMP_PATH, d), 0);

    Decal *back = decal_load_from_file(TMP_PATH);
    assert_non_null(back);
    assert_int_equal(back->pattern_cols, 2);
    assert_int_equal(back->pattern_rows, 3);

    for (int i = 0; i < 6; i++) {
        assert_int_equal(back->pattern[i].glyph,       glyphs[i]);
        assert_int_equal(back->pattern[i].material_id, materials[i]);
    }

    decal_free(d);
    decal_free(back);
    remove(TMP_PATH);
}

/**
 * test_load_save_load_identity — Load a real engine asset, save it, reload
 * the saved copy, and verify every glyph and material_id is unchanged.
 *
 * This protects against future editor writes silently corrupting data that
 * was originally produced by the engine's own loader.
 */
static void test_load_save_load_identity(void **state) {
    (void)state;

    /* First load: use the existing engine asset */
    Decal *first = decal_load_from_file("assets/decals/1.txt");
    assert_non_null(first);
    assert_non_null(first->pattern);

    int cols  = first->pattern_cols;
    int rows  = first->pattern_rows;
    int cells = cols * rows;
    assert_true(cells > 0);

    /* Save the loaded decal to a temp file */
    assert_int_equal(decal_save_to_file(TMP_PATH, first), 0);

    /* Second load: reload from the file we just saved */
    Decal *second = decal_load_from_file(TMP_PATH);
    assert_non_null(second);
    assert_int_equal(second->pattern_cols, cols);
    assert_int_equal(second->pattern_rows, rows);
    assert_non_null(second->pattern);

    /* Every cell must be identical */
    for (int i = 0; i < cells; i++) {
        assert_int_equal(second->pattern[i].glyph,
                         first->pattern[i].glyph);
        assert_int_equal(second->pattern[i].material_id,
                         first->pattern[i].material_id);
    }

    decal_free(first);
    decal_free(second);
    remove(TMP_PATH);
}

/**
 * test_engine_compatibility — Save a decal with decal_io, then load it
 * through the engine's runtime batch loader (asset_loader_load_map_data).
 *
 * This proves that decal_save_to_file() writes a file that the engine can
 * consume without any extra translation step.
 */
static void test_engine_compatibility(void **state) {
    (void)state;

#define COMPAT_DIR "tests/tmp_decal_io_compat"

    /* Build the directory tree the engine loader expects */
    assert_int_equal(system("mkdir -p " COMPAT_DIR "/maps "
                       COMPAT_DIR "/decals "
                       COMPAT_DIR "/lights "
                       COMPAT_DIR "/materials "
                       COMPAT_DIR "/palettes "
                       COMPAT_DIR "/sprites"), 0);

    /* Minimal map (3×3, all walls) */
    FILE *fmap = fopen(COMPAT_DIR "/maps/1.txt", "w");
    assert_non_null(fmap);
    fprintf(fmap, "width=3\nheight=3\ndata=\n###\n###\n###\n");
    fclose(fmap);

    /* Build a known decal and save it with decal_io */
    Decal *d = calloc(1, sizeof(Decal));
    assert_non_null(d);
    d->surface      = DECAL_SURFACE_WALL;
    d->pattern_cols = 3;
    d->pattern_rows = 1;
    d->pattern = calloc(3, sizeof(PatternCell));
    assert_non_null(d->pattern);
    d->pattern[0] = (PatternCell){'X', 1};
    d->pattern[1] = (PatternCell){'Y', 2};
    d->pattern[2] = (PatternCell){'Z', 3};

    assert_int_equal(
        decal_save_to_file(COMPAT_DIR "/decals/1.txt", d), 0);

    /* Load through the engine's batch loader */
    WorldState world;
    world_init(&world);
    Map *m = asset_loader_load_map_data(&world, COMPAT_DIR, 1);
    assert_non_null(m);

    /* Engine must have loaded exactly one decal with the correct pattern */
    assert_int_equal(world.num_decals, 1);
    assert_int_equal(world.decals[0].pattern_cols, 3);
    assert_int_equal(world.decals[0].pattern_rows, 1);
    assert_int_equal(world.decals[0].pattern[0].glyph, 'X');
    assert_int_equal(world.decals[0].pattern[1].glyph, 'Y');
    assert_int_equal(world.decals[0].pattern[2].glyph, 'Z');
    assert_int_equal(world.decals[0].pattern[0].material_id, 1);
    assert_int_equal(world.decals[0].pattern[1].material_id, 2);
    assert_int_equal(world.decals[0].pattern[2].material_id, 3);

    decal_free(d);
    world_clear(&world);
    map_destroy(m);
    assert_int_equal(system("rm -rf " COMPAT_DIR), 0);

#undef COMPAT_DIR
}

/* ===================================================================
 *  Entry point
 * =================================================================== */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_round_trip),
        cmocka_unit_test(test_save_format_keys),
        cmocka_unit_test(test_save_pattern_content),
        cmocka_unit_test(test_load_engine_asset),
        cmocka_unit_test(test_load_art_mode),
        cmocka_unit_test(test_save_invalid_path),
        cmocka_unit_test(test_load_missing_file),
        cmocka_unit_test(test_load_rejects_invalid_dimensions),
        cmocka_unit_test(test_load_dimension_boundaries),
        cmocka_unit_test(test_all_checked_in_decals_load),
        cmocka_unit_test(test_free_null),
        cmocka_unit_test(test_multirow_materials),
        cmocka_unit_test(test_load_save_load_identity),
        cmocka_unit_test(test_engine_compatibility),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
