#ifndef MAP_H
#define MAP_H
#include <stdbool.h>

typedef struct {
    int width;
    int height;
    int *data;
} Map;

Map* map_create(int width, int height);
void map_destroy(Map *map);
bool map_in_bounds(Map *map, int x, int y);
int map_get(Map *map, int x, int y);
void map_set(Map *map, int x, int y, int value);

#endif
