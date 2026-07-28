#ifndef FRAME_DISPATCH_H
#define FRAME_DISPATCH_H

#include "camera.h"
#include "grid.h"
#include <stdint.h>

void frame_dispatch_apply_scenario(Grid *grid, Camera *camera,
                                   const char *scenario, uint64_t frame_count);

#endif