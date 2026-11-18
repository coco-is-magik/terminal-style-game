/**
 * test_font_manifest.c — the font contract's codec
 *
 * Two halves. The happy path proves a realistic manifest round-trips through canonical text
 * unchanged. The rejection tests matter more: this is hand-edited text that decides which bytes
 * become the app's glyphs, so every way of being wrong must be named rather than tolerated — and
 * a rejection must leave the caller's manifest untouched.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/font_manifest.h"

#define DIGEST_A "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
#define DIGEST_B "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"

static const char VALID_ONE_FACE[] =
    "# The reference face: it proves the cell size is achievable at all.\n"
    "font=jp\n"
    "name=Misaki Gothic\n"
    "file=misaki_gothic.ttf\n"
    "sha256=" DIGEST_A "\n"
    "source=https://littlelimit.net/misaki.htm\n"
    "licence=M+ FONTS\n"
    "cell=8x8\n";

static const char VALID_TWO_FACES[] =
    "font=jp\n"
    "name=Misaki Gothic\n"
    "file=misaki_gothic.ttf\n"
    "sha256=" DIGEST_A "\n"
    "source=https://littlelimit.net/misaki.htm\n"
    "licence=M+ FONTS\n"
    "version=2021-05-05\n"
    "coverage=JIS X 0208\n"
    "cell=8x8\n"
    "\n"
    "font=latin\n"
    "name=Noto Sans Mono\n"
    "file=NotoSansMono-Regular.ttf\n"
    "sha256=" DIGEST_B "\n"
    "source=https://fonts.google.com/noto/specimen/Noto+Sans+Mono\n"
    "licence=OFL-1.1\n"
    "cell=8x8\n";

static void test_valid_manifest_parses_every_field(void **state) {
    FontManifest manifest;
    FontManifestDiagnostic diagnostic;
    const FontManifestFace *face;
    (void)state;
    assert_int_equal(font_manifest_read_text(VALID_TWO_FACES, &manifest, &diagnostic),
                     FONT_MANIFEST_OK);
    assert_int_equal(diagnostic.result, FONT_MANIFEST_OK);
    assert_int_equal(manifest.face_count, 2U);

    face = font_manifest_find_face(&manifest, "jp");
    assert_non_null(face);
    assert_string_equal(face->name, "Misaki Gothic");
    assert_string_equal(face->source, "https://littlelimit.net/misaki.htm");
    assert_string_equal(face->licence, "M+ FONTS");
    assert_string_equal(face->version, "2021-05-05");
    assert_string_equal(face->coverage, "JIS X 0208");
    assert_int_equal(face->file_count, 1U);
    assert_string_equal(face->files[0].file, "misaki_gothic.ttf");
    {
        /* The empty-string digest, spelled out: a typo here would make every later comparison
           agree with itself. */
        uint8_t expected[32];
        assert_true(sha256_from_hex(DIGEST_A, expected));
        assert_memory_equal(face->files[0].sha256, expected, 32U);
    }

    /* Optional fields are absent for the second face, not defaulted to anything. */
    face = font_manifest_find_face(&manifest, "latin");
    assert_non_null(face);
    assert_string_equal(face->version, "");
    assert_string_equal(face->coverage, "");
    assert_null(font_manifest_find_face(&manifest, "JP")); /* case-sensitive */
    assert_null(font_manifest_find_face(&manifest, "kana"));
}

static void test_canonical_text_is_stable_and_reparses_identically(void **state) {
    FontManifest parsed;
    FontManifest reparsed;
    char first[4096];
    char second[4096];
    size_t first_length = 0U;
    (void)state;
    assert_int_equal(font_manifest_read_text(VALID_TWO_FACES, &parsed, NULL), FONT_MANIFEST_OK);
    assert_int_equal(font_manifest_write_text(&parsed, first, sizeof(first), &first_length),
                     FONT_MANIFEST_OK);
    assert_int_equal(first_length, strlen(first));
    /* Comments and blank lines are not part of the canonical form; key order is. */
    assert_non_null(strstr(first, "font=jp\nname=Misaki Gothic\nfile=misaki_gothic.ttf\n"
                                  "sha256=" DIGEST_A "\nsource="));
    assert_null(strstr(first, "#"));
    assert_int_equal(font_manifest_write_text(&parsed, second, sizeof(second), NULL),
                     FONT_MANIFEST_OK);
    assert_string_equal(first, second);
    assert_int_equal(font_manifest_read_text(first, &reparsed, NULL), FONT_MANIFEST_OK);
    assert_int_equal(font_manifest_write_text(&reparsed, second, sizeof(second), NULL),
                     FONT_MANIFEST_OK);
    assert_string_equal(first, second);
}

