/**
 * ui_text.h — what may live in an authored text field
 *
 * V1-6 outcome 1 makes authored UI content validated UTF-8. "Validated" has to mean something specific
 * and it has to be decided once, so this module is the only place that answers "may this text appear in
 * an authored field?" — the same shape as `glyph_source` for precedence and `text_utf8` for encoding.
 *
 * Four rules, each a decision rather than a default:
 *
 * - **Strict UTF-8.** Overlong forms, surrogates and values above U+10FFFF are refused, and the refusal
 *   names the byte offset where the bad sequence begins.
 * - **No invisible characters.** Category Cc and Cf are refused, which covers NUL, newline and DEL as
 *   well as the bidi embeddings, overrides and isolates. ZWJ and ZWNJ are exempt because they are how a
 *   grapheme cluster is spelled: refusing them would make emoji sequences unrepresentable.
 * - **No backslash.** The `.tui` format is line-oriented `key=value` with no escaping, so a backslash is
 *   the one character a future escape syntax would want. Refusing it keeps that door open.
 * - **No right-to-left text.** V1-6 refuses RTL at ingest with a typed result (plan D-C) rather than
 *   drawing it left-to-right, which would be a silent, invisible wrongness. This is a recorded deviation
 *   from outcome 3's wording, not a completed bidi implementation.
 *
 * Capacity is deliberately *not* checked here: how many bytes a field can hold is its owner's business
 * (256 for `UiDocument.content`), while what may be *said* is the same everywhere.
 *
 * Pure: no I/O, no SDL, no globals.
 */

#ifndef UI_TEXT_H
#define UI_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    UI_TEXT_OK = 0,
    /* No buffer while a size was given, or unusable output pointer. */
    UI_TEXT_INVALID_ARGUMENT,
    /* Not strict UTF-8. `offset` is where the bad sequence starts. */
    UI_TEXT_MALFORMED,
    /* A control or format character (Cc/Cf, joiners excepted). */
    UI_TEXT_CONTROL,
    /* A backslash, which the line format cannot carry unambiguously. */
    UI_TEXT_BACKSLASH,
    /* A right-to-left character, refused at ingest rather than mis-rendered. */
    UI_TEXT_RIGHT_TO_LEFT,
    /* One grapheme cluster longer than a cell can hold (TEXT_UTF8_MAX_CLUSTER_BYTES). */
    UI_TEXT_CLUSTER_TOO_LONG
} UiTextResult;

/* What went wrong, and exactly where. A refusal without a position cannot be fixed by the person who
   wrote the text, so every refusal here carries one. */
typedef struct {
    UiTextResult result;
    /* Byte offset of the offending codepoint or sequence. Equals the input size on UI_TEXT_OK, and for
       UI_TEXT_CLUSTER_TOO_LONG points at the cluster's first byte. */
    size_t offset;
    /* The offending codepoint, or 0 when the failure is not attributable to one (cluster too long). */
    uint32_t codepoint;
} UiTextDiagnostic;

/* Stable short name for a result, for tool output and test failure messages. */
const char *ui_text_result_name(UiTextResult result);

/**
 * Check one field's bytes (no terminator included in `size`, though a trailing NUL is refused as a
 * control character if it is inside `size`).
 *
 * `out_diagnostic` may be NULL. Nothing is written to it on UI_TEXT_OK except the zeroed result and
 * `offset == size`, so a caller can test the result and ignore the details.
 */
UiTextResult ui_text_check(const char *bytes, size_t size, UiTextDiagnostic *out_diagnostic);

/* Convenience for the common case of a NUL-terminated buffer. NULL is an invalid argument in the same
   way an unusable buffer is: a caller with no text has no field. */
UiTextResult ui_text_check_cstr(const char *text, UiTextDiagnostic *out_diagnostic);

/* One-line human description of a diagnostic, without a trailing newline: for load diagnostics that have
   to be printed. Writes a truncated but always terminated string. */
void ui_text_describe(const UiTextDiagnostic *diagnostic, char *buffer, size_t capacity);

#endif /* UI_TEXT_H */
