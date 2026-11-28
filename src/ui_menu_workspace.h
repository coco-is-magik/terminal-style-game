/** ui_menu_workspace.h — Headless staged UI Scene workspace. */
#ifndef UI_MENU_WORKSPACE_H
#define UI_MENU_WORKSPACE_H

#include "map_catalog.h"
#include "ui_document.h"
#include "ui_animation_playback.h"
#include "ui_nested_inspector.h"

#define UI_MENU_WORKSPACE_HISTORY_CAPACITY 32U

typedef enum {
    UI_MENU_WORKSPACE_CHOOSER = 0,
    UI_MENU_WORKSPACE_CREATE_NAME,
    UI_MENU_WORKSPACE_HIERARCHY,
    UI_MENU_WORKSPACE_PROPERTIES,
    UI_MENU_WORKSPACE_ACTIONS,
    UI_MENU_WORKSPACE_EDIT_CONTENT,
    UI_MENU_WORKSPACE_EDIT_PORT,
    UI_MENU_WORKSPACE_EDIT_NAME,
    UI_MENU_WORKSPACE_REPARENT,
    UI_MENU_WORKSPACE_PREVIEW_SETTINGS,
    UI_MENU_WORKSPACE_REMOVE_PROMPT,
    UI_MENU_WORKSPACE_CLOSE_PROMPT
} UiMenuWorkspaceMode;

typedef enum {
    UI_MENU_ACTION_PROPERTIES = 0,
    UI_MENU_ACTION_ADD_CONTAINER,
    UI_MENU_ACTION_ADD_TEXT,
    UI_MENU_ACTION_ADD_BUTTON,
    UI_MENU_ACTION_EDIT_CONTENT,
    UI_MENU_ACTION_EDIT_PORT,
    UI_MENU_ACTION_REMOVE,
    UI_MENU_ACTION_RENAME,
    UI_MENU_ACTION_REPARENT,
    UI_MENU_ACTION_MOVE_EARLIER,
    UI_MENU_ACTION_MOVE_LATER,
    UI_MENU_ACTION_PREVIEW_SETTINGS,
    UI_MENU_ACTION_ADD_ANIMATION,
    UI_MENU_ACTION_COUNT
} UiMenuAction;

typedef enum {
    UI_MENU_PROPERTY_X = 0,
    UI_MENU_PROPERTY_Y,
    UI_MENU_PROPERTY_WIDTH,
    UI_MENU_PROPERTY_HEIGHT,
    UI_MENU_PROPERTY_SCALE,
    UI_MENU_PROPERTY_HORIZONTAL_ANCHOR,
    UI_MENU_PROPERTY_VERTICAL_ANCHOR,
    UI_MENU_PROPERTY_VISUAL_MODE,
    UI_MENU_PROPERTY_SPRITE_ID,
    UI_MENU_PROPERTY_ALIGNMENT,
    UI_MENU_PROPERTY_FOREGROUND_RED,
    UI_MENU_PROPERTY_FOREGROUND_GREEN,
    UI_MENU_PROPERTY_FOREGROUND_BLUE,
    UI_MENU_PROPERTY_FOREGROUND_ALPHA,
    UI_MENU_PROPERTY_BACKGROUND_RED,
    UI_MENU_PROPERTY_BACKGROUND_GREEN,
    UI_MENU_PROPERTY_BACKGROUND_BLUE,
    UI_MENU_PROPERTY_BACKGROUND_ALPHA,
    UI_MENU_PROPERTY_FILL_ENABLED,
    UI_MENU_PROPERTY_FILL_GLYPH,
    UI_MENU_PROPERTY_BORDER_ENABLED,
    UI_MENU_PROPERTY_BORDER_GLYPH,
    UI_MENU_PROPERTY_VISIBLE,
    UI_MENU_PROPERTY_ENTRY_EFFECT,
    UI_MENU_PROPERTY_EXIT_EFFECT,
    UI_MENU_PROPERTY_FOCUS_EFFECT,
    UI_MENU_PROPERTY_ACTIVATE_EFFECT,
    UI_MENU_PROPERTY_ANIMATION_TARGET,
    UI_MENU_PROPERTY_ANIMATION_PRESET,
    UI_MENU_PROPERTY_ANIMATION_TRIGGER,
    UI_MENU_PROPERTY_ANIMATION_ORIENTATION,
    UI_MENU_PROPERTY_ANIMATION_LOOP,
    UI_MENU_PROPERTY_ANIMATION_RANDOMIZE,
    UI_MENU_PROPERTY_COUNT
} UiMenuProperty;