static void test_multiple_files_form_one_face(void **state) {
    static const char text[] =
        "font=jp\n"
        "name=Misaki Gothic\n"
        "file=misaki_gothic.ttf\n"
        "sha256=" DIGEST_A "\n"
        "file=misaki_mincho.ttf\n"
        "sha256=" DIGEST_B "\n"
        "source=https://littlelimit.net/misaki.htm\n"
        "licence=M+ FONTS\n"
        "cell=8x8\n";
    FontManifest manifest;
    (void)state;
    assert_int_equal(font_manifest_read_text(text, &manifest, NULL), FONT_MANIFEST_OK);
    assert_int_equal(manifest.face_count, 1U);
    assert_int_equal(manifest.faces[0].file_count, 2U);
    assert_string_equal(manifest.faces[0].files[1].file, "misaki_mincho.ttf");
}

/* Each rejection is a distinct thing a user can do wrong, and the result names it. */
static void test_structural_rejections_are_named(void **state) {
    FontManifest manifest;
    FontManifestDiagnostic diagnostic;
    (void)state;

    assert_int_equal(font_manifest_read_text(NULL, &manifest, &diagnostic),
                     FONT_MANIFEST_INVALID_ARGUMENT);
    assert_int_equal(font_manifest_read_text(VALID_ONE_FACE, NULL, &diagnostic),
                     FONT_MANIFEST_INVALID_ARGUMENT);
    /* Empty text has no face at all. */
    assert_int_equal(font_manifest_read_text("", &manifest, &diagnostic),
                     FONT_MANIFEST_MISSING_KEY);
    /* A key with no '=' is not a key. */
    assert_int_equal(font_manifest_read_text("font=jp\nnonsense\n", &manifest, &diagnostic),
                     FONT_MANIFEST_MALFORMED);
    /* Unknown key, with the line number that carries it. */
    assert_int_equal(font_manifest_read_text("font=jp\nfontsize=8\n", &manifest, &diagnostic),
                     FONT_MANIFEST_UNKNOWN_KEY);
    assert_int_equal(diagnostic.result, FONT_MANIFEST_UNKNOWN_KEY);
    assert_int_equal(diagnostic.line, 2U);
    /* Keys outside a face belong to nobody. */
    assert_int_equal(font_manifest_read_text("name=orphan\nfont=jp\n", &manifest, &diagnostic),
                     FONT_MANIFEST_MISSING_KEY);
    assert_string_equal(diagnostic.key, "font");
}

/* A face that cannot be found again is not a face: `source` is required, not decorative. */
static void test_required_and_paired_keys_are_enforced(void **state) {
    FontManifest manifest;
    FontManifestDiagnostic diagnostic;
    (void)state;
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=Misaki\n"
                                             "file=a.ttf\n"
                                             "sha256=" DIGEST_A "\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x8\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_MISSING_KEY);
    assert_string_equal(diagnostic.key, "source");
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=Misaki\n"
                                             "file=a.ttf\n"
                                             "sha256=" DIGEST_A "\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_MISSING_KEY);
    assert_string_equal(diagnostic.key, "cell");
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=Misaki\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x8\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_MISSING_KEY);
    assert_string_equal(diagnostic.key, "file");
    /* A file without its hash. */
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=Misaki\n"
                                             "file=a.ttf\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x8\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_UNPAIRED_FILE);
    /* A hash without its file: the hash of nothing identifies nothing. */
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=Misaki\n"
                                             "sha256=" DIGEST_A "\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x8\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_UNPAIRED_FILE);
    /* Two files, one hash. */
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=Misaki\n"
                                             "file=a.ttf\n"
                                             "file=b.ttf\n"
                                             "sha256=" DIGEST_A "\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x8\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_UNPAIRED_FILE);
}

