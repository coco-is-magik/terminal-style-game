#ifndef CHECKED_SIZE_H
#define CHECKED_SIZE_H

#include <stdbool.h>
#include <stddef.h>

bool checked_size_2d(int width, int height, size_t *count);
bool checked_size_bytes(size_t count, size_t element_size, size_t *bytes);

#endif