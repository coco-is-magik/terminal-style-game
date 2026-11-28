/** platform_clipboard.h — The system clipboard, read once per paste. */
#ifndef PLATFORM_CLIPBOARD_H
#define PLATFORM_CLIPBOARD_H

#include <stdbool.h>
#include <stddef.h>

/**
 * PlatformClipboardResult — what the clipboard held, never what a caller wishes it held.
 *
 * The distinction that matters is between *nothing to paste* and *too much to paste*. An empty clipboard
 * is a status — the editor does nothing and says nothing (V1-6 plan, D-E rule 1). A clipboard larger than
 * the caller's buffer is a refusal, because truncating a paste is silent data loss the user cannot see
 * (D-E rule 3). This module therefore **never truncates**: it reports and leaves the decision to the field.
 */
typedef enum {
    PLATFORM_CLIPBOARD_OK = 0,
    /* The clipboard holds no text, or holds an unsupported type. Not an error. */
    PLATFORM_CLIPBOARD_EMPTY,
    /* The clipboard holds more text than the caller's buffer can accept. Nothing was written. */
    PLATFORM_CLIPBOARD_TOO_LARGE,
    /* The platform has no clipboard to ask (no video subsystem, or the read failed). */
    PLATFORM_CLIPBOARD_UNAVAILABLE,
    PLATFORM_CLIPBOARD_INVALID_ARGUMENT
} PlatformClipboardResult;

/* Stable short name for a result, for tool output and test failure messages. */
const char *platform_clipboard_result_name(PlatformClipboardResult result);

/**
 * Copy the clipboard's text into `buffer`, or say why not.
 *
 * `buffer` receives a NUL-terminated copy and `*out_size` its length. `capacity` counts the terminator,
 * so a result of PLATFORM_CLIPBOARD_OK guarantees `*out_size < capacity`. The buffer is cleared on entry,
 * so a refusal can never leave the previous paste looking current, and **no partial text is ever written**:
 * a clipboard that is one byte too large yields an empty buffer, not a truncated one.
 */
PlatformClipboardResult platform_clipboard_read_text(char *buffer, size_t capacity,
                                                     size_t *out_size);

#endif /* PLATFORM_CLIPBOARD_H */
