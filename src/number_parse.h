/** number_parse.h — Strict decimal parsing for project text formats. */
#ifndef NUMBER_PARSE_H
#define NUMBER_PARSE_H

#include <stdbool.h>

/* Output values are unchanged when parsing fails. */
bool number_parse_int(const char *text, int minimum, int maximum, int *out_value);
bool number_parse_finite_double(const char *text, double *out_value);

#endif