static void test_values_are_validated_not_coerced(void **state) {
    FontManifest manifest;
    FontManifestDiagnostic diagnostic;
    (void)state;
    /* The cell is the renderer's answer. A manifest may not have a different one. */
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=Misaki\n"
                                             "file=a.ttf\n"
                                             "sha256=" DIGEST_A "\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x16\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_INVALID_CELL);
    assert_string_equal(diagnostic.key, "cell");
    /* Truncated hash. */
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=Misaki\n"
                                             "file=a.ttf\n"
                                             "sha256=e3b0c442\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x8\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_INVALID_VALUE);
    assert_string_equal(diagnostic.key, "sha256");
    /* Empty value. */
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=\n"
                                             "file=a.ttf\n"
                                             "sha256=" DIGEST_A "\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x8\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_INVALID_VALUE);
    /* An id that cannot be typed on a command line without quoting. */
    assert_int_equal(font_manifest_read_text("font=Jp Face!\n"
                                             "name=Misaki\n"
                                             "file=a.ttf\n"
                                             "sha256=" DIGEST_A "\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x8\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_INVALID_VALUE);
    /* Duplicate singleton key: two names, no way to know which one is the face. */
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=One\n"
                                             "name=Two\n"
                                             "file=a.ttf\n"
                                             "sha256=" DIGEST_A "\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x8\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_DUPLICATE_KEY);
    assert_string_equal(diagnostic.key, "name");
    /* The same file twice in one face: one of the two hashes describes nothing. */
    assert_int_equal(font_manifest_read_text("font=jp\n"
                                             "name=One\n"
                                             "file=a.ttf\n"
                                             "sha256=" DIGEST_A "\n"
                                             "file=a.ttf\n"
                                             "sha256=" DIGEST_B "\n"
                                             "source=https://example.invalid\n"
                                             "licence=M+ FONTS\n"
                                             "cell=8x8\n",
                                             &manifest, &diagnostic),
                     FONT_MANIFEST_DUPLICATE_KEY);
}


static void test_rejection_leaves_the_caller_manifest_untouched(void **state) {
    FontManifest manifest;
    FontManifest before;
    (void)state;
    assert_int_equal(font_manifest_read_text(VALID_ONE_FACE, &manifest, NULL), FONT_MANIFEST_OK);
    before = manifest;
    /* A second parse that fails must not have written through the caller's pointer. The cell
       error wins over the missing keys: a value that is definitely wrong is reported before a
       count of what is absent, because it is the more specific thing to say. */
    assert_int_equal(font_manifest_read_text("font=jp\ncell=8x16\n", &manifest, NULL),
                     FONT_MANIFEST_INVALID_CELL);
    assert_int_equal(manifest.face_count, before.face_count);
    assert_string_equal(manifest.faces[0].id, before.faces[0].id);
    assert_int_equal(font_manifest_read_text("font=jp\nname=Only a name\n", &manifest, NULL),
                     FONT_MANIFEST_MISSING_KEY);
    assert_int_equal(manifest.face_count, before.face_count);
    assert_string_equal(manifest.faces[0].id, before.faces[0].id);
}

static void test_writer_reports_small_buffers_and_incomplete_faces(void **state) {
    FontManifest manifest;
    char tiny[16];
    char buffer[4096];
    (void)state;
    assert_int_equal(font_manifest_read_text(VALID_ONE_FACE, &manifest, NULL), FONT_MANIFEST_OK);
    assert_int_equal(font_manifest_write_text(&manifest, tiny, sizeof(tiny), NULL),
                     FONT_MANIFEST_BUFFER_TOO_SMALL);
    assert_int_equal(tiny[0], '\0'); /* never a half-written manifest */
    assert_int_equal(font_manifest_write_text(NULL, buffer, sizeof(buffer), NULL),
                     FONT_MANIFEST_INVALID_ARGUMENT);
    assert_int_equal(font_manifest_write_text(&manifest, NULL, sizeof(buffer), NULL),
                     FONT_MANIFEST_INVALID_ARGUMENT);
    manifest.faces[0].file_count = 0U;
    assert_int_equal(font_manifest_write_text(&manifest, buffer, sizeof(buffer), NULL),
                     FONT_MANIFEST_MISSING_KEY);
}

static void test_result_names_are_stable(void **state) {
    (void)state;
    assert_string_equal(font_manifest_result_name(FONT_MANIFEST_OK), "OK");
    assert_string_equal(font_manifest_result_name(FONT_MANIFEST_INVALID_CELL), "INVALID_CELL");
    assert_string_equal(font_manifest_result_name(FONT_MANIFEST_UNPAIRED_FILE),
                        "UNPAIRED_FILE");
    assert_string_equal(font_manifest_result_name(FONT_MANIFEST_DUPLICATE_FACE),
                        "DUPLICATE_FACE");
}

/* The shipped example is documentation, and documentation that no longer parses is worse than
   none: someone will follow it, get a rejection, and conclude the format is broken. This test
   reads the real file from the repository, so the example cannot drift out of the format without
   failing the suite. It is run from the repository root, like every other runner. */
static void test_shipped_example_manifest_is_valid(void **state) {
    static const char path[] = "assets/fonts.example.txt";
    FontManifest manifest;
    FontManifestDiagnostic diagnostic;
    char *text = NULL;
    long size;
    FILE *file = fopen(path, "rb");
    (void)state;
    assert_non_null(file);
    assert_int_equal(fseek(file, 0L, SEEK_END), 0);
    size = ftell(file);
    assert_true(size > 0L);
    assert_true((size_t)size < FONT_MANIFEST_TEXT_MAX);
    assert_int_equal(fseek(file, 0L, SEEK_SET), 0);
    text = malloc((size_t)size + 1U);
    assert_non_null(text);
    assert_int_equal(fread(text, 1U, (size_t)size, file), (size_t)size);
    text[size] = '\0';
    assert_int_equal(fclose(file), 0);

    assert_int_equal(font_manifest_read_text(text, &manifest, &diagnostic), FONT_MANIFEST_OK);
    free(text);

    /* The shape the example documents: a two-file face and a one-file face, both Japanese, both
       at the renderer's cell size. */
    assert_int_equal(manifest.face_count, 2U);
    assert_non_null(font_manifest_find_face(&manifest, "misaki-gothic"));
    assert_non_null(font_manifest_find_face(&manifest, "misaki-mincho"));
    assert_int_equal(manifest.faces[0].file_count, 2U);
    assert_int_equal(manifest.faces[1].file_count, 1U);
    /* Every digest in the example is a placeholder. If one ever becomes a real digest, the file
       has stopped being an example and started being a stale claim about someone's bytes. */
    {
        uint8_t zero[SHA256_DIGEST_SIZE];
        unsigned face_index;
        memset(zero, 0, sizeof(zero));
        for (face_index = 0U; face_index < manifest.face_count; face_index++) {
            unsigned file_index;
            for (file_index = 0U; file_index < manifest.faces[face_index].file_count;
                 file_index++) {
                assert_memory_equal(manifest.faces[face_index].files[file_index].sha256, zero,
                                    sizeof(zero));
            }
        }
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_valid_manifest_parses_every_field),
        cmocka_unit_test(test_canonical_text_is_stable_and_reparses_identically),
        cmocka_unit_test(test_multiple_files_form_one_face),
        cmocka_unit_test(test_structural_rejections_are_named),
        cmocka_unit_test(test_required_and_paired_keys_are_enforced),
        cmocka_unit_test(test_values_are_validated_not_coerced),
        cmocka_unit_test(test_rejection_leaves_the_caller_manifest_untouched),
        cmocka_unit_test(test_writer_reports_small_buffers_and_incomplete_faces),
        cmocka_unit_test(test_result_names_are_stable),
        cmocka_unit_test(test_shipped_example_manifest_is_valid)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
