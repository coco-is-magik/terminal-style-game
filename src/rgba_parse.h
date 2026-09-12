/** rgba_parse.h — Strict parsing for decimal RGBA color fields. */
#ifndef RGBA_PARSE_H
#define RGBA_PARSE_H

#include <stdbool.h>
#include <stdint.h>

/* On failure, out_channels is unchanged. */
bool rgba_parse(const char *text, uint8_t out_channels[4]);

#endif