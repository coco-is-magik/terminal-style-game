/** platform_number.h — Locale-independent ASCII double conversion. */
#ifndef PLATFORM_NUMBER_H
#define PLATFORM_NUMBER_H

#include <stddef.h>

typedef enum {
    PLATFORM_NUMBER_OK = 0,
    PLATFORM_NUMBER_INVALID_ARGUMENT,
    PLATFORM_NUMBER_INVALID_TEXT,
    PLATFORM_NUMBER_OUT_OF_RANGE,
    PLATFORM_NUMBER_LOCALE_FAILED,
    PLATFORM_NUMBER_FORMAT_FAILED,
    PLATFORM_NUMBER_BUFFER_TOO_SMALL
} PlatformNumberResult;

PlatformNumberResult platform_number_parse_double(
    const char *text, double *out_value
);

PlatformNumberResult platform_number_format_double(
    double value, char *out_text, size_t out_size, size_t *out_length
);

#endif /* PLATFORM_NUMBER_H */
