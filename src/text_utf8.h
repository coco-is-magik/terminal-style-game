/**
 * text_utf8.h — strict UTF-8, canonical NFC, and bounded grapheme clusters
 *
 * V1-4 **D3** selects vendored `utf8proc` for this and rejects a hand-rolled decoder as a correctness
 * trap. This module is the project's only door to it, for the same reason `glyph_source` is the only
 * door to font precedence: one place decides what "valid UTF-8" means, one decides what a cluster is,
 * and one imposes the bound that keeps a cluster a *cell*.
 *
 * Three rules that are decisions, not defaults:
 *
 * - **Strict.** Overlong encodings, surrogates and anything above U+10FFFF are malformed. A text path
 *   that accepts them is a text path that will one day write them out again.
 * - **Canonical NFC.** NFKC is deliberately not offered: it rewrites creative content, and V1-4's
 *   locked product model says it does not.
 * - **A cluster is bounded.** `TEXT_UTF8_MAX_CLUSTER_BYTES` exists because one glyph-art cell stores
 *   one cluster *only when the face resolves it to one bounded cell image*. An unbounded ZWJ sequence
 *   is refused here, where the refusal can be explained, rather than silently overflowing a cell.
 *
 * The vendored release is pinned (see `vendor/src/utf8proc`, tag recorded in the V1-5 plan), because
 * D10 pins the Unicode version to the vendored data rather than to "latest".
 */

#ifndef TEXT_UTF8_H
#define TEXT_UTF8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Longer than any cluster the declared faces can resolve to one cell. Refused, not truncated. */
#define TEXT_UTF8_MAX_CLUSTER_BYTES 64U

typedef enum {
    TEXT_UTF8_OK = 0,
    /* A caller-supplied argument was unusable: no buffer, no output, or an offset past the end. */
    TEXT_UTF8_INVALID_ARGUMENT,
    /* The bytes are not valid UTF-8: bad lead byte, truncated sequence, overlong form, surrogate or
       a value above U+10FFFF. */
    TEXT_UTF8_MALFORMED,
    /* A legitimate status, not an error: the offset reached the end of the input. */
    TEXT_UTF8_END,
    /* One cluster exceeded the cell bound. Refused rather than swallowed. */
    TEXT_UTF8_CLUSTER_TOO_LONG,
    /* The caller's buffer cannot hold the result. Nothing partial is written. */
    TEXT_UTF8_BUFFER_TOO_SMALL,
    /* `utf8proc` could not allocate while normalising. Transient, not a property of the text. */
    TEXT_UTF8_NO_MEMORY
} TextUtf8Result;

/* Stable short name for a result, for tool output and test failure messages. */
const char *text_utf8_result_name(TextUtf8Result result);

/* True when `size` bytes are strictly valid UTF-8. An empty input is valid. */
TextUtf8Result text_utf8_validate(const char *bytes, size_t size);

/**
 * Normalise to canonical NFC.
 *
 * Writes at most `out_capacity` bytes to `out` and reports the length in `*out_size`. On
 * TEXT_UTF8_BUFFER_TOO_SMALL nothing is written and `*out_size` is 0.
 *
 * The library allocates a transient buffer of the input's size for the mapping and frees it before
 * returning; that is the one allocation in this module, it is bounded by the input, and it is why
 * this is not on the per-frame path.
 */
TextUtf8Result text_utf8_normalize_nfc(const char *bytes, size_t size, char *out,
                                       size_t out_capacity, size_t *out_size);

/**
 * Read one extended grapheme cluster starting at `*offset`.
 *
 * Advances `*offset` past the cluster and reports the cluster's first codepoint and its byte length.
 * The first codepoint is what a cell would store: one cluster, one codepoint identity, which is the
 * model V1-4 locked ("one glyph-art cell stores one normalized extended grapheme cluster only when
 * the selected face resolves it to one bounded cell image"). A cluster whose first codepoint is not
 * the one a face should draw is a later, per-face question — not this module's.
 *
 * Returns TEXT_UTF8_END with `*offset == size` at the end, and leaves `*offset` unchanged on any
 * refusal.
 */
TextUtf8Result text_utf8_next_cluster(const char *bytes, size_t size, size_t *offset,
                                      uint32_t *out_first_codepoint, size_t *out_length);

#endif /* TEXT_UTF8_H */
