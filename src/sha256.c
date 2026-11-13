/**
 * sha256.c — FIPS 180-4 SHA-256
 *
 * Straight transcription of the standard, deliberately: no tables generated at runtime, no
 * unrolling tricks, no platform intrinsics. Correctness here is checked against published
 * vectors in tests/test_sha256.c, and the only way this file is allowed to be interesting is if
 * one of those vectors fails.
 */

#include "sha256.h"

#include <string.h>

static const uint32_t SHA256_ROUND_CONSTANTS[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
    0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
    0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
    0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static const uint32_t SHA256_INITIAL_STATE[8] = {
    0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
    0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
};

#define ROTATE_RIGHT(value, bits) \
    (((value) >> (bits)) | ((value) << (32U - (bits))))

/* The four logical functions of §4.1.2. Each is written exactly as the standard names it. */
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BIG_SIGMA_0(x) (ROTATE_RIGHT(x, 2U) ^ ROTATE_RIGHT(x, 13U) ^ ROTATE_RIGHT(x, 22U))
#define BIG_SIGMA_1(x) (ROTATE_RIGHT(x, 6U) ^ ROTATE_RIGHT(x, 11U) ^ ROTATE_RIGHT(x, 25U))
#define SMALL_SIGMA_0(x) (ROTATE_RIGHT(x, 7U) ^ ROTATE_RIGHT(x, 18U) ^ ((x) >> 3U))
#define SMALL_SIGMA_1(x) (ROTATE_RIGHT(x, 17U) ^ ROTATE_RIGHT(x, 19U) ^ ((x) >> 10U))

static uint32_t read_be32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24U) | ((uint32_t)bytes[1] << 16U) |
           ((uint32_t)bytes[2] << 8U) | (uint32_t)bytes[3];
}

static void write_be32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24U);
    bytes[1] = (uint8_t)(value >> 16U);
    bytes[2] = (uint8_t)(value >> 8U);
    bytes[3] = (uint8_t)value;
}

