/**
 * test_sha256.c — SHA-256 against published vectors
 *
 * The digest is load-bearing for the font contract, so it is checked against FIPS 180-4's own
 * vectors rather than against itself. A round-trip test would pass for a digest that is wrong in
 * exactly the same way every time; these would not.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <string.h>

#include "../src/sha256.h"

static void test_empty_string_matches_the_published_vector(void **state) {
    char hex[SHA256_HEX_SIZE];
    (void)state;
    sha256_digest_hex(NULL, 0U, hex);
    assert_string_equal(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

static void test_abc_matches_the_published_vector(void **state) {
    char hex[SHA256_HEX_SIZE];
    (void)state;
    sha256_digest_hex("abc", 3U, hex);
    assert_string_equal(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

/* 56 bytes: the case where the 0x80 byte does not fit before the length field, so the padding
   spills into a second block. This is the one padding bug that survives casual testing. */
static void test_two_block_message_matches_the_published_vector(void **state) {
    static const char message[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    char hex[SHA256_HEX_SIZE];
    (void)state;
    assert_int_equal(strlen(message), 56U);
    sha256_digest_hex(message, strlen(message), hex);
    assert_string_equal(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

static void test_million_a_matches_the_published_vector(void **state) {
    static char block[1000];
    uint8_t digest[SHA256_DIGEST_SIZE];
    char hex[SHA256_HEX_SIZE];
    Sha256 context;
    size_t index;
    (void)state;
    memset(block, 'a', sizeof(block));
    sha256_init(&context);
    for (index = 0U; index < 1000U; index++) {
        sha256_update(&context, block, sizeof(block));
    }
    sha256_final(&context, digest);
    sha256_to_hex(digest, hex);
    assert_string_equal(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

static void test_streaming_matches_one_shot_at_every_split(void **state) {
    static const char message[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    uint8_t one_shot[SHA256_DIGEST_SIZE];
    size_t split;
    (void)state;
    sha256_digest(message, strlen(message), one_shot);
    for (split = 0U; split <= strlen(message); split++) {
        Sha256 context;
        uint8_t streamed[SHA256_DIGEST_SIZE];
        sha256_init(&context);
        sha256_update(&context, message, split);
        sha256_update(&context, message + split, strlen(message) - split);
        sha256_final(&context, streamed);
        assert_memory_equal(streamed, one_shot, sizeof(one_shot));
    }
}

static void test_hex_round_trip_and_strict_parse(void **state) {
    uint8_t digest[SHA256_DIGEST_SIZE];
    uint8_t parsed[SHA256_DIGEST_SIZE];
    uint8_t untouched[SHA256_DIGEST_SIZE];
    char hex[SHA256_HEX_SIZE];
    char upper[SHA256_HEX_SIZE];
    size_t index;
    (void)state;
    sha256_digest("font", 4U, digest);
    sha256_to_hex(digest, hex);
    assert_int_equal(strlen(hex), 64U);
    assert_true(sha256_from_hex(hex, parsed));
    assert_memory_equal(parsed, digest, sizeof(digest));
    /* Uppercase hex is the same digest; a manifest may be typed either way. */
    for (index = 0U; index < 64U; index++) {
        char character = hex[index];
        upper[index] = (character >= 'a' && character <= 'f')
                           ? (char)(character - 'a' + 'A')
                           : character;
    }
    upper[64] = '\0';
    assert_true(sha256_from_hex(upper, parsed));
    assert_memory_equal(parsed, digest, sizeof(digest));
    /* Strictness: short, long, non-hex and empty are all rejections, and the output buffer is
       left alone so a caller cannot mistake a rejection for a zero digest. */
    memset(untouched, 0x5a, sizeof(untouched));
    memcpy(parsed, untouched, sizeof(parsed));
    assert_false(sha256_from_hex("abc", parsed));
    assert_false(sha256_from_hex("", parsed));
    assert_false(sha256_from_hex(NULL, parsed));
    {
        char too_long[82];
        memcpy(too_long, hex, 64U);
        memcpy(too_long + 64U, "00", 3U);
        assert_false(sha256_from_hex(too_long, parsed));
    }
    {
        char non_hex[SHA256_HEX_SIZE];
        memcpy(non_hex, hex, sizeof(non_hex));
        non_hex[10] = 'g';
        assert_false(sha256_from_hex(non_hex, parsed));
    }
    assert_memory_equal(parsed, untouched, sizeof(parsed));
}

static void test_null_boundaries_are_tolerated(void **state) {
    (void)state;
    sha256_init(NULL);
    sha256_update(NULL, "x", 1U);
    sha256_final(NULL, NULL);
    sha256_to_hex(NULL, NULL);
    assert_false(sha256_from_hex("abc", NULL));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_empty_string_matches_the_published_vector),
        cmocka_unit_test(test_abc_matches_the_published_vector),
        cmocka_unit_test(test_two_block_message_matches_the_published_vector),
        cmocka_unit_test(test_million_a_matches_the_published_vector),
        cmocka_unit_test(test_streaming_matches_one_shot_at_every_split),
        cmocka_unit_test(test_hex_round_trip_and_strict_parse),
        cmocka_unit_test(test_null_boundaries_are_tolerated)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