typedef enum {
    UI_MENU_PLAYBACK_STOPPED = 0,
    UI_MENU_PLAYBACK_PLAYING
} UiMenuPlaybackStatus;

typedef enum {
    UI_MENU_CLOSE_SAVE = 0,
    UI_MENU_CLOSE_DISCARD,
    UI_MENU_CLOSE_CANCEL,
    UI_MENU_CLOSE_COUNT
} UiMenuCloseChoice;

typedef enum {
    UI_MENU_POINTER_NONE = 0,
    UI_MENU_POINTER_MOVE,
    UI_MENU_POINTER_RESIZE
} UiMenuPointerMode;

typedef enum {
    UI_MENU_PREVIEW_RESOLUTION_40X15 = 0,
    UI_MENU_PREVIEW_RESOLUTION_60X20,
    UI_MENU_PREVIEW_RESOLUTION_80X25,
    UI_MENU_PREVIEW_RESOLUTION_COUNT
} UiMenuPreviewResolution;

typedef enum {
    UI_MENU_PREVIEW_SCALE_100 = 0,
    UI_MENU_PREVIEW_SCALE_125,
    UI_MENU_PREVIEW_SCALE_150,
    UI_MENU_PREVIEW_SCALE_200,
    UI_MENU_PREVIEW_SCALE_COUNT
} UiMenuPreviewScale;

typedef enum {
    UI_MENU_PREVIEW_FIELD_RESOLUTION = 0,
    UI_MENU_PREVIEW_FIELD_SCALE,
    UI_MENU_PREVIEW_FIELD_COUNT
} UiMenuPreviewField;

typedef enum {
    UI_MENU_WORKSPACE_OK = 0,
    UI_MENU_WORKSPACE_INVALID_ARGUMENT,
    UI_MENU_WORKSPACE_INACTIVE,
    UI_MENU_WORKSPACE_NO_ACTION,
    UI_MENU_WORKSPACE_CATALOG_FAILED,
    UI_MENU_WORKSPACE_LOAD_FAILED,
    UI_MENU_WORKSPACE_INVALID_NAME,
    UI_MENU_WORKSPACE_SAVE_FAILED,
    UI_MENU_WORKSPACE_MUTATION_FAILED,
    UI_MENU_WORKSPACE_HISTORY_FULL,
    UI_MENU_WORKSPACE_OK_DURABILITY_WARNING,
    /* The text is not allowed in this field at all — a control character in a label, or a character the
       identifier rule cannot use. Nothing changed; this is a refusal the editor can show rather than a
       failure. */
    UI_MENU_WORKSPACE_REJECTED_TEXT,
    /* The field is full. The keystroke was dropped rather than shortened, so nothing half-written can be
       committed (V1-6 plan, I4). */
    UI_MENU_WORKSPACE_FIELD_FULL
} UiMenuWorkspaceResult;

typedef struct {
    UiDocument *before;
    UiDocument *after;
    UiElementId before_selected_id;
    UiElementId after_selected_id;
} UiMenuWorkspaceChange;

