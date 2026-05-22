/**
 * input.c — Input state polling and event processing
 *
 * This file implements the single function that bridges SDL input events
 * (keyboard, mouse, window close) with the engine's abstract InputState
 * struct.  It is called once per frame by the main loop in app.c.
 *
 * InputState fields filled by this function:
 *   quit       — set true on SDL_EVENT_QUIT or ESC key press
 *   forward    — true while W is held
 *   backward   — true while S is held
 *   left       — true while A is held
 *   right      — true while D is held
 *   mouse_dx   — accumulated relative mouse X movement this frame (pixels)
 *   mouse_dy   — accumulated relative mouse Y movement this frame (pixels)
 *
 * Two operating modes:
 *   Interactive  (headless_mode = false) — polls SDL events and keyboard state
 *   Headless     (headless_mode = true)  — only checks for SDL_EVENT_QUIT;
 *                                          keyboard/mouse input is skipped.
 *                                          This is used in benchmark and
 *                                          stability-test modes.
 */

#include "input.h"        /* InputState struct, input_process() declaration */
#include <SDL3/SDL.h>     /* SDL_PollEvent(), SDL_GetKeyboardState(),
                             SDL_EVENT_QUIT, SDLK_ESCAPE, etc. */

/**
 * input_process() — Poll SDL events and update the InputState
 *
 * Must be called exactly once per frame, at the start of the frame loop.
 * This function does two things:
 *
 *   1. Event queue polling (SDL_PollEvent loop):
 *        - SDL_EVENT_QUIT       (window close button) → sets input->quit = true
 *        - SDLK_ESCAPE          (ESC key)             → sets input->quit = true
 *        - SDL_EVENT_MOUSE_MOTION                     → accumulates xrel/yrel
 *                                                       into mouse_dx/mouse_dy
 *
 *   2. Keyboard state snapshot (SDL_GetKeyboardState):
 *        - Reads the current state of W/A/S/D into the movement flags
 *
 * Mouse deltas are reset to zero at the start of each call, then
 * re-accumulated from all mouse motion events that occurred since the
 * last frame.  This prevents drift accumulation across frames.
 *
 * In headless mode, only SDL_EVENT_QUIT is processed — keyboard and mouse
 * updates are skipped.  In the main app this means movement stays disabled
 * because InputState starts zeroed; this function does not clear WASD flags
 * while in headless mode.
 *
 * @param input         Pointer to the InputState to update (must not be NULL)
 * @param headless_mode If true, skip keyboard/mouse input (for benchmarks)
 */
void input_process(InputState *input, bool headless_mode) {
    /* ---- Reset per-frame fields ---- */
    /* Mouse deltas are re-accumulated from scratch each frame. */
    input->mouse_dx = 0.0f;
    input->mouse_dy = 0.0f;
    /* Edge-triggered navigation fields: reset each frame so they are true
     * for exactly one frame per key press (non-repeat only). */
    input->up      = false;
    input->down    = false;
    input->confirm = false;
    input->esc     = false;

    /* ---- Poll the SDL event queue ---- */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        /* Window close button (X) or system quit request */
        if (e.type == SDL_EVENT_QUIT) {
            input->quit = true;
        }

        /* Interactive-only input processing.
         * In headless mode, we skip keyboard events and mouse motion
         * so the simulation runs without user interference. */
        if (!headless_mode) {
            /* Key press events (non-repeat for navigation keys) */
            if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                    case SDLK_ESCAPE:
                        if (!e.key.repeat) input->esc = true;
                        break;
                    /* Edge-triggered navigation: only on initial press, not repeat */
                    case SDLK_UP:
                        if (!e.key.repeat) input->up = true;
                        break;
                    case SDLK_DOWN:
                        if (!e.key.repeat) input->down = true;
                        break;
                    case SDLK_RETURN:
                        if (!e.key.repeat) input->confirm = true;
                        break;
                    default:
                        break;
                }
            }
            /* Relative mouse motion → look around.
             * xrel/yrel are the delta from the last mouse position in
             * pixels.  These are accumulated across multiple motion events
             * that may occur in a single frame (e.g. fast mouse movements). */
            else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                input->mouse_dx += e.motion.xrel;
                input->mouse_dy += e.motion.yrel;
            }
        }
    }

    /* ---- Poll keyboard state (interactive mode only) ---- */
    if (!headless_mode) {
        /* SDL_GetKeyboardState returns a pointer to SDL's internal array
         * of 512+ booleans, one per scancode.  Passing NULL means we don't
         * need the numkeys output.  This is a snapshot — it reflects which
         * keys are *currently* held down, NOT key press/release events. */
        const bool *state = SDL_GetKeyboardState(NULL);

        /* WASD movement keys */
        input->forward  = state[SDL_SCANCODE_W];
        input->backward = state[SDL_SCANCODE_S];
        input->left     = state[SDL_SCANCODE_A];
        input->right    = state[SDL_SCANCODE_D];
    }
    /* In the main benchmark path, movement flags remain false because the
     * InputState is zero-initialised before the loop.  This function does not
     * modify those flags in headless mode. */
}