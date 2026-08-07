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

typedef enum {
    INPUT_EVENT_QUIT,
    INPUT_EVENT_KEY_DOWN,
    INPUT_EVENT_TEXT,
    INPUT_EVENT_MOUSE_MOTION,
    INPUT_EVENT_MOUSE_BUTTON_DOWN,
    INPUT_EVENT_MOUSE_BUTTON_UP,
    INPUT_EVENT_MOUSE_WHEEL
} InputEventType;

typedef enum {
    INPUT_KEY_NONE, INPUT_KEY_ESCAPE, INPUT_KEY_UP, INPUT_KEY_DOWN,
    INPUT_KEY_RETURN, INPUT_KEY_LEFT, INPUT_KEY_RIGHT, INPUT_KEY_SPACE,
    INPUT_KEY_BACKSPACE, INPUT_KEY_F5, INPUT_KEY_F9, INPUT_KEY_LEFTBRACKET,
    INPUT_KEY_RIGHTBRACKET, INPUT_KEY_F10, INPUT_KEY_TAB, INPUT_KEY_E,
    INPUT_KEY_Z, INPUT_KEY_Y, INPUT_KEY_S, INPUT_KEY_O, INPUT_KEY_N, INPUT_KEY_I,
    INPUT_KEY_EQUALS, INPUT_KEY_MINUS, INPUT_KEY_ZERO
} InputKey;

typedef struct {
    InputEventType type;
    InputKey key;
    bool repeat;
    bool ctrl;
    bool shift;
    float x;
    float y;
    int button;
    const char *text;
} InputEvent;

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
    bool mouse_left;
    bool mouse_right;
    float mouse_wheel_x;
    float mouse_wheel_y;
    bool up;                /* Up arrow key — edge-triggered: true for one frame on press */
    bool down;              /* Down arrow key — edge-triggered */
    bool confirm;           /* Enter key — edge-triggered */
    bool esc;               /* ESC key — edge-triggered: true for one frame on press (non-repeat) */
    bool ui_scale_increase_pressed; /* Ctrl+= — global UI scale increase */
    bool ui_scale_decrease_pressed; /* Ctrl+- — global UI scale decrease */
    bool ui_scale_reset_pressed;    /* Ctrl+0 — reset immutable UI default */

    /* ---- Asset Designer input (ignored outside designer state) ---- */
    bool arrow_left;    /* Left arrow key — edge-triggered, non-repeat */
    bool arrow_right;   /* Right arrow key — edge-triggered, non-repeat */
    bool place;         /* Space bar — place glyph at cursor, edge-triggered */
    bool erase;         /* Backspace/Delete — erase glyph at cursor, edge-triggered */
    bool save;          /* F5 — autosave current decal, edge-triggered */
    bool load;          /* F9 — autoload saved decal, edge-triggered */
    bool prev_glyph;    /* [ — cycle glyph palette backward, edge-triggered */
    bool next_glyph;    /* ] — cycle glyph palette forward, edge-triggered */
    bool save_as;       /* F10 — always enter Save-As prompt, edge-triggered */
    bool tab;           /* Tab — toggle canvas/metadata focus, edge-triggered */
    bool ctrl_left;     /* Ctrl+Left — directional focus move, edge-triggered */
    bool ctrl_right;    /* Ctrl+Right — directional focus move, edge-triggered */
    bool ctrl_up;       /* Ctrl+Up — directional focus move, edge-triggered */
    bool ctrl_down;     /* Ctrl+Down — directional focus move, edge-triggered */

    /* ---- Unified editor input (ignored outside APP_STATE_EDITOR) ---- */
    bool editor_toggle_mode_pressed; /* Tab — walk/edit toggle */
    bool editor_select_pressed;      /* E — select hovered wall */
    bool editor_confirm_pressed;     /* Enter — confirm inspector / modal */
    bool editor_cancel_pressed;      /* Escape — cancel / close hierarchy */
    bool editor_undo_pressed;        /* Ctrl+Z */
    bool editor_redo_pressed;        /* Ctrl+Y */
    bool editor_save_pressed;        /* Ctrl+S */
    bool editor_save_as_pressed;     /* Ctrl+Shift+S */
    bool editor_open_pressed;        /* Ctrl+O — native scene chooser */
    bool editor_import_pressed;      /* Ctrl+I — legacy map chooser */
    bool editor_new_pressed;         /* Ctrl+N */
    bool editor_reload_pressed;      /* F5 — reload scene */
    bool editor_text_backspace_pressed; /* Backspace — edit active text field */
    bool editor_previous_pressed;    /* Up — picker previous */
    bool editor_next_pressed;        /* Down — picker next */
    bool editor_decrease_pressed;    /* Left — decrease inspector field */
    bool editor_increase_pressed;    /* Right — increase inspector field */

  /* Held state — physically depressed this frame.

   * Filled from SDL_GetKeyboardState() snapshot.
   * Used by asset_designer for auto-repeat and paint-while-moving.
   * All other consumers should ignore these fields. */
  bool held_up;           /* Up arrow physically held */
  bool held_down;         /* Down arrow physically held */
  bool held_arrow_left;   /* Left arrow physically held */
  bool held_arrow_right;  /* Right arrow physically held */
  bool held_place;        /* Space bar physically held */
  bool held_erase;        /* Backspace physically held */

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
void input_begin_frame(InputState *input);
void input_apply_event(InputState *input, const InputEvent *event, bool headless_mode);
void input_apply_movement_state(InputState *input, bool forward, bool backward,
                                bool left, bool right, bool ctrl_held);

#endif /* INPUT_H */