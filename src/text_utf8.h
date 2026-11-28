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

/* How a codepoint is treated by the authored-text rules (V1-6 plan, I3).
 *
 * The classification comes from the vendored `utf8proc` data rather than a handwritten table, for the
 * same reason D10 pins the Unicode version to that data: a table someone typed is a table that drifts.
 *
 * - PLAIN may appear in authored text.
 * - CONTROL is category Cc or Cf — invisible characters with no place in a single-line label, including
 *   the bidi embeddings, overrides and isolates. ZWJ and ZWNJ are deliberately **not** controls: they are
 *   grapheme-cluster machinery and refusing them would make emoji sequences unrepresentable.
 * - RIGHT_TO_LEFT is bidi class R or AL on a *visible* character. V1-6 refuses these at ingest rather
 *   than drawing them left-to-right (D-C).
 */
typedef enum {
    TEXT_UTF8_CLASS_PLAIN = 0,
    TEXT_UTF8_CLASS_CONTROL,
    TEXT_UTF8_CLASS_RIGHT_TO_LEFT
} TextUtf8Class;

/* Stable short name for a class, for tool output and test failure messages. */
const char *text_utf8_class_name(TextUtf8Class class_);

/* Classify one codepoint. Values above U+10FFFF are CONTROL: they cannot occur in valid text, so a
   caller holding one is holding a decoder bug, and "unusable" is the safe answer. */
TextUtf8Class text_utf8_classify(uint32_t codepoint);

/**
 * Read one codepoint starting at `*offset`.
 *
 * The per-codepoint companion to `text_utf8_next_cluster`: a cluster is a drawing unit, a codepoint is a
 * character, and character rules (control, direction, backslash) must look at each codepoint rather than
 * only at a cluster's first. Advances `*offset` past the codepoint; on any refusal `*offset` is unchanged,
 * so a caller can report exactly where the problem starts.
 */
TextUtf8Result text_utf8_next_codepoint(const char *bytes, size_t size, size_t *offset,
                                        uint32_t *out_codepoint);

#endif /* TEXT_UTF8_H */
