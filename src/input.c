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
#include <string.h>

void input_begin_frame(InputState *input) {
    if (!input) return;
    input->mouse_dx = 0.0f;
    input->mouse_dy = 0.0f;
    input->mouse_wheel_x = 0.0f;
    input->mouse_wheel_y = 0.0f;
#define RESET_FIELD(field) input->field = false
    RESET_FIELD(up); RESET_FIELD(down); RESET_FIELD(confirm); RESET_FIELD(esc);
    RESET_FIELD(arrow_left); RESET_FIELD(arrow_right); RESET_FIELD(place);
    RESET_FIELD(erase); RESET_FIELD(save); RESET_FIELD(load); RESET_FIELD(prev_glyph);
    RESET_FIELD(next_glyph); RESET_FIELD(save_as); RESET_FIELD(tab);
    RESET_FIELD(ctrl_left); RESET_FIELD(ctrl_right); RESET_FIELD(ctrl_up);
    RESET_FIELD(ctrl_down); RESET_FIELD(editor_toggle_mode_pressed);
    RESET_FIELD(editor_select_pressed); RESET_FIELD(editor_confirm_pressed);
    RESET_FIELD(editor_cancel_pressed); RESET_FIELD(editor_undo_pressed);
    RESET_FIELD(editor_redo_pressed); RESET_FIELD(editor_save_pressed);
    RESET_FIELD(editor_save_as_pressed); RESET_FIELD(editor_open_pressed);
    RESET_FIELD(editor_import_pressed); RESET_FIELD(editor_new_pressed);
    RESET_FIELD(editor_reload_pressed);
    RESET_FIELD(editor_text_backspace_pressed);
    RESET_FIELD(editor_previous_pressed);
    RESET_FIELD(editor_next_pressed);
    RESET_FIELD(editor_decrease_pressed);
    RESET_FIELD(editor_increase_pressed);
    RESET_FIELD(ui_scale_increase_pressed);
    RESET_FIELD(ui_scale_decrease_pressed);
    RESET_FIELD(ui_scale_reset_pressed);
#undef RESET_FIELD
    input->text_input[0] = '\0';
    input->text_input_len = 0;
}

void input_apply_event(InputState *input, const InputEvent *event, bool headless_mode) {
    if (!input || !event) return;
    if (event->type == INPUT_EVENT_QUIT) {
        input->quit = true;
        return;
    }
    if (headless_mode) return;
    if (event->type == INPUT_EVENT_TEXT && event->text) {
        const char *text = event->text;
        while (*text && input->text_input_len < (int)sizeof(input->text_input) - 1) {
            input->text_input[input->text_input_len++] = *text++;
        }
        input->text_input[input->text_input_len] = '\0';
        return;
    }
    if (event->type == INPUT_EVENT_MOUSE_MOTION) {
        input->mouse_dx += event->x;
        input->mouse_dy += event->y;
        return;
    }
    if (event->type == INPUT_EVENT_MOUSE_WHEEL) {
        input->mouse_wheel_x += event->x;
        input->mouse_wheel_y += event->y;
        return;
    }
    if (event->type == INPUT_EVENT_MOUSE_BUTTON_DOWN ||
        event->type == INPUT_EVENT_MOUSE_BUTTON_UP) {
        bool pressed = event->type == INPUT_EVENT_MOUSE_BUTTON_DOWN;
        if (event->button == 1) input->mouse_left = pressed;
        if (event->button == 3) input->mouse_right = pressed;
        return;
    }
    if (event->type != INPUT_EVENT_KEY_DOWN || event->repeat) return;
    if (event->ctrl) {
        if (event->key == INPUT_KEY_EQUALS) {
            input->ui_scale_increase_pressed = true;
            return;
        }
        if (event->key == INPUT_KEY_MINUS) {
            input->ui_scale_decrease_pressed = true;
            return;
        }
        if (event->key == INPUT_KEY_ZERO) {
            input->ui_scale_reset_pressed = true;
            return;
        }
    }
    switch (event->key) {
        case INPUT_KEY_ESCAPE: input->esc = true; input->editor_cancel_pressed = true; break;
        case INPUT_KEY_UP:
            if (event->ctrl) input->ctrl_up = true;
            else { input->up = true; input->editor_previous_pressed = true; }
            break;
        case INPUT_KEY_DOWN:
            if (event->ctrl) input->ctrl_down = true;
            else { input->down = true; input->editor_next_pressed = true; }
            break;
        case INPUT_KEY_RETURN: input->confirm = true; input->editor_confirm_pressed = true; break;
        case INPUT_KEY_LEFT:
            if (event->ctrl) input->ctrl_left = true;
            else {
                input->arrow_left = true;
                input->editor_decrease_pressed = true;
            }
            break;
        case INPUT_KEY_RIGHT:
            if (event->ctrl) input->ctrl_right = true;
            else {
                input->arrow_right = true;
                input->editor_increase_pressed = true;
            }
            break;
        case INPUT_KEY_SPACE: input->place = true; break;
        case INPUT_KEY_BACKSPACE:
            input->erase = true;
            input->editor_text_backspace_pressed = true;
            break;
        case INPUT_KEY_F5: input->save = true; input->editor_reload_pressed = true; break;
        case INPUT_KEY_F9: input->load = true; break;
        case INPUT_KEY_LEFTBRACKET: input->prev_glyph = true; break;
        case INPUT_KEY_RIGHTBRACKET: input->next_glyph = true; break;
        case INPUT_KEY_F10: input->save_as = true; break;
        case INPUT_KEY_TAB: input->tab = true; input->editor_toggle_mode_pressed = true; break;
        case INPUT_KEY_E: input->editor_select_pressed = true; break;
        case INPUT_KEY_Z: if (event->ctrl) input->editor_undo_pressed = true; break;
        case INPUT_KEY_Y: if (event->ctrl) input->editor_redo_pressed = true; break;
        case INPUT_KEY_S:
            if (event->ctrl && event->shift) input->editor_save_as_pressed = true;
            else if (event->ctrl) input->editor_save_pressed = true;
            break;
        case INPUT_KEY_O: if (event->ctrl) input->editor_open_pressed = true; break;
        case INPUT_KEY_N: if (event->ctrl) input->editor_new_pressed = true; break;
        case INPUT_KEY_I: if (event->ctrl) input->editor_import_pressed = true; break;
        case INPUT_KEY_EQUALS:
        case INPUT_KEY_MINUS:
        case INPUT_KEY_ZERO:
        case INPUT_KEY_NONE: break;
    }
}

