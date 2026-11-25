/**
 * test_glyph_paint.c — the colour side of the missing-glyph rule
 *
 * D8 says a missing glyph takes over the cell's colours entirely: magenta on red, opaque. That is
 * the part a user actually notices, and it is the part a theme could quietly soften if it were
 * implemented at three draw sites instead of here.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <string.h>

#include "../src/glyph_paint.h"
#include "../src/font8x8.h"

static GlyphSet build_set(GlyphSetEntry *entries, unsigned count, uint8_t *storage,
                          size_t storage_size) {
    GlyphSet set;
    uint8_t provenance[SHA256_DIGEST_SIZE];
    size_t length = 0U;
    memset(provenance, 0x11, sizeof(provenance));
    assert_true(glyph_set_write(entries, count, provenance, storage, storage_size, &length));
    assert_int_equal(glyph_set_from_bytes(storage, length, &set, NULL), GLYPH_SET_OK);
    return set;
}

static void test_a_found_glyph_keeps_the_cells_own_colours(void **state) {
    SDL_Color fg = {10U, 20U, 30U, 255U};
    SDL_Color bg = {40U, 50U, 60U, 255U};
    GlyphSourceGlyph glyph;
    (void)state;
    memset(&glyph, 0, sizeof(glyph));
    glyph.kind = GLYPH_SOURCE_BUILTIN;
    glyph.codepoint = 'A';
    glyph_paint_colors(&glyph, &fg, &bg);
    assert_int_equal(fg.r, 10U);
    assert_int_equal(fg.g, 20U);
    assert_int_equal(fg.b, 30U);
    assert_int_equal(bg.r, 40U);
    assert_int_equal(bg.g, 50U);
    assert_int_equal(bg.b, 60U);
}

static void test_a_missing_glyph_takes_over_both_colours(void **state) {
    SDL_Color fg = {10U, 20U, 30U, 128U};
    SDL_Color bg = {40U, 50U, 60U, 64U};
    GlyphSourceGlyph glyph;
    (void)state;
    memset(&glyph, 0, sizeof(glyph));
    glyph.kind = GLYPH_SOURCE_MISSING;
    glyph.codepoint = 0x3042U;
    glyph_paint_colors(&glyph, &fg, &bg);
    assert_int_equal(fg.r, GLYPH_SOURCE_SENTINEL_FG_R);
    assert_int_equal(fg.g, GLYPH_SOURCE_SENTINEL_FG_G);
    assert_int_equal(fg.b, GLYPH_SOURCE_SENTINEL_FG_B);
    assert_int_equal(bg.r, GLYPH_SOURCE_SENTINEL_BG_R);
    assert_int_equal(bg.g, GLYPH_SOURCE_SENTINEL_BG_G);
    assert_int_equal(bg.b, GLYPH_SOURCE_SENTINEL_BG_B);
    /* Opaque on purpose: a translucent diagnostic composites into the art and stops being obvious. */
    assert_int_equal(fg.a, 255U);
    assert_int_equal(bg.a, 255U);
}

static void test_resolve_uses_the_active_set_and_restores_the_builtin_face(void **state) {
    GlyphSetEntry entries[1];
    GlyphSet set;
    uint8_t storage[128];
    SDL_Color fg = {1U, 2U, 3U, 255U};
    SDL_Color bg = {4U, 5U, 6U, 255U};
    GlyphSourceGlyph glyph;
    (void)state;

    memset(entries, 0, sizeof(entries));
    entries[0].codepoint = 0x3042U;
    entries[0].rows[0] = 0xAAU;
    set = build_set(entries, 1U, storage, sizeof(storage));

    glyph_source_set_active(&set);
    glyph = glyph_paint_resolve(0x3042U, &fg, &bg);
    assert_int_equal(glyph.kind, GLYPH_SOURCE_SET);
    assert_int_equal(glyph.rows[0], 0xAAU);
    assert_int_equal(fg.r, 1U); /* found: colours untouched */

    /* A codepoint the set lacks, above ASCII, becomes the diagnostic and takes the colours. */
    fg = (SDL_Color){1U, 2U, 3U, 255U};
    bg = (SDL_Color){4U, 5U, 6U, 255U};
    glyph = glyph_paint_resolve(0x3043U, &fg, &bg);
    assert_int_equal(glyph.kind, GLYPH_SOURCE_MISSING);
    assert_int_equal(fg.r, GLYPH_SOURCE_SENTINEL_FG_R);
    assert_int_equal(bg.r, GLYPH_SOURCE_SENTINEL_BG_R);

    /* Clearing the active set returns the process to the built-in face, which is the state a real
       launch has when no conversion has been run. */
    glyph_source_set_active(NULL);
    fg = (SDL_Color){1U, 2U, 3U, 255U};
    glyph = glyph_paint_resolve(0x0041U, &fg, &bg);
    assert_int_equal(glyph.kind, GLYPH_SOURCE_BUILTIN);
    assert_memory_equal(glyph.rows, font8x8_basic[0x41], GLYPH_SOURCE_CELL_SIZE);
    assert_int_equal(fg.r, 1U);
    assert_non_null(glyph_source_active());
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_a_found_glyph_keeps_the_cells_own_colours),
        cmocka_unit_test(test_a_missing_glyph_takes_over_both_colours),
        cmocka_unit_test(test_resolve_uses_the_active_set_and_restores_the_builtin_face)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
