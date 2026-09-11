# R12 UI/Menu Authoring Plan — 2026-09-04

## Status and authority

**Implementation complete; Ready for Final Manual Retest.** R11 is verified and closed. R12 I1 implements the separate
versioned `FlowDocument` graph; I2 implements typed asset/port validation and pure
runtime navigation; I3 implements the minimum authored Menu `UiDocument` tree and
Button-port export; I4 adds versioned responsive anchor/Stretch geometry and local
per-item scale resolution; I5 adds native/sprite visuals, transient state/theme input,
and headless canvas rendering; I6 adds pure pointer, focus, directional navigation,
and activation semantics; I7 adds scene v11 named exit triggers and borrowed scene-port
catalog adaptation; I8 adds pure typed Button/scene-exit activation-to-flow binding.
I9 composes rendering, interaction, and flow binding into a headless authored Menu
runtime host. I10 adds the first unified-editor game-flow workspace for staged graph browsing,
rewiring, history, Save, and Discard. I11 composes authoritative project Scene/Menu
catalogs and adds catalog-backed connect/add/remove graph workflows. I12 adds the first
visual authored-Menu workspace with project
open/create, responsive I4/I5 preview, stable hierarchy selection, bounded layout-property
history, and transactional Save/Discard. I13 adds typed Container/Text/Button construction,
staged content/Button-port editing, confirmed subtree removal, and generalized bounded
history. I14 adds staged validated rename, cycle-safe Container reparenting, and adjacent
sibling subtree moves in painter order. I15 exposes the existing v3 anchors, visual
mode/sprite ID, alignment, colors, fill/border
policy and glyphs, and default visibility through typed scrolling properties. I16 adds pointer
preview selection, live drag move,
bounded bottom-right handle resize, one-command release, and exact cancellation. I17 adds
session-only multi-resolution/UI-scale preview and runtime-like Menu testing with current
staged-reference diagnostics and report-only typed targets. All seventeen increments pass
their automated gates; final display-backed acceptance remains pending.
This document
records the broader responsive authored-UI and UI/menu-authoring direction.

This plan is intentionally scoped to **game-authored UI** and **game progression
authoring**. It does **not** replace or redesign the existing editor shell UI,
the menus that launch the editor, or the current application chrome.

## 1. Purpose

R12 defines the long-term system for authoring the UI that appears in a game built
with the editor. The intended output is a unified, data-driven UI system that can
support:

- menus;
- buttons;
- HUD/status displays;
- decorative and structural UI elements;
- screen-to-screen game progression; and
- live authoring of those elements inside the editor workflow.

The phase also needs a clean way to connect scenes, menus, and exits so that a
project can express hub-and-spoke games, linear mission chains, menu interludes,
and other authored progression structures.

## 2. Scope boundary

### In scope

- authored in-game UI, not the editor shell;
- UI layout and visual element authoring;
- game progression connections between start, menus, scenes, and exits;
- live preview of authored UI;
- per-item scale for authored UI elements;
- reusable nested-inspector / hierarchy interaction;
- runtime binding between UI and gameplay state/actions;
- validation, migration, Save/Discard, and recovery behavior.

### Out of scope

- redesigning the editor application UI;
- replacing the current menus that enter the editor;
- in-place modification of the editor chrome as a target feature;
- arbitrary hard-coded resolution branches as the responsive model;
- corruptible authoring states that cannot be restored;
- a separate general-purpose application-shell UI framework.

## 3. High-level design summary

R12 is planned as two related authoring systems with a shared runtime binding layer:

1. **Flow/progression editor**
   - defines how the game proceeds from Start Game to scenes, menus, and exits;
   - connects scene exits to destination nodes;
   - connects menu buttons to destination nodes;
   - supports branching and hub-based game structures.

2. **Visual UI editor**
   - defines what the player sees;
   - lets the user draw and place buttons, panels, text, icons, and other UI
     elements on a screen-sized canvas;
   - supports layout, scale, focus, ordering, and appearance.

3. **Binding / runtime layer**
   - connects visual elements and flow nodes to gameplay state and actions;
   - reuses existing trigger and sprite systems where appropriate;
   - preserves deterministic, data-driven behavior.

This split keeps game progression, visual layout, and behavior wiring distinct while
still letting the user author them in the same workflow.

## 4. Decisions already made

### 4.1 R12 is for authored game UI

The UI editor is not meant to replace the existing editor UI. It is meant to design
the UI used by the user’s game while working in the editor context.

### 4.2 The editor shell remains untouched

The current application UI, editor chrome, and launcher/menu surfaces are out of
scope for R12. That means R12 should not require a redesign of the existing app
menus or editor navigation.

### 4.3 Global UI scale and authored UI scale are separate concerns

The existing global UI scale remains available for readability/accessibility of the
app/editor UI. Authored UI should define scale on a per-item basis.

### 4.4 Menus, buttons, HUD, and similar gameplay UI should use one unified system

The authored UI system should not split into separate frameworks for menu UI and HUD
UI unless a later requirement proves that separation necessary.