typedef struct {
    UiDocument document;
    UiDocument saved_document;
    MapCatalog catalog;
    UiMenuWorkspaceChange changes[UI_MENU_WORKSPACE_HISTORY_CAPACITY];
    size_t change_count;
    size_t change_cursor;
    size_t chooser_index;
    size_t element_index;
    size_t action_index;
    size_t reparent_index;
    UiNestedInspectorCursor nested_cursor;
    UiMenuProperty property;
    UiMenuWorkspaceMode mode;
    UiMenuWorkspaceMode close_return_mode;
    UiMenuCloseChoice close_choice;
    UiDocument *pointer_before;
    UiDocument *candidate_document;
    UiElementId pointer_element_id;
    UiMenuPointerMode pointer_mode;
    UiMenuPreviewResolution preview_resolution;
    UiMenuPreviewScale preview_scale;
    UiMenuPreviewField preview_field;
    int pointer_start_x;
    int pointer_start_y;
    char asset_root[UI_DOCUMENT_PATH_CAPACITY];
    char create_name[UI_DOCUMENT_NAME_CAPACITY];
    size_t create_name_length;
    /* The same caret/anchor pair for the create-name field, which is not part of any document yet. */
    size_t create_name_cursor;
    size_t create_name_anchor;
    char edit_text[UI_DOCUMENT_CONTENT_CAPACITY];
    size_t edit_text_length;
    /* Where the caret sits and where the selection was anchored, both byte offsets on cluster
       boundaries. `edit_text_anchor == edit_text_cursor` means "no selection". The field is append-only
       no longer: a caret is what makes Left, Right, Backspace and typing agree about where "here" is
       (V1-6 plan, I4). */
    size_t edit_text_cursor;
    size_t edit_text_anchor;
    /* The IME's composition for the open field: drawn after the caret, never part of the text, and cleared
       by the commit that replaces it (V1-6 plan, D-F). Empty means nothing is being composed. */
    char edit_preedit[UI_DOCUMENT_CONTENT_CAPACITY];
    size_t edit_preedit_length;
    /* Why the last text operation was refused, and why the last load failed, kept so the surface can say
       so. A result code tells a caller that something was refused; only the diagnostic tells the *person*
       which byte and which rule, and a refusal nobody can locate cannot be fixed (I3/I4/I5 records). */
    UiTextDiagnostic last_text_diagnostic;
    bool last_text_refused;
    UiDocumentLoadDiagnostic last_load_diagnostic;
    bool has_document;
    bool active;
    UiMenuProperty candidate_property;
    UiAnimationPlayback playback;
    UiMenuPlaybackStatus playback_status;
    bool reduced_motion;
} UiMenuWorkspace;

