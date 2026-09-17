# Application UI Workbench I1 Implementation Record

**Date:** 2026-09-17  
**Outcome:** Implemented and automatically verified; native interactive visual acceptance is
still a manual follow-up.

## Scope delivered

- Added dedicated `make ui-workbench` and `--ui-workbench` launch paths.
- The application branches before world, map, game-flow, or unified-editor initialization and
  owns only renderer/grid resources plus application UI assets.
- The bounded contexts are main menu, pause, settings, and quit confirmation.
- Up/Down browses the active element set, including its ancestor container; Enter toggles edit
  selection; arrows move one cell; Tab cycles property; `[`/`]` cycles compatible values;
  Ctrl+Left/Right cycles contexts; Ctrl+Enter invokes safe application-menu navigation; Escape
  exits.
- Scale uses the same `default_user.ini` / `user.ini` precedence and persistence as normal run.
  Ctrl+`-`/Ctrl+`+` moves through 100%, 125%, 150%, and 200%; Ctrl+`0` resets to the immutable
  default. Only the application UI preview inherits the scale; workbench instructions and
  status remain fixed and visible.
- Editable values are x/y, a root or valid active-layout container parent, class-compatible
  style, transition metadata, and focus/effect metadata.

## Data and rendering contract

Legacy element files default to `style=plain`, `transition=none`, and `focus_effect=none`.
Unknown or class-incompatible presets reject the element.

Implemented styles:

- button: `plain`, `bracket`, `inverse`;
- container: `plain`, `frame`;
- text: `plain`, `bright` (resolved from the accepted `text.primary` token).

Recognized transitions are `none`, `center_out`, `perimeter_burst`, and `local_glitch`.
Recognized effects are `none`, `focus_pulse`, `focus_glitch`, and `input_hold_short`.
Transitions/effects are persisted and previewed only in the workbench. They do not expand the
approved production-motion contexts. `input_hold_short` is explicitly metadata-only.

Ancestor containers omitted from legacy layout element lists are rendered once in a
non-recursive decoration prepass, making a saved `frame` visible without duplicating child
rendering. Existing child positions and focus/action ownership remain unchanged.

## Persistence and failure behavior

Every accepted edit validates the complete element, writes canonical text to a same-directory
temporary file, applies destination metadata, flushes and syncs the file, and atomically
replaces the existing destination through `platform_fs_replace`. The active layout then reloads
and restores selection by element name. Invalid values and pre-commit failures preserve the
destination; controller mutations are restored in memory on pre-commit failure. Self-parenting
and active-layout cycles are rejected.

Application UI uses `parent=` as the authoritative relation: every active layout lists child
elements directly and checked-in application containers have no `children=` lists. Therefore a
parent edit correctly changes one element file rather than rewriting unrelated container files.

## Explicit exclusions

- authored `assets/menus/*.tui` documents and `UiDocument` workspace;
- click selection or formal pointer-edit ownership;
- arbitrary create/delete, undo/redo, drafts, scripting, or audio;
- writable custom themes or preference-format migration;
- production execution of transition/effect metadata.

## Verification evidence

Passed after the final hierarchy/render corrections:

- strict default SMC-stream application build;
- strict `USE_NO_STATE_TRACKER=1` application build;
- focused UI element suite: 18/18;
- workbench controller suite: 3/3;
- shared UI-preferences suite: 6/6, covering default/user precedence, 100/125/150/200%
  transitions and endpoints, reset-to-default, atomic persistence, and save failures;
- workbench parser/store suite: 4/4;
- input suite: 15/15;
- application-options suite: 11/11;
- `make test-ui-standards` with all seventeen owners;
- complete `make test` functional aggregate;
- cppcheck style analysis, test inventory, and project-structure checks;
- complete ASan aggregate;
- complete UBSan aggregate;
- rebuilt dummy-video-driver `--ui-workbench` smoke remained alive for the three-second bound
  with no stdout/stderr diagnostic.

One aggregate attempt failed because it was run concurrently with a separate `make clean`,
which removed a UI-preferences test temporary file under `build/`. The same UI aggregate and
complete suite passed serially with no source change for that test.

## Rollback boundary

The mode is isolated behind one run-mode branch. Removing `ui_workbench*`, its Make targets and
tests, the three optional element metadata fields, and the local preset render cases restores
the prior application behavior without touching authored Menu documents or saved game data.