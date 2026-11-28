/** platform_clipboard_internal.h — Seam for testing the clipboard policy without a desktop. */
#ifndef PLATFORM_CLIPBOARD_INTERNAL_H
#define PLATFORM_CLIPBOARD_INTERNAL_H

#include "platform_clipboard.h"

/* Supplies the clipboard's contents as the platform would hand them over: a heap-free view that the
   caller must not keep. Returning NULL means "no text", which is the empty case rather than a failure;
   `failing` distinguishes a platform that could not answer at all. This is the same shape as the other
   `platform_*_internal` seams: the policy is testable, the one system call is not. */
typedef struct {
    const char *(*read)(void *context);
    void *context;
    bool failing;
} PlatformClipboardSource;

PlatformClipboardResult platform_clipboard_internal_read_text(
    const PlatformClipboardSource *source, char *buffer, size_t capacity, size_t *out_size);

#endif /* PLATFORM_CLIPBOARD_INTERNAL_H */