void ui_menu_workspace_init(UiMenuWorkspace *workspace);
void ui_menu_workspace_clear(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_open(UiMenuWorkspace *workspace,
                                             const char *asset_root);
UiMenuWorkspaceResult ui_menu_workspace_previous(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_next(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_confirm(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_escape(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_adjust(UiMenuWorkspace *workspace,
                                               int direction);
UiMenuWorkspaceResult ui_menu_workspace_open_actions(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_request_remove(UiMenuWorkspace *workspace);
bool ui_menu_workspace_action_available(const UiMenuWorkspace *workspace,
                                        UiMenuAction action);
bool ui_menu_workspace_reparent_target_available(const UiMenuWorkspace *workspace,
                                                  size_t element_index);
bool ui_menu_workspace_property_available(const UiMenuWorkspace *workspace,
                                          UiMenuProperty property);
/* The authored element under a preview-local cell: the topmost visible element in paint
   order, never the screen root (id 1). The pointer press and the hover indicator both
   read this one test, so what a click selects is what the pointer highlights. While a
   gesture grips an element, the gripped element is returned rather than re-deciding the
   hit from wherever the pointer has been dragged to. */
UiMenuWorkspaceResult ui_menu_workspace_element_at(
    const UiMenuWorkspace *workspace, int pointer_x, int pointer_y,
    int viewport_width, int viewport_height, UiElementId *out_element_id);
UiMenuWorkspaceResult ui_menu_workspace_pointer_press(
    UiMenuWorkspace *workspace, int pointer_x, int pointer_y,
    int viewport_width, int viewport_height);
UiMenuWorkspaceResult ui_menu_workspace_pointer_motion(
    UiMenuWorkspace *workspace, int pointer_x, int pointer_y);
UiMenuWorkspaceResult ui_menu_workspace_pointer_release(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_pointer_cancel(UiMenuWorkspace *workspace);
void ui_menu_workspace_preview_dimensions(const UiMenuWorkspace *workspace,
                                          int *out_width, int *out_height);
int ui_menu_workspace_preview_scale_percent(const UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_append_text(UiMenuWorkspace *workspace,
                                                    const char *text);
UiMenuWorkspaceResult ui_menu_workspace_backspace(UiMenuWorkspace *workspace);
/* Move the caret one cluster left (direction < 0) or right (direction > 0) in a text field.
 *
 * `extend` keeps the anchor where it is, growing or shrinking the selection; without it the selection
 * collapses to the new caret position, which is what a plain arrow key does everywhere. Refused outside
 * a text mode (NO_ACTION), and at either end of the field the result is OK with nothing moved — a caret
 * at the start has nowhere to go, which is not an error. */
UiMenuWorkspaceResult ui_menu_workspace_move_caret(UiMenuWorkspace *workspace, int direction,
                                                   bool extend);
/* Select the whole field, or clear the selection with `select == false`. */
UiMenuWorkspaceResult ui_menu_workspace_select_all(UiMenuWorkspace *workspace, bool select);
/* True when the field has a non-empty selection. */
bool ui_menu_workspace_has_selection(const UiMenuWorkspace *workspace);

/**
 * Paste `text` at the caret as one operation, under D-E's four rules.
 *
 * 1. Nothing on the clipboard: the caller passes `size == 0` (or the platform reported EMPTY) and this
 *    returns NO_ACTION. Silent by design — an empty clipboard is a status, not an error.
 * 2. Malformed UTF-8 anywhere: refused whole, and `out_diagnostic` names the byte offset and the kind.
 * 3. Would exceed the field's capacity: refused whole (`FIELD_FULL`). Truncating a paste is silent data
 *    loss the user cannot see.
 * 4. A character this field cannot hold (a control character in a single-line label): refused whole.
 *
 * "Whole" is the whole point: on every refusal the field is byte-for-byte unchanged, so the paste can be
 * retried after the user edits the source. A success is one edit, which is one change when it commits —
 * that is what makes it one undo step.
 */
UiMenuWorkspaceResult ui_menu_workspace_paste_text(UiMenuWorkspace *workspace, const char *text,
                                                   size_t size, UiTextDiagnostic *out_diagnostic);
/**
 * Show the IME's composition, or clear it with empty text or NULL.
 *
 * The pre-edit is **display only**: it is never part of the field, so a user who cancels a composition
 * leaves no trace, and one who commits it gets the committed text as a single edit. Truncated at a cluster
 * boundary if it does not fit, because a composition that cannot be shown in full must still be showable
 * (D-D). Refused outside a text mode (NO_ACTION) — there is no field to compose into.
 */
UiMenuWorkspaceResult ui_menu_workspace_set_preedit(UiMenuWorkspace *workspace, const char *text,
                                                    size_t size);
/* True when something is being composed into the open field. */
bool ui_menu_workspace_has_preedit(const UiMenuWorkspace *workspace);
/* Why the last text operation was refused, or NULL when it was not. Valid after a call that returned
   REJECTED_TEXT; cleared by the next text operation that returns OK. */
const UiTextDiagnostic *ui_menu_workspace_last_text_diagnostic(const UiMenuWorkspace *workspace);
/* Where the last refused load went wrong, or NULL when the failure was not about content (a missing file,
   a structural error) — in which case there is no position to name. */
const UiDocumentLoadDiagnostic *ui_menu_workspace_last_load_diagnostic(
    const UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_save(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_undo(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_redo(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_begin_candidate(
    UiMenuWorkspace *workspace, UiMenuProperty property);
UiMenuWorkspaceResult ui_menu_workspace_adjust_candidate(
    UiMenuWorkspace *workspace, int direction);
UiMenuWorkspaceResult ui_menu_workspace_accept_candidate(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_cancel_candidate(UiMenuWorkspace *workspace);
const UiDocument *ui_menu_workspace_preview_document(const UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_playback_start(
    UiMenuWorkspace *workspace, double now_ms);
UiMenuWorkspaceResult ui_menu_workspace_playback_event(
    UiMenuWorkspace *workspace, UiAnimationEvent event,
    UiElementId target_id, double now_ms);
UiMenuWorkspaceResult ui_menu_workspace_playback_stop(UiMenuWorkspace *workspace);
bool ui_menu_workspace_is_dirty(const UiMenuWorkspace *workspace);
bool ui_menu_workspace_result_is_committed(UiMenuWorkspaceResult result);
bool ui_menu_workspace_build_hierarchy(
    const UiMenuWorkspace *workspace,
    size_t out_document_indices[UI_DOCUMENT_MAX_ELEMENTS],
    size_t *out_count
);
const UiDocumentElement *ui_menu_workspace_selected_element(
    const UiMenuWorkspace *workspace
);

#endif /* UI_MENU_WORKSPACE_H */
