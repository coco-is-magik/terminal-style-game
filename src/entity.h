/**
 * entity.h — Basic game entity definition
 *
 * Defines the Entity struct, which is the simplest game object:
 * a position (Vec2) and a facing angle in radians.  This is used
 * as the transform component of the Camera.
 *
 * The Vec2 type and PI constant are defined in math.h.
 */

#ifndef ENTITY_H
#define ENTITY_H

#include "math.h"     /* Vec2, PI */

/**
 * Entity — A positioned, angled object in the 2D world
 *
 * The pos field stores the entity's position in world/grid coordinates
 * (fractional values allowed — e.g. (1.5, 1.5) is the centre of tile (1,1)).
 *
 * The angle field stores the facing direction in radians:
 *   0     = east  (+X direction)
 *   PI/2  = north (+Y direction)
 *   PI    = west  (-X direction)
 *   3PI/2 = south (-Y direction)
 */
typedef struct {
    Vec2 pos;            /* World position (fractional grid coordinates) */
    double angle;        /* Facing direction in radians */
} Entity;

#endif /* ENTITY_H */