/**
 * input.h — Input state struct for keyboard and mouse
 *
 * Defines the InputState struct that captures the current frame's
 * input state: quit flag, WASD movement flags, and accumulated
 * relative mouse motion deltas.
 *
 * See input.c for the implementation.
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>     /* bool */

/**
 * InputState — Per-frame input snapshot
 *
 * Populated by input_process() once per frame.  Mouse deltas are
 * accumulated from all SDL_EVENT_MOUSE_MOTION events that occurred
 * since the last frame, then reset to zero at the start of the
 * next frame.
 */
typedef struct {
    bool quit;              /* Set true on ESC key or window close */
    bool forward;           /* W key held */
    bool backward;          /* S key held */
    bool left;              /* A key held */
    bool right;             /* D key held */
    float mouse_dx;         /* Accumulated relative mouse X movement (pixels) */
    float mouse_dy;         /* Accumulated relative mouse Y movement (pixels) */
    bool up;                /* Up arrow key — edge-triggered: true for one frame on press */
    bool down;              /* Down arrow key — edge-triggered */
    bool confirm;           /* Enter key — edge-triggered */
    bool esc;               /* ESC key — edge-triggered: true for one frame on press (non-repeat) */

    /* ---- Asset Designer input (ignored outside designer state) ---- */
    bool arrow_left;    /* Left arrow key — edge-triggered, non-repeat */
    bool arrow_right;   /* Right arrow key — edge-triggered, non-repeat */
    bool place;         /* Space bar — place glyph at cursor, edge-triggered */
    bool erase;         /* Backspace/Delete — erase glyph at cursor, edge-triggered */
    bool save;          /* F5 — autosave current decal, edge-triggered */
    bool load;          /* F9 — autoload saved decal, edge-triggered */
    bool prev_glyph;    /* Q   — cycle glyph palette backward, edge-triggered */
    bool next_glyph;    /* E   — cycle glyph palette forward, edge-triggered */
    bool save_as;       /* F10 — always enter Save-As prompt, edge-triggered */
    bool tab;           /* Tab — toggle canvas/metadata focus, edge-triggered */

    /* Text typed this frame (from SDL_EVENT_TEXT_INPUT).
     * Cleared at the start of every frame.  nul-terminated.
     * Only populated in interactive mode; empty string in headless mode. */
    char text_input[64];
    int  text_input_len;
} InputState;

/**
 * input_process() — Poll SDL events and update InputState
 *
 * Must be called once per frame.  Handles quit events, ESC key,
 * mouse motion, and keyboard state (WASD).
 *
 * In headless_mode, only SDL_EVENT_QUIT is processed; keyboard
 * and mouse input are skipped (used for benchmarking).
 *
 * @param input         InputState to update (must not be NULL)
 * @param headless_mode If true, skip interactive input processing
 */
void input_process(InputState *input, bool headless_mode);

#endif /* INPUT_H */