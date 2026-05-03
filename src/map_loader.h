#ifndef MAP_LOADER_H
#define MAP_LOADER_H

#include "map.h"

// Parses a simple newline-delimited text grid where '0'=0, '1'=1, etc.
Map* map_load_from_string(const char *map_txt);

#endif
