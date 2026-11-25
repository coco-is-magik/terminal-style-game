/**
 * test_glyph_set_file.c — four outcomes, and no half-loaded set
 *
 * The interesting distinction is MISSING versus INVALID: the first is ordinary (the built-in ASCII
 * face is used instead), the second is a refusal. Conflating them would either make a normal
 * configuration look broken or let a corrupt font render with holes.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/glyph_set_file.h"

/* A temp path this test owns: created then left empty, so the name is free for the loader to use. */
static void temp_path(char *buffer, size_t size) {
    int descriptor;
    assert_true(size >= 32U);
    memcpy(buffer, "/tmp/tsg_glyph_set_XXXXXX", 26U);
    descriptor = mkstemp(buffer);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
}

static bool write_file(const char *path, const uint8_t *bytes, size_t size) {
    FILE *file = fopen(path, "wb");
    bool ok;
    if (!file) return false;
    ok = fwrite(bytes, 1U, size, file) == size;
    if (fclose(file) != 0) ok = false;
    return ok;
}

static void test_a_valid_set_loads_with_its_entries_intact(void **state) {
    GlyphSetEntry entries[2];
    GlyphSet set;
    GlyphSetFileDiagnostic diagnostic;
    uint8_t encoded[128];
    uint8_t provenance[SHA256_DIGEST_SIZE];
    char path[32];
    uint8_t *bytes = NULL;
    size_t length = 0U;
    GlyphSetEntry found;
    (void)state;

    memset(provenance, 0x7c, sizeof(provenance));
    memset(entries, 0, sizeof(entries));
    entries[0].codepoint = 0x0041U;
    entries[0].rows[0] = 0x0FU;
    entries[1].codepoint = 0x3042U;
    entries[1].mode = 1U;
    entries[1].rows[3] = 0xF0U;
    assert_true(glyph_set_write(entries, 2U, provenance, encoded, sizeof(encoded), &length));

    temp_path(path, sizeof(path));
    assert_true(write_file(path, encoded, length));
    assert_int_equal(glyph_set_file_load(path, &bytes, &set, &diagnostic), GLYPH_SET_FILE_OK);
    assert_int_equal(diagnostic.result, GLYPH_SET_FILE_OK);
    assert_non_null(bytes);
    assert_int_equal(set.glyph_count, 2U);
    assert_memory_equal(set.provenance, provenance, SHA256_DIGEST_SIZE);
    assert_true(glyph_set_lookup(&set, 0x3042U, &found));
    assert_int_equal(found.mode, 1U);
    assert_int_equal(found.rows[3], 0xF0U);

    glyph_set_file_free(bytes);
    unlink(path);
}

static void test_missing_and_unreadable_are_different_answers(void **state) {
    GlyphSet set;
    GlyphSetFileDiagnostic diagnostic;
    uint8_t *bytes = (uint8_t *)0x1;
    (void)state;

    /* Ordinary: no set configured, so the application uses its built-in face. */
    assert_int_equal(glyph_set_file_load("/tmp/tsg_glyph_set_absent", &bytes, &set, &diagnostic),
                     GLYPH_SET_FILE_MISSING);
    assert_null(bytes);
    /* Not ordinary: something is there and cannot be read as a file. */
    assert_int_equal(glyph_set_file_load("/tmp", &bytes, &set, &diagnostic),
                     GLYPH_SET_FILE_UNREADABLE);
    assert_null(bytes);
    assert_int_equal(glyph_set_file_load(NULL, &bytes, &set, &diagnostic),
                     GLYPH_SET_FILE_INVALID_ARGUMENT);
    assert_string_equal(glyph_set_file_result_name(GLYPH_SET_FILE_MISSING), "MISSING");
    assert_string_equal(glyph_set_file_result_name(GLYPH_SET_FILE_INVALID), "INVALID");
}

static void test_a_corrupt_set_is_refused_with_the_format_reason(void **state) {
    uint8_t encoded[128];
    uint8_t provenance[SHA256_DIGEST_SIZE];
    GlyphSetEntry entries[1];
    GlyphSet set;
    GlyphSetFileDiagnostic diagnostic;
    char path[32];
    uint8_t *bytes = NULL;
    size_t length = 0U;
    (void)state;

    memset(provenance, 0, sizeof(provenance));
    memset(entries, 0, sizeof(entries));
    entries[0].codepoint = 0x0041U;
    assert_true(glyph_set_write(entries, 1U, provenance, encoded, sizeof(encoded), &length));

    /* Lop off the last byte: the length no longer matches the count. */
    temp_path(path, sizeof(path));
    assert_true(write_file(path, encoded, length - 1U));
    assert_int_equal(glyph_set_file_load(path, &bytes, &set, &diagnostic),
                     GLYPH_SET_FILE_INVALID);
    assert_int_equal(diagnostic.set_result, GLYPH_SET_TRUNCATED);
    /* A refused load must not hand back a partial set for a caller to render. */
    assert_null(bytes);
    assert_int_equal(set.glyph_count, 0U);
    unlink(path);

    /* Wrong magic: something else entirely, and the reason says which. */
    memcpy(encoded, "NOTASET!", 8U);
    temp_path(path, sizeof(path));
    assert_true(write_file(path, encoded, length));
    assert_int_equal(glyph_set_file_load(path, &bytes, &set, &diagnostic),
                     GLYPH_SET_FILE_INVALID);
    assert_int_equal(diagnostic.set_result, GLYPH_SET_BAD_MAGIC);
    unlink(path);
}

static void test_an_oversized_file_is_refused_before_allocating(void **state) {
    char path[32];
    uint8_t *bytes = NULL;
    GlyphSet set;
    GlyphSetFileDiagnostic diagnostic;
    size_t oversized = glyph_set_encoded_size(GLYPH_SET_MAX_GLYPHS) + 1U;
    uint8_t zero = 0U;
    FILE *file;
    size_t written = 0U;
    (void)state;

    temp_path(path, sizeof(path));
    file = fopen(path, "wb");
    assert_non_null(file);
    /* One byte past the largest legal set: enough to prove the bound, cheap to write. */
    while (written < oversized) {
        assert_true(fwrite(&zero, 1U, 1U, file) == 1U);
        written++;
    }
    assert_int_equal(fclose(file), 0);

    assert_int_equal(glyph_set_file_load(path, &bytes, &set, &diagnostic),
                     GLYPH_SET_FILE_TOO_LARGE);
    assert_null(bytes);
    unlink(path);
}

static void test_an_empty_file_is_invalid_not_unreadable(void **state) {
    char path[32];
    uint8_t *bytes = NULL;
    GlyphSet set;
    GlyphSetFileDiagnostic diagnostic;
    (void)state;
    temp_path(path, sizeof(path));
    assert_int_equal(glyph_set_file_load(path, &bytes, &set, &diagnostic),
                     GLYPH_SET_FILE_INVALID);
    assert_int_equal(diagnostic.set_result, GLYPH_SET_TOO_SHORT);
    assert_null(bytes);
    unlink(path);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_a_valid_set_loads_with_its_entries_intact),
        cmocka_unit_test(test_missing_and_unreadable_are_different_answers),
        cmocka_unit_test(test_a_corrupt_set_is_refused_with_the_format_reason),
        cmocka_unit_test(test_an_oversized_file_is_refused_before_allocating),
        cmocka_unit_test(test_an_empty_file_is_invalid_not_unreadable)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

