/**
 * decal.h — Surface decoration (decal) definition
 *
 * Defines the Decal struct used for wall posters, floor markings,
 * ceiling textures, and other surface decorations that are rendered
 * on top of the base wall/floor/ceiling geometry.
 *
 * A Decal can be placed on three surface types (DecalSurface enum)
 * and has its own pattern grid (glyphs + materials), spatial properties
 * (position, size, rotation, depth), and UV coordinates for precise
 * placement on walls.
 */

#ifndef DECAL_H
#define DECAL_H

#include <SDL3/SDL.h>    /* (included for future use) */
#include <stdbool.h>      /* bool */

#include "assets.h"       /* PatternCell — used in the pattern grid */

/**
 * DecalSurface — The type of surface a decal is attached to
 *
 * DECAL_SURFACE_WALL    — Vertical surface (wall face)
 * DECAL_SURFACE_FLOOR   — Horizontal surface at z = 0 (ground)
 * DECAL_SURFACE_CEILING — Horizontal surface at z = 1 (top)
 */
typedef enum {
    DECAL_SURFACE_WALL,      /* Wall/vertical surface */
    DECAL_SURFACE_FLOOR,     /* Floor/ground surface */
    DECAL_SURFACE_CEILING    /* Ceiling/overhead surface */
} DecalSurface;

/**
 * Decal — A surface decoration
 *
 * For wall decals, position is specified using map_x/map_y/side
 * (which wall tile) and u/v (where on that wall face).
 * For floor/ceiling decals, position uses world-space x, y, z.
 *
 * The pattern grid (pattern_rows × pattern_cols) stores the
 * glyphs and material IDs that make up the decal's appearance.
 */
typedef struct {
    DecalSurface surface;       /* Which surface type this decal attaches to */

    /* --- Position (world space for floor/ceiling, map space for walls) --- */
    double x, y, z;             /* World-space position (for floor/ceiling) */
    int map_x, map_y;           /* Map tile coordinates (for wall decals) */
    int side;                   /* Wall orientation: 0 = NS, 1 = EW */

    /* --- Surface-relative positioning --- */
    double u, v;                /* UV coordinates on the surface (0–1 range) */

    /* --- Size and orientation --- */
    double width;               /* Width in world units (for floor/ceiling) or UV units */
    double height;              /* Height in world units (for floor/ceiling) or UV units */
    double depth;               /* Depth offset from the surface (prevents z-fighting) */
    double rotation;            /* Rotation angle in radians (around surface normal) */

    /* --- Pattern grid --- */
    int pattern_cols;           /* Number of columns in the pattern */
    int pattern_rows;           /* Number of rows in the pattern */
    PatternCell *pattern;       /* Dynamically allocated pattern array */
} Decal;

#endif /* DECAL_H */