#define _GNU_SOURCE

#include "platform_number.h"
#include "platform_number_internal.h"

#include <errno.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PLATFORM_NUMBER_FORMAT_CAPACITY = 32 };

static bool valid_float_grammar(const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    bool before = false;
    bool after = false;
    if (*cursor == '+' || *cursor == '-') cursor++;
    while (*cursor >= '0' && *cursor <= '9') {
        before = true;
        cursor++;
    }
    if (*cursor == '.') {
        cursor++;
        while (*cursor >= '0' && *cursor <= '9') {
            after = true;
            cursor++;
        }
    }
    if (!before && !after) return false;
    if (*cursor == 'e' || *cursor == 'E') {
        bool exponent = false;
        cursor++;
        if (*cursor == '+' || *cursor == '-') cursor++;
        while (*cursor >= '0' && *cursor <= '9') {
            exponent = true;
            cursor++;
        }
        if (!exponent) return false;
    }
    return *cursor == '\0';
}

PlatformNumberResult platform_number_internal_parse_double(
    const char *text, double *out_value, PlatformNumberFault fault
) {
    char *end = NULL;
    double parsed;
    int conversion_error;
#ifdef _WIN32
    _locale_t c_locale;
#else
    locale_t c_locale;
#endif
    if (!text || !out_value) return PLATFORM_NUMBER_INVALID_ARGUMENT;
    if (!valid_float_grammar(text)) return PLATFORM_NUMBER_INVALID_TEXT;
    if (fault == PLATFORM_NUMBER_FAULT_LOCALE)
        return PLATFORM_NUMBER_LOCALE_FAILED;
#ifdef _WIN32
    c_locale = _create_locale(LC_NUMERIC, "C");
    if (!c_locale) return PLATFORM_NUMBER_LOCALE_FAILED;
    errno = 0;
    parsed = fault == PLATFORM_NUMBER_FAULT_PARSE
        ? 0.0 : _strtod_l(text, &end, c_locale);
    conversion_error = errno;
    _free_locale(c_locale);
#else
    c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    if (c_locale == (locale_t)0) return PLATFORM_NUMBER_LOCALE_FAILED;
    errno = 0;
    parsed = fault == PLATFORM_NUMBER_FAULT_PARSE
        ? 0.0 : strtod_l(text, &end, c_locale);
    conversion_error = errno;
    freelocale(c_locale);
#endif
    if (fault == PLATFORM_NUMBER_FAULT_PARSE)
        return PLATFORM_NUMBER_INVALID_TEXT;
    if (conversion_error == ERANGE || !end || *end != '\0' || !isfinite(parsed) ||
        parsed > DBL_MAX || parsed < -DBL_MAX)
        return conversion_error == ERANGE ? PLATFORM_NUMBER_OUT_OF_RANGE
                                          : PLATFORM_NUMBER_INVALID_TEXT;
    *out_value = parsed;
    return PLATFORM_NUMBER_OK;
}

PlatformNumberResult platform_number_parse_double(
    const char *text, double *out_value
) {
    return platform_number_internal_parse_double(
        text, out_value, PLATFORM_NUMBER_FAULT_NONE);
}

PlatformNumberResult platform_number_internal_format_double(
    double value, char *out_text, size_t out_size, size_t *out_length,
    PlatformNumberFault fault
) {
    char temporary[PLATFORM_NUMBER_FORMAT_CAPACITY];
    int written;
#ifdef _WIN32
    _locale_t c_locale;
#else
    locale_t c_locale;
    locale_t previous;
#endif
    if (!out_text || !out_length || out_size == 0U)
        return PLATFORM_NUMBER_INVALID_ARGUMENT;
    if (!isfinite(value)) return PLATFORM_NUMBER_OUT_OF_RANGE;
    if (fault == PLATFORM_NUMBER_FAULT_LOCALE)
        return PLATFORM_NUMBER_LOCALE_FAILED;
    if (value == 0.0) value = 0.0;
#ifdef _WIN32
    c_locale = _create_locale(LC_NUMERIC, "C");
    if (!c_locale) return PLATFORM_NUMBER_LOCALE_FAILED;
    written = fault == PLATFORM_NUMBER_FAULT_FORMAT
        ? -1 : _snprintf_l(temporary, sizeof(temporary), "%.17g", c_locale, value);
    _free_locale(c_locale);
#else
    c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    if (c_locale == (locale_t)0) return PLATFORM_NUMBER_LOCALE_FAILED;
    previous = uselocale(c_locale);
    if (previous == (locale_t)0) {
        freelocale(c_locale);
        return PLATFORM_NUMBER_LOCALE_FAILED;
    }
    written = fault == PLATFORM_NUMBER_FAULT_FORMAT
        ? -1 : snprintf(temporary, sizeof(temporary), "%.17g", value);
    if (uselocale(previous) == (locale_t)0) {
        freelocale(c_locale);
        return PLATFORM_NUMBER_LOCALE_FAILED;
    }
    freelocale(c_locale);
#endif
    if (written < 0 || (size_t)written >= sizeof(temporary))
        return PLATFORM_NUMBER_FORMAT_FAILED;
    if ((size_t)written >= out_size) return PLATFORM_NUMBER_BUFFER_TOO_SMALL;
    memcpy(out_text, temporary, (size_t)written + 1U);
    *out_length = (size_t)written;
    return PLATFORM_NUMBER_OK;
}

PlatformNumberResult platform_number_format_double(
    double value, char *out_text, size_t out_size, size_t *out_length
) {
    return platform_number_internal_format_double(
        value, out_text, out_size, out_length, PLATFORM_NUMBER_FAULT_NONE);
}
