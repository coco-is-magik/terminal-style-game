#include "math.h"
#include <math.h>

double normalize_angle(double angle) {
    while (angle < 0) angle += 2 * PI;
    while (angle >= 2 * PI) angle -= 2 * PI;
    return angle;
}
