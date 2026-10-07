/**
 * ui_effect.h — Registry of UI effect primitives (single source of truth).
 *
 * An "effect" is a named piece of presentation the UI can bind to an element:
 * a backdrop field, a button flourish, a context transition, and so on. Each
 * primitive declares the target type it needs, the triggers and orientations it
 * accepts, the loop/randomize values it requires, and whether it is a backdrop
 * substrate. Element loading, the workbench inspector, and the animation
 * renderer all read this one table, so adding an effect is one row here plus a
 * render function in ui_animation.c — not an edit scattered across three files.
 */

#ifndef UI_EFFECT_H
#define UI_EFFECT_H

#include "ui_ele.h"

#include <stdbool.h>

typedef enum {
    UI_EFFECT_LIVING_FIELD = 0,
    UI_EFFECT_PAUSE_GLITCH,
    UI_EFFECT_CENTER_OUT,
    UI_EFFECT_PERIMETER_BURST,
    UI_EFFECT_LOCAL_GLITCH,
    UI_EFFECT_EDGE_TRACE,
    UI_EFFECT_CHROMATIC_REGISTER,
    UI_EFFECT_COMMAND_FLASH,
    UI_EFFECT_BUTTON_REASSEMBLE,
    UI_EFFECT_PANEL_REGISTER,
    UI_EFFECT_COUNT
} UiEffectId;

/* Trigger and orientation acceptance sets (bit flags). */
#define UI_EFFECT_TRIGGER_CONTEXT_ENTER (1U << 0)
#define UI_EFFECT_TRIGGER_CONTEXT_EXIT  (1U << 1)
#define UI_EFFECT_TRIGGER_FOCUS         (1U << 2)
#define UI_EFFECT_TRIGGER_ACTIVATE      (1U << 3)
#define UI_EFFECT_TRIGGER_WHILE_VISIBLE (1U << 4)
#define UI_EFFECT_TRIGGER_ANY                                                     \
    (UI_EFFECT_TRIGGER_CONTEXT_ENTER | UI_EFFECT_TRIGGER_CONTEXT_EXIT |          \
     UI_EFFECT_TRIGGER_FOCUS | UI_EFFECT_TRIGGER_ACTIVATE |                      \
     UI_EFFECT_TRIGGER_WHILE_VISIBLE)

#define UI_EFFECT_ORIENT_HORIZONTAL (1U << 0)
#define UI_EFFECT_ORIENT_VERTICAL   (1U << 1)
#define UI_EFFECT_ORIENT_RADIAL     (1U << 2)
#define UI_EFFECT_ORIENT_ANY                                                      \
    (UI_EFFECT_ORIENT_HORIZONTAL | UI_EFFECT_ORIENT_VERTICAL |                    \
     UI_EFFECT_ORIENT_RADIAL)

/* Sentinel: this primitive does not constrain the value. */
#define UI_EFFECT_ANY_BOOL (-1)

typedef struct {
    UiEffectId id;
    const char *name;             /* stable primitive name (element `preset`)   */
    UiElementType target_type;    /* UI_ELE_ANIMATION means "any target"        */
    unsigned int trigger_mask;
    unsigned int orientation_mask;
    int loop;                     /* required value, or UI_EFFECT_ANY_BOOL       */
    int randomize;                /* required value, or UI_EFFECT_ANY_BOOL       */
    bool backdrop;                /* paints a substrate behind other content     */
} UiEffectSpec;

int ui_effect_count(void);
const UiEffectSpec *ui_effect_spec_at(int index);
const UiEffectSpec *ui_effect_spec(UiEffectId id);
const UiEffectSpec *ui_effect_spec_by_name(const char *name);
bool ui_effect_name_is_valid(const char *name);

/* A named, reusable effect definition loaded from `assets/ui_effects/<name>.txt`.
 * It is a parameterised instance of a primitive: a binding references it by name
 * (an `effect=` line in an animation element) and may override any field, so a
 * designer can reuse or retune an effect without touching C. */
typedef struct {
    char name[UI_ELE_NAME_MAX];
    char primitive[UI_ELE_PRESET_MAX];
    char trigger[UI_ELE_PRESET_MAX];
    char orientation[UI_ELE_PRESET_MAX];
    int loop;        /* -1 = not set in the file */
    int randomize;   /* -1 = not set in the file */
} UiEffectDefinition;

/* Load `assets/ui_effects/<name>.txt`. Returns false if the file is missing, the
 * primitive is unknown, or any field is malformed. */
bool ui_effect_definition_load(const char *name, UiEffectDefinition *out);

/* True when the animation binding fields on `element` satisfy `spec`. Pure; no
 * I/O. A NULL spec (unknown preset) is never valid; a non-animation element is
 * always valid. */
bool ui_effect_binding_is_valid(const UiEffectSpec *spec, const UiElement *element);

#endif /* UI_EFFECT_H */
