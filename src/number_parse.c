#include "number_parse.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

static bool only_trailing_space(const char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
    return *text == '\0';
}

bool number_parse_int(const char *text, int minimum, int maximum, int *out_value) {
    char *end = NULL;
    long value;

    if (!text || !out_value || minimum > maximum) return false;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || !only_trailing_space(end) ||
        value < minimum || value > maximum || value < INT_MIN || value > INT_MAX) {
        return false;
    }
    *out_value = (int)value;
    return true;
}

bool number_parse_finite_double(const char *text, double *out_value) {
    char *end = NULL;
    double value;

    if (!text || !out_value) return false;
    errno = 0;
    value = strtod(text, &end);
    if (errno == ERANGE || end == text || !only_trailing_space(end) || !isfinite(value)) {
        return false;
    }
    *out_value = value;
    return true;
}
