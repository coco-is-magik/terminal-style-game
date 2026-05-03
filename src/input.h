#ifndef INPUT_H
#define INPUT_H
#include <stdbool.h>

typedef struct {
    bool quit;
    bool forward;
    bool backward;
    bool left;
    bool right;
    float mouse_dx;
    float mouse_dy;
} InputState;

void input_process(InputState *input, bool headless_mode);

#endif
