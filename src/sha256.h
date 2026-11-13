/**
 * sha256.h — SHA-256 digest, so a font file can be identified by its bytes
 *
 * The font contract (V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md, D12) identifies a face by the
 * SHA-256 of every file it ships. Filenames and font family names are routinely wrong and
 * trivially edited; a digest is neither. This module exists so the converter, the build and the
 * runtime can all ask the same question — "are these the bytes we converted?" — without a
 * dependency, a subprocess, or a platform-specific tool.
 *
 * Streaming by design: fonts are read in bounded chunks, never slurped whole, so a 40 MB CJK face
 * costs one 64-byte context and a caller-owned buffer.
 *
 * Not a general crypto surface. No HMAC, no keyed modes, no constant-time claims: this compares
 * provenance, it does not authenticate an adversary.
 */

#ifndef SHA256_H
#define SHA256_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_SIZE 32U
#define SHA256_BLOCK_SIZE 64U
/* 32 digest bytes as lowercase hex, plus the terminator. */
#define SHA256_HEX_SIZE 65U

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[SHA256_BLOCK_SIZE];
    size_t buffer_length;
} Sha256;

/* Any context may be re-initialised and reused; init is total. */
void sha256_init(Sha256 *context);

/* Incremental input. `data` may be NULL only when `length` is zero. */
void sha256_update(Sha256 *context, const void *data, size_t length);

/* Writes the digest and wipes the context, so a finished context cannot be fed again by
   accident. `out_digest` must hold SHA256_DIGEST_SIZE bytes. */
void sha256_final(Sha256 *context, uint8_t out_digest[SHA256_DIGEST_SIZE]);

/* One-shot digest over a contiguous buffer. */
void sha256_digest(const void *data, size_t length,
                   uint8_t out_digest[SHA256_DIGEST_SIZE]);

/* Lowercase hex, NUL-terminated. `out_hex` must hold SHA256_HEX_SIZE bytes. */
void sha256_digest_hex(const void *data, size_t length, char out_hex[SHA256_HEX_SIZE]);

/* Format an existing digest as lowercase hex. */
void sha256_to_hex(const uint8_t digest[SHA256_DIGEST_SIZE],
                   char out_hex[SHA256_HEX_SIZE]);

/* Strict parse: exactly SHA256_HEX_SIZE - 1 hex digits, either case, nothing else. Returns false
   without touching `out_digest` on any deviation, so a truncated or over-long manifest value is a
   rejection rather than a guess. */
bool sha256_from_hex(const char *hex, uint8_t out_digest[SHA256_DIGEST_SIZE]);

#endif /* SHA256_H */
