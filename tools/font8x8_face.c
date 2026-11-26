/**
 * tools/font8x8_face.c — defines the built-in 8×8 ASCII face for fontconv
 *
 * The face is a definition inside src/font8x8.h, so whichever translation unit includes that
 * header owns the array. In the game that unit is src/font8x8_face.c; the converter links no
 * renderer, so it needs one of its own to satisfy glyph_source.c's extern declaration. One line, and
 * its only job is to be the place the array lives.
 */

#include "../src/font8x8.h"