static void sha256_compress(Sha256 *context, const uint8_t block[SHA256_BLOCK_SIZE]) {
    uint32_t schedule[64];
    uint32_t a, b, c, d, e, f, g, h;
    size_t round;
    for (round = 0U; round < 16U; round++) {
        schedule[round] = read_be32(block + round * 4U);
    }
    for (round = 16U; round < 64U; round++) {
        schedule[round] = SMALL_SIGMA_1(schedule[round - 2U]) + schedule[round - 7U] +
                          SMALL_SIGMA_0(schedule[round - 15U]) + schedule[round - 16U];
    }
    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    f = context->state[5];
    g = context->state[6];
    h = context->state[7];
    for (round = 0U; round < 64U; round++) {
        uint32_t temporary_1 = h + BIG_SIGMA_1(e) + CH(e, f, g) +
                               SHA256_ROUND_CONSTANTS[round] + schedule[round];
        uint32_t temporary_2 = BIG_SIGMA_0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + temporary_1;
        d = c;
        c = b;
        b = a;
        a = temporary_1 + temporary_2;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}


void sha256_init(Sha256 *context) {
    if (!context) return;
    memcpy(context->state, SHA256_INITIAL_STATE, sizeof(context->state));
    context->bit_count = 0U;
    context->buffer_length = 0U;
    memset(context->buffer, 0, sizeof(context->buffer));
}

void sha256_update(Sha256 *context, const void *data, size_t length) {
    const uint8_t *bytes = (const uint8_t *)data;
    if (!context || (!data && length != 0U)) return;
    context->bit_count += (uint64_t)length * 8U;
    while (length > 0U) {
        size_t free_bytes = SHA256_BLOCK_SIZE - context->buffer_length;
        size_t take = length < free_bytes ? length : free_bytes;
        memcpy(context->buffer + context->buffer_length, bytes, take);
        context->buffer_length += take;
        bytes += take;
        length -= take;
        if (context->buffer_length == SHA256_BLOCK_SIZE) {
            sha256_compress(context, context->buffer);
            context->buffer_length = 0U;
        }
    }
}

void sha256_final(Sha256 *context, uint8_t out_digest[SHA256_DIGEST_SIZE]) {
    uint64_t bit_count;
    size_t index;
    if (!context || !out_digest) return;
    bit_count = context->bit_count;
    /* 0x80, then zeros, then the 64-bit length: keeping bit_count apart from buffer_length is
       what lets the length survive the padding. */
    context->buffer[context->buffer_length++] = 0x80U;
    if (context->buffer_length > SHA256_BLOCK_SIZE - 8U) {
        memset(context->buffer + context->buffer_length, 0,
               SHA256_BLOCK_SIZE - context->buffer_length);
        sha256_compress(context, context->buffer);
        context->buffer_length = 0U;
    }
    memset(context->buffer + context->buffer_length, 0,
           SHA256_BLOCK_SIZE - context->buffer_length);
    write_be32(context->buffer + SHA256_BLOCK_SIZE - 8U, (uint32_t)(bit_count >> 32U));
    write_be32(context->buffer + SHA256_BLOCK_SIZE - 4U, (uint32_t)bit_count);
    sha256_compress(context, context->buffer);
    for (index = 0U; index < 8U; index++) {
        write_be32(out_digest + index * 4U, context->state[index]);
    }
    /* A finalised context must not be resumable: zeroing makes an accidental second
       sha256_final produce a wrong digest rather than a plausible one. */
    memset(context, 0, sizeof(*context));
}

void sha256_digest(const void *data, size_t length,
                   uint8_t out_digest[SHA256_DIGEST_SIZE]) {
    Sha256 context;
    sha256_init(&context);
    sha256_update(&context, data, length);
    sha256_final(&context, out_digest);
}

static char hex_digit(unsigned value) {
    return (char)(value < 10U ? ('0' + value) : ('a' + (value - 10U)));
}

static int hex_value(char character) {
    if (character >= '0' && character <= '9') return (int)(character - '0');
    if (character >= 'a' && character <= 'f') return ((int)(character - 'a')) + 10;
    if (character >= 'A' && character <= 'F') return ((int)(character - 'A')) + 10;
    return -1;
}

void sha256_to_hex(const uint8_t digest[SHA256_DIGEST_SIZE],
                   char out_hex[SHA256_HEX_SIZE]) {
    size_t index;
    if (!digest || !out_hex) return;
    for (index = 0U; index < SHA256_DIGEST_SIZE; index++) {
        out_hex[index * 2U] = hex_digit((unsigned)(digest[index] >> 4U));
        out_hex[index * 2U + 1U] = hex_digit((unsigned)(digest[index] & 0x0fU));
    }
    out_hex[SHA256_HEX_SIZE - 1U] = '\0';
}

void sha256_digest_hex(const void *data, size_t length, char out_hex[SHA256_HEX_SIZE]) {
    uint8_t digest[SHA256_DIGEST_SIZE];
    sha256_digest(data, length, digest);
    sha256_to_hex(digest, out_hex);
}

bool sha256_from_hex(const char *hex, uint8_t out_digest[SHA256_DIGEST_SIZE]) {
    uint8_t parsed[SHA256_DIGEST_SIZE];
    size_t index;
    if (!hex || !out_digest) return false;
    for (index = 0U; index < SHA256_DIGEST_SIZE; index++) {
        int high = hex_value(hex[index * 2U]);
        int low = hex_value(hex[index * 2U + 1U]);
        if (high < 0 || low < 0) return false;
        parsed[index] = (uint8_t)((high << 4) | low);
    }
    /* Exactly 64 digits: a 63- or 65-digit value is a different thing, not this thing. */
    if (hex[SHA256_DIGEST_SIZE * 2U] != '\0') return false;
    memcpy(out_digest, parsed, sizeof(parsed));
    return true;
}
