/** platform_clipboard.c — see platform_clipboard.h for why nothing is ever truncated. */

#include "platform_clipboard.h"
#include "platform_clipboard_internal.h"

#include <SDL3/SDL.h>

#include <string.h>

const char *platform_clipboard_result_name(PlatformClipboardResult result) {
    switch (result) {
    case PLATFORM_CLIPBOARD_OK: return "OK";
    case PLATFORM_CLIPBOARD_EMPTY: return "EMPTY";
    case PLATFORM_CLIPBOARD_TOO_LARGE: return "TOO_LARGE";
    case PLATFORM_CLIPBOARD_UNAVAILABLE: return "UNAVAILABLE";
    case PLATFORM_CLIPBOARD_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    }
    return "UNKNOWN";
}

PlatformClipboardResult platform_clipboard_internal_read_text(
    const PlatformClipboardSource *source, char *buffer, size_t capacity, size_t *out_size) {
    const char *text;
    size_t length;

    if (!buffer || capacity == 0U || !out_size) return PLATFORM_CLIPBOARD_INVALID_ARGUMENT;
    buffer[0] = '\0';
    *out_size = 0U;
    if (!source || !source->read) return PLATFORM_CLIPBOARD_INVALID_ARGUMENT;
    if (source->failing) return PLATFORM_CLIPBOARD_UNAVAILABLE;

    text = source->read(source->context);
    if (!text || text[0] == '\0') return PLATFORM_CLIPBOARD_EMPTY;

    length = strlen(text);
    /* Refused, not shortened: a caller who receives the first 255 bytes of a 400-byte paste has no way to
       know what it lost (D-E rule 3). */
    if (length >= capacity) return PLATFORM_CLIPBOARD_TOO_LARGE;
    memcpy(buffer, text, length + 1U);
    *out_size = length;
    return PLATFORM_CLIPBOARD_OK;
}

/* The one function that talks to SDL, written out rather than routed through the seam: SDL owns the string
   it returns, so it is copied and freed here and nothing outlives the call. */
PlatformClipboardResult platform_clipboard_read_text(char *buffer, size_t capacity,
                                                     size_t *out_size) {
    char *text;
    size_t length;
    PlatformClipboardResult result;

    if (!buffer || capacity == 0U || !out_size) return PLATFORM_CLIPBOARD_INVALID_ARGUMENT;
    buffer[0] = '\0';
    *out_size = 0U;
    if (!SDL_WasInit(SDL_INIT_VIDEO)) return PLATFORM_CLIPBOARD_UNAVAILABLE;
    if (!SDL_HasClipboardText()) return PLATFORM_CLIPBOARD_EMPTY;
    text = SDL_GetClipboardText();
    if (!text) return PLATFORM_CLIPBOARD_UNAVAILABLE;

    length = strlen(text);
    if (length >= capacity) {
        result = PLATFORM_CLIPBOARD_TOO_LARGE;
    } else {
        memcpy(buffer, text, length + 1U);
        *out_size = length;
        result = PLATFORM_CLIPBOARD_OK;
    }
    SDL_free(text);
    return result;
}
