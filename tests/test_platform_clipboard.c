/*
 * test_platform_clipboard.c — the clipboard policy without a desktop (V1-6 I5, D-E)
 *
 * The one system call cannot be tested here; what can be tested is the decision it feeds: nothing to
 * paste is a status, too much to paste is a refusal, and nothing partial ever reaches the caller.
 */

#include <cmocka.h>

#include "../src/platform_clipboard.h"
#include "../src/platform_clipboard_internal.h"

#include <string.h>

typedef struct {
    const char *text;
} ClipFixture;

static const char *fixture_read(void *context) {
    return ((const ClipFixture *)context)->text;
}

static void test_empty_clipboard_is_a_status(void **state) {
    PlatformClipboardSource source;
    ClipFixture fixture;
    char buffer[16];
    size_t size = 99U;
    (void)state;

    /* NULL from the platform means "no text", not "an error". */
    fixture.text = NULL;
    source.read = fixture_read;
    source.context = &fixture;
    source.failing = false;
    buffer[0] = 'x';
    assert_int_equal(platform_clipboard_internal_read_text(&source, buffer, sizeof(buffer), &size),
                     PLATFORM_CLIPBOARD_EMPTY);
    assert_int_equal(buffer[0], '\0');
    assert_int_equal(size, 0U);

    /* An empty string is the same status. */
    fixture.text = "";
    assert_int_equal(platform_clipboard_internal_read_text(&source, buffer, sizeof(buffer), &size),
                     PLATFORM_CLIPBOARD_EMPTY);
    assert_int_equal(size, 0U);
}

static void test_text_is_copied_with_room_for_the_terminator(void **state) {
    PlatformClipboardSource source;
    ClipFixture fixture;
    char buffer[8];
    size_t size = 0U;
    (void)state;

    fixture.text = "paste";
    source.read = fixture_read;
    source.context = &fixture;
    source.failing = false;
    assert_int_equal(platform_clipboard_internal_read_text(&source, buffer, sizeof(buffer), &size),
                     PLATFORM_CLIPBOARD_OK);
    assert_string_equal(buffer, "paste");
    assert_int_equal(size, 5U);

    /* Five bytes of text need six bytes of buffer. Given five, the last byte holds the terminator. */
    assert_int_equal(platform_clipboard_internal_read_text(&source, buffer, 6U, &size),
                     PLATFORM_CLIPBOARD_OK);
    assert_int_equal(platform_clipboard_internal_read_text(&source, buffer, 5U, &size),
                     PLATFORM_CLIPBOARD_TOO_LARGE);
}

static void test_too_large_is_refused_and_writes_nothing(void **state) {
    PlatformClipboardSource source;
    ClipFixture fixture;
    char buffer[8];
    size_t size = 0U;
    (void)state;

    fixture.text = "abcdefgh";
    source.read = fixture_read;
    source.context = &fixture;
    source.failing = false;
    memcpy(buffer, "keep", sizeof("keep"));
    /* Four bytes of room, nine bytes on the clipboard. */
    assert_int_equal(platform_clipboard_internal_read_text(&source, buffer, 4U, &size),
                     PLATFORM_CLIPBOARD_TOO_LARGE);
    /* Not even truncated to "abc": the caller must not be able to paste a prefix by accident. The
       buffer is cleared so nothing stale can be mistaken for the paste. */
    assert_string_equal(buffer, "");
    assert_int_equal(size, 0U);
}

static void test_unavailable_and_arguments_are_distinguished(void **state) {
    PlatformClipboardSource source;
    ClipFixture fixture;
    char buffer[8];
    size_t size = 0U;
    (void)state;

    fixture.text = "text";
    source.read = fixture_read;
    source.context = &fixture;
    source.failing = true;
    assert_int_equal(platform_clipboard_internal_read_text(&source, buffer, sizeof(buffer), &size),
                     PLATFORM_CLIPBOARD_UNAVAILABLE);

    source.failing = false;
    assert_int_equal(platform_clipboard_internal_read_text(&source, NULL, 8U, &size),
                     PLATFORM_CLIPBOARD_INVALID_ARGUMENT);
    assert_int_equal(platform_clipboard_internal_read_text(&source, buffer, 0U, &size),
                     PLATFORM_CLIPBOARD_INVALID_ARGUMENT);
    assert_int_equal(platform_clipboard_internal_read_text(&source, buffer, sizeof(buffer), NULL),
                     PLATFORM_CLIPBOARD_INVALID_ARGUMENT);
    assert_int_equal(platform_clipboard_internal_read_text(NULL, buffer, sizeof(buffer), &size),
                     PLATFORM_CLIPBOARD_INVALID_ARGUMENT);
    assert_string_equal(platform_clipboard_result_name(PLATFORM_CLIPBOARD_TOO_LARGE), "TOO_LARGE");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_empty_clipboard_is_a_status),
        cmocka_unit_test(test_text_is_copied_with_room_for_the_terminator),
        cmocka_unit_test(test_too_large_is_refused_and_writes_nothing),
        cmocka_unit_test(test_unavailable_and_arguments_are_distinguished)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
