#ifndef DECAL_PROJECTION_H
#define DECAL_PROJECTION_H

#include "decal.h"

typedef struct {
    double normal[3];
    double tangent[3];
    double bitangent[3];
} DecalBasis;

DecalBasis decal_projection_basis(const Decal *decal);
void decal_projection_glyph_world(const Decal *decal, const DecalBasis *basis,
                                  int column, int row, double compression,
                                  double *world_x, double *world_y, double *world_z);

#endif