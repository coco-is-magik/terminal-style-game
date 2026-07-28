#include "checked_size.h"

#include <stdint.h>

bool checked_size_2d(int width, int height, size_t *count) {
    if (!count || width <= 0 || height <= 0) return false;
    size_t w = (size_t)width;
    size_t h = (size_t)height;
    if (w > SIZE_MAX / h) return false;
    *count = w * h;
    return true;
}

bool checked_size_bytes(size_t count, size_t element_size, size_t *bytes) {
    if (!bytes || count == 0 || element_size == 0 || count > SIZE_MAX / element_size) {
        return false;
    }
    *bytes = count * element_size;
    return true;
}