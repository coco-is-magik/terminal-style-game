/** platform_number_internal.h — Explicit per-call numeric conversion fault seam. */
#ifndef PLATFORM_NUMBER_INTERNAL_H
#define PLATFORM_NUMBER_INTERNAL_H

#include "platform_number.h"

typedef enum {
    PLATFORM_NUMBER_FAULT_NONE = 0,
    PLATFORM_NUMBER_FAULT_LOCALE,
    PLATFORM_NUMBER_FAULT_PARSE,
    PLATFORM_NUMBER_FAULT_FORMAT
} PlatformNumberFault;

PlatformNumberResult platform_number_internal_parse_double(
    const char *text, double *out_value, PlatformNumberFault fault
);

PlatformNumberResult platform_number_internal_format_double(
    double value, char *out_text, size_t out_size, size_t *out_length,
    PlatformNumberFault fault
);

#endif /* PLATFORM_NUMBER_INTERNAL_H */
