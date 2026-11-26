/**
 * font8x8_face.c — the one translation unit in the application that defines the built-in face
 *
 * The face is a *definition* inside `src/font8x8.h`, so whichever translation unit includes that
 * header owns the array; every other one sees `glyph_source.c`'s extern declaration. Exactly one
 * owner is required per binary, and the previous owner — `glyph_atlas.c` — was deleted when residency
 * replaced the cache (V1_5 plan, I5/D-E read together with V1-4's resident-set budget).
 *
 * `tests/font8x8_test_data.c` and `tools/font8x8_face.c` are the same arrangement for the test
 * binaries and the converter. This file's only job is to be the place the array lives.
 */

#include "font8x8.h"
