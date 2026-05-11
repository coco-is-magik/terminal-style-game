#ifndef DECAL_H
#define DECAL_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "assets.h"

typedef enum {
    DECAL_SURFACE_WALL,
    DECAL_SURFACE_FLOOR,
    DECAL_SURFACE_CEILING
} DecalSurface;

typedef struct {
    DecalSurface surface;
    
    // For wall: map_x, map_y, side
    // For floor/ceiling: world x, y center
    double x, y;
    int map_x, map_y;
    int side; // 0 for NS, 1 for EW
    
    // Position within the surface
    // For wall: u is horizontal (0-1), v is vertical (0-1)
    // For floor/ceiling: width and height in world units
    double u, v; 
    double width, height;
    double rotation;
    
    int pattern_cols;
    int pattern_rows;
    PatternCell *pattern;
} Decal;

#endif