### 4.5 The user should be able to draw/design the UI elements themselves

R12 is not just binding logic. The user must be able to author the visual elements
and layouts that compose the in-game UI.

### 4.6 Scene/menu progression should be authorable from within the editor

The game’s progression structure should be editable as connections between scenes,
menus, buttons, and exits.

## 5. Forced or implied constraints from the existing design

### 5.1 Existing UI primitives are data-driven and file-defined

The current `ui_ele` system already models small reusable UI elements with:

- container/text/button element types;
- parent/child links;
- absolute and relative positioning;
- alignment;
- visibility;
- z-order;
- colors;
- action strings;
- focus state.

This strongly implies R12 should build on a data-driven UI model instead of a purely
code-driven UI editor.

### 5.2 `menu_state` is a generic stack, not the authored UI model

The menu stack remains a generic navigation primitive. It should not be overloaded
into the authored UI schema itself.

### 5.3 The editor architecture already favors controller/data separation

The unified editor owns scene state, command history, and editor modes, while lower
layers remain generic. R12 should preserve that separation and not collapse layout,
binding, and editing concerns into one monolithic subsystem.

### 5.4 Staged editing and transactional Save/Discard are the house style

The project already uses staged documents, validation, and restoration on failure.
R12 should follow the same pattern for UI authoring.

### 5.5 Runtime and authoring state should remain separate

The verified R11 boundary kept runtime phase, renderer time, and authored data apart.
R12 should preserve that same philosophy for authored UI and any progression data.

### 5.6 Sprites and triggers are the likely reuse points

Sprites are the likely visual asset source for buttons, icons, and decorative UI.
The trigger system is the likely behavior source for UI actions and transitions.

## 6. Proposed authoring model

### 6.1 Flow graph workspace

The first interaction surface may be a graph/tree-like workspace that connects:

- Start Game;
- scenes;
- menus;
- scene exits;
- menu buttons.

The user can define where the game goes next by connecting one node to another.
Although a tree-like UI may be useful for presentation, the underlying data should be
allowed to behave like a graph because branching and loops are likely.

### 6.2 Visual UI workspace

The second interaction surface is a screen-sized canvas for designing the visible UI
the player sees.

This workspace should support:

- placing text, buttons, panels, and decorative elements;
- assigning sprite visuals where appropriate;
- arranging elements spatially;
- editing scale, order, and focus behavior;
- previewing the authored result.

### 6.3 Binding layer

The binding layer connects visual UI and flow graph behavior to gameplay state.

Likely responsibilities:

- button activation;
- navigation between menus/scenes;
- scene exit handling;
- HUD/status updates;
- focus and selection transitions;
- trigger or action dispatch.

## 7. Editor interaction model

The user’s interaction with the editor is a core R12 question.

### 7.1 Recommended interaction states

#### Browse / select

- click or keyboard-select an element;
- show selection in the canvas and hierarchy;
- route property editing to the inspector.

#### Manipulate

- move, resize, reorder, duplicate, delete, and reparent UI elements;
- support canvas manipulation and tree manipulation;
- keep operations transactional.

#### Edit properties

- modify text, colors, scale, anchors, alignment, ordering, focusability, and
  action/binding references.

#### Preview / test

- switch the workspace into runtime-like interaction mode;
- allow click and navigation testing;
- verify focus, transitions, and visual state changes;
- return to edit mode without losing staged changes.

### 7.2 Recommended workflow structure

The simplest interaction pattern is a hybrid:

- a dedicated authoring workspace for editing;
- a live preview/test mode for runtime-like behavior;
- staged changes with Save/Discard semantics.

This gives a clear separation between editing and playing while still letting the user
see the actual UI behave as intended.

## 8. Runtime ownership and document boundaries

### 8.1 UI document boundary

R12 likely needs a `UiDocument` that owns the authored UI layout and related metadata.

Open question: whether a `UiDocument` represents a single screen, a reusable UI
package, or a larger authored UI bundle.

### 8.2 Scene ownership boundary

Scenes should likely reference UI and progression data rather than absorb it
inline. That keeps scene content, UI content, and flow content separable and easier to
reuse.

### 8.3 Flow/progression ownership boundary

The connectivity model may need its own document or asset type so that game flow can
be authored independently of a specific scene’s local layout.

### 8.4 Runtime model

At runtime, the game should be able to resolve:

- which UI layout is active;
- which flow node is current;
- where a button leads;
- what a scene exit points to;
- how UI state reacts to gameplay state.

## 9. Open decisions

The progression ownership choice is locked by
`R12_I1_FLOW_DOCUMENT_DECISION_AND_PLAN_2026-09-04.md`: progression is a separate
versioned directed graph, cycles are allowed, and scenes/future UI documents expose
stable named source ports. `R12_I3_UI_DOCUMENT_MENU_SCHEMA_PLAN_2026-09-04.md` locks
one reusable screen per `UiDocument`, a validated stable-ID element tree, initial
Menu-only scope, and unique Button flow ports. The remaining questions below govern
later increments.

