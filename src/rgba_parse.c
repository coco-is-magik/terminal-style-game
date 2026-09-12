#include "rgba_parse.h"

#include <errno.h>
#include <stdlib.h>

static const char *skip_horizontal_space(const char *text) {
    while (*text == ' ' || *text == '\t') text++;
    return text;
}

bool rgba_parse(const char *text, uint8_t out_channels[4]) {
    uint8_t channels[4];
    const char *cursor;

    if (!text || !out_channels) return false;
    cursor = text;
    for (size_t i = 0U; i < 4U; i++) {
        char *end = NULL;
        long value;

        cursor = skip_horizontal_space(cursor);
        errno = 0;
        value = strtol(cursor, &end, 10);
        if (errno == ERANGE || end == cursor || value < 0L || value > 255L) {
            return false;
        }
        channels[i] = (uint8_t)value;
        cursor = skip_horizontal_space(end);
        if (i < 3U) {
            if (*cursor != ',') return false;
            cursor++;
        } else if (*cursor != '\0') {
            return false;
        }
    }

    for (size_t i = 0U; i < 4U; i++) out_channels[i] = channels[i];
    return true;
}