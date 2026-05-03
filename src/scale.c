#include "scale.h"

ScaleResult scale_calculate(int win_w, int win_h, int grid_w, int grid_h, int cell_w, int cell_h) {
    ScaleResult res = {0};
    
    if (win_w <= 0 || win_h <= 0 || grid_w <= 0 || grid_h <= 0 || cell_w <= 0 || cell_h <= 0) {
        res.valid = false;
        return res;
    }
    
    res.logical_w = grid_w * cell_w;
    res.logical_h = grid_h * cell_h;
    
    // integer scaling factors
    int scale_x = win_w / res.logical_w;
    int scale_y = win_h / res.logical_h;
    
    res.scale_factor = scale_x < scale_y ? scale_x : scale_y;
    if (res.scale_factor < 1) {
        res.scale_factor = 1; // Minimum scale is 1 even if clipped
    }
    
    int fitted_w = res.logical_w * res.scale_factor;
    int fitted_h = res.logical_h * res.scale_factor;
    
    res.offset_x = (win_w - fitted_w) / 2;
    res.offset_y = (win_h - fitted_h) / 2;
    
    res.valid = true;
    return res;
}