### 9.1 `UiDocument` schema beyond I3

Still open:

- How are HUD screens activated and bound to scene/game state?
- Which style, sprite, binding, and responsive layout fields are added after their
  semantics are locked?
- Which later element types are justified beyond Container, Text, and Button?

### 9.2 Responsive layout model

I4 locks integer parent-relative rectangles with independent Start/Center/End/Stretch
anchors, parent/viewport clipping, and document-order overlap. Row/column flow,
breakpoints, and broader constraint forms remain unapproved future extensions.

### 9.3 Scale semantics

I4 locks explicit local per-item scale percentages in `[25,400]`. Local scale changes
the element's non-Stretch size around its anchor but does not scale its anchor offset
or child coordinate system. Children independently resolve against the resulting
parent rectangle.

### 9.4 Visual vocabulary

I5 locks native Container/Text/Button visuals with authored colors, optional fill and
border glyphs, alignment, and default visibility, plus numeric sprite-backed visuals.
Transient state colors come from a caller-supplied theme and Button states retain
non-color-only markers. Custom shapes and additional element types remain unapproved.

### 9.5 Interaction details

I6 locks runtime-style Button interaction against resolved clipped geometry: pointer
overlap follows painter order; hidden/disabled/empty-clipped Buttons are ineligible;
next/previous traversal wraps; directional navigation uses deterministic center-distance
scoring; activation returns stable Button ID and flow port. Input mappings, editor
selection/manipulation, and Escape behavior remain adapter/workspace decisions.

### 9.6 Binding model

Still open:

- Are UI actions trigger-like, event-like, or both?
- Does the UI system directly reference gameplay state or route through a binding
  adapter?
- What is the canonical path for button click to game transition?

### 9.7 Flow graph details

Still open:

- How are scene exits represented?
- How are scene/menu asset names resolved against catalogs?
- Should the editor show a graph directly or a tree-shaped view over graph data?
- How are future button and trigger ports validated against their owning documents?

### 9.8 Migration and compatibility

Still open:

- Which current UI assets survive unchanged?
- Which are migrated?
- Is the current `ui_ele`/`ui_layouts` system the source format or a predecessor?
- What compatibility guarantees are required during transition?

### 9.9 Validation and failure handling

Still open:

- What makes a UI document invalid?
- What failures block save?
- What failures can degrade gracefully?
- What must be restored on failed save or failed rebuild?

## 10. Recommended implementation order

1. **Implemented:** lock the flow/progression model and document boundaries.
2. **Implemented:** add typed catalog/reference validation and pure runtime
   navigation semantics.
3. **Implemented:** define `UiDocument` ownership and its minimum Menu/button-port
   schema.
4. **Implemented:** responsive layout and per-item scale semantics.
5. **Implemented:** visual vocabulary, sprite reuse, and headless rendering.
6. **Implemented:** pure hit testing, focus/navigation, and activation semantics.
7. **Implemented:** scene-exit authoring, runtime exit requests, and Scene catalog
   adaptation.
8. **Implemented:** pure runtime binding from typed Button/scene-exit activations to
   Scene/Menu target requests.
9. **Implemented:** minimum headless authored Menu runtime host with logical input,
   transient state, rendering, and typed flow-target handoff.
10. **Implemented:** minimum unified-editor game-flow workspace with conventional
    project loading, deterministic nested navigation, edge rewiring, undo/redo, and
    transactional Save/Discard.
11. **Implemented:** authoritative asset catalogs and graph construction.
12. **Implemented:** visual Menu open/create, responsive preview, hierarchy selection,
    layout properties, and transactional Save/Discard.
13. **Implemented:** typed element construction/removal and staged content/Button-port edits.
14. **Implemented:** validated rename, cycle-safe reparenting, and adjacent subtree order.
15. **Implemented:** expose existing v3 visual/layout properties through typed scrolling rows.
16. **Implemented:** pointer canvas selection, live move/resize, one-command release, and cancel.
17. **Implemented:** integrated multi-resolution runtime-like preview/reference validation
    with typed report-only target handoff.

## 11. Risks and constraints

- Do not turn R12 into an editor-shell rewrite.
- Do not bury scene data inside inline UI art/layout state.
- Do not let authored UI rely on ad hoc, resolution-specific branches.
- Do not make save failure or rebuild failure corrupt the authored document.
- Do not lose the project’s deterministic, data-first, staged-editing style.

## 12. Current best interpretation

R12 is best understood as a pair of related authoring tools:

- a progression/connection editor for menus and scenes; and
- a visual UI editor for the actual player-facing UI.

Those two tools should share a runtime binding layer and follow the existing staged
document/transactional workflow used elsewhere in the project.

## 13. Next safe action

I1–I17 and both manual-review correction rounds are implemented. All applicable focused and broad
automated gates pass.
The next safe step is only the two-item hierarchy/Flow display-backed manual retest in
`reviews/2026-09-11-roadmap-r12-closeout.md`. Do not begin post-R12 work or mark R12 Verified
until the affected checks are completed and recorded.