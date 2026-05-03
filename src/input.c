#include "input.h"
#include <SDL3/SDL.h>

void input_process(InputState *input, bool headless_mode) {
    input->mouse_dx = 0.0f;
    input->mouse_dy = 0.0f;
    
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            input->quit = true;
        }
        if (!headless_mode) {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                input->quit = true;
            } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                input->mouse_dx += e.motion.xrel;
                input->mouse_dy += e.motion.yrel;
            }
        }
    }
    
    if (!headless_mode) {
        const bool *state = SDL_GetKeyboardState(NULL);
        input->forward = state[SDL_SCANCODE_W];
        input->backward = state[SDL_SCANCODE_S];
        input->left = state[SDL_SCANCODE_A];
        input->right = state[SDL_SCANCODE_D];
    }
}
