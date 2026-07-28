/**
 * map_loader.h — Text-based map parser
 *
 * Declares the function that converts a simple newline-delimited digit
 * grid text string into a Map struct.  Digits '0'–'9' become material
 * IDs; any other character uses the default material from config.
 *
 * See map_loader.c for the implementation.
 */

#ifndef MAP_LOADER_H
#define MAP_LOADER_H

#include "map.h"       /* Map struct — the output of parsing */

#define MAP_TEXT_MAX_WIDTH 512
#define MAP_TEXT_MAX_HEIGHT 256

/**
 * map_load_from_string() — Parse a digit-grid string into a Map
 *
 * Scans the string in two passes:
 *   1. Determines width (longest row) and height (number of rows)
 *   2. Creates the Map and sets each cell's material_id based on
 *      the digit character ('0' = 0, '1' = 1, ..., '9' = 9).
 *      Non-digit characters use config_get()->default_material_id.
 *
 * @param map_txt  NUL-terminated string containing the digit grid
 * Ragged rows are padded with material 0 to the longest accepted row.
 * Width and height are limited to MAP_TEXT_MAX_WIDTH and
 * MAP_TEXT_MAX_HEIGHT respectively.
 *
 * @return         Newly allocated Map, or NULL on empty/oversized input or
 *                 allocation failure
 */
Map* map_load_from_string(const char *map_txt);

#endif /* MAP_LOADER_H */