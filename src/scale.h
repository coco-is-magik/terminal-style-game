#ifndef SCALE_H
#define SCALE_H

#include <stdbool.h>

typedef struct {
    int logical_w;
    int logical_h;
    int scale_factor;
    int offset_x;
    int offset_y;
    bool valid;
} ScaleResult;

ScaleResult scale_calculate(int win_w, int win_h, int grid_w, int grid_h, int cell_w, int cell_h);

#endif
