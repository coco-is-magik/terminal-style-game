#include "decal_projection.h"
#include <math.h>

DecalBasis decal_projection_basis(const Decal *decal) {
    DecalBasis basis = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
    if (!decal) return basis;
    double cos_rotation = cos(decal->rotation);
    double sin_rotation = sin(decal->rotation);
    if (decal->surface == DECAL_SURFACE_WALL) {
        basis.normal[0] = cos_rotation; basis.normal[1] = sin_rotation;
        basis.tangent[0] = sin_rotation; basis.tangent[1] = -cos_rotation;
        basis.bitangent[2] = -1.0;
    } else {
        basis.normal[2] = decal->surface == DECAL_SURFACE_FLOOR ? 1.0 : -1.0;
        basis.tangent[0] = cos_rotation; basis.tangent[1] = sin_rotation;
        basis.bitangent[0] = -sin_rotation; basis.bitangent[1] = cos_rotation;
    }
    return basis;
}

void decal_projection_glyph_world(const Decal *decal, const DecalBasis *basis,
                                  int column, int row, double compression,
                                  double *world_x, double *world_y, double *world_z) {
    if (!decal || !basis || !world_x || !world_y || !world_z ||
        decal->pattern_cols <= 0 || decal->pattern_rows <= 0 || compression <= 0.0) return;
    double step_u = decal->glyph_step_u > 0.0 ? decal->glyph_step_u :
                    decal->width / decal->pattern_cols / compression;
    double step_v = decal->glyph_step_v > 0.0 ? decal->glyph_step_v :
                    decal->height / decal->pattern_rows / compression;
    double local_u = (column - (decal->pattern_cols - 1.0) * 0.5) * step_u;
    double local_v = (row - (decal->pattern_rows - 1.0) * 0.5) * step_v;
    *world_x = decal->x + local_u * basis->tangent[0] + local_v * basis->bitangent[0];
    *world_y = decal->y + local_u * basis->tangent[1] + local_v * basis->bitangent[1];
    *world_z = decal->z + local_u * basis->tangent[2] + local_v * basis->bitangent[2];
}