static InputKey translate_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_ESCAPE: return INPUT_KEY_ESCAPE; case SDLK_UP: return INPUT_KEY_UP;
        case SDLK_DOWN: return INPUT_KEY_DOWN; case SDLK_RETURN: return INPUT_KEY_RETURN;
        case SDLK_LEFT: return INPUT_KEY_LEFT; case SDLK_RIGHT: return INPUT_KEY_RIGHT;
        case SDLK_SPACE: return INPUT_KEY_SPACE; case SDLK_BACKSPACE: return INPUT_KEY_BACKSPACE;
        case SDLK_F5: return INPUT_KEY_F5; case SDLK_F9: return INPUT_KEY_F9;
        case SDLK_LEFTBRACKET: return INPUT_KEY_LEFTBRACKET;
        case SDLK_RIGHTBRACKET: return INPUT_KEY_RIGHTBRACKET; case SDLK_F10: return INPUT_KEY_F10;
        case SDLK_TAB: return INPUT_KEY_TAB; case SDLK_E: return INPUT_KEY_E;
        case SDLK_Z: return INPUT_KEY_Z; case SDLK_Y: return INPUT_KEY_Y;
        case SDLK_S: return INPUT_KEY_S; case SDLK_O: return INPUT_KEY_O;
        case SDLK_N: return INPUT_KEY_N; case SDLK_I: return INPUT_KEY_I;
        case SDLK_EQUALS: return INPUT_KEY_EQUALS; case SDLK_MINUS: return INPUT_KEY_MINUS;
        case SDLK_0: return INPUT_KEY_ZERO;
        default: return INPUT_KEY_NONE;
    }
}

void input_apply_movement_state(InputState *input, bool forward, bool backward,
                                bool left, bool right, bool ctrl_held) {
    if (!input) return;
    input->forward = forward && !ctrl_held;
    input->backward = backward && !ctrl_held;
    input->left = left && !ctrl_held;
    input->right = right && !ctrl_held;
}

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
    if (!input) return;
    input_begin_frame(input);
    /* ---- Poll the SDL event queue ---- */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        InputEvent event;
        memset(&event, 0, sizeof(event));
        if (e.type == SDL_EVENT_QUIT) event.type = INPUT_EVENT_QUIT;
        else if (e.type == SDL_EVENT_KEY_DOWN) {
            event.type = INPUT_EVENT_KEY_DOWN;
            event.key = translate_key(e.key.key);
            event.repeat = e.key.repeat;
            event.ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
            event.shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
        } else if (e.type == SDL_EVENT_TEXT_INPUT) {
            event.type = INPUT_EVENT_TEXT; event.text = e.text.text;
        } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
            event.type = INPUT_EVENT_MOUSE_MOTION; event.x = e.motion.xrel; event.y = e.motion.yrel;
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            event.type = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP;
            event.button = e.button.button;
        } else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
            event.type = INPUT_EVENT_MOUSE_WHEEL; event.x = e.wheel.x; event.y = e.wheel.y;
        } else continue;
        input_apply_event(input, &event, headless_mode);
    }

    /* ---- Poll keyboard state (interactive mode only) ---- */
    if (!headless_mode) {
        /* SDL_GetKeyboardState returns a pointer to SDL's internal array
         * of 512+ booleans, one per scancode.  Passing NULL means we don't
         * need the numkeys output.  This is a snapshot — it reflects which
         * keys are *currently* held down, NOT key press/release events. */
        const bool *state = SDL_GetKeyboardState(NULL);
        const bool ctrl_held = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;

        /* Ctrl-modified letters are shortcuts, not simultaneous movement. */
        input_apply_movement_state(input,
                                   state[SDL_SCANCODE_W],
                                   state[SDL_SCANCODE_S],
                                   state[SDL_SCANCODE_A],
                                   state[SDL_SCANCODE_D],
                                   ctrl_held);
        if (input->editor_save_pressed || input->editor_save_as_pressed) {
            input->backward = false;
        }

        /* Held state for designer auto-repeat and paint-while-moving */
        input->held_up          = state[SDL_SCANCODE_UP];
        input->held_down        = state[SDL_SCANCODE_DOWN];
        input->held_arrow_left  = state[SDL_SCANCODE_LEFT];
        input->held_arrow_right = state[SDL_SCANCODE_RIGHT];
        input->held_place       = state[SDL_SCANCODE_SPACE];
        input->held_erase       = state[SDL_SCANCODE_BACKSPACE];
    }
    /* In the main benchmark path, movement flags remain false because the
     * InputState is zero-initialised before the loop.  This function does not
     * modify those flags in headless mode. */
}
