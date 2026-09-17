# Application UI Workbench I1 Implementation Record

**Date:** 2026-09-17  
**Outcome:** Implemented and automatically verified; native interactive visual acceptance is
still a manual follow-up.

## Scope delivered

- Added dedicated `make ui-workbench` and `--ui-workbench` launch paths.
- The application branches before world, map, game-flow, or unified-editor initialization and
  owns only renderer/grid resources plus application UI assets.
- The bounded contexts are main menu, pause, settings, and quit confirmation.
- Up/Down browses the active element set, including its ancestor container; Enter toggles move
  mode; arrows move one cell while in move mode; Tab cycles property; the literal `[` and `]`
  keys cycle compatible values directly on the highlighted element without entering move mode;
  Ctrl+Left/Right cycles contexts; Ctrl+Enter invokes safe application-menu navigation; Escape
  exits.
- Scale uses the same `default_user.ini` / `user.ini` precedence and persistence as normal run.
  Ctrl+`-`/Ctrl+`+` moves through 100%, 125%, 150%, and 200%; Ctrl+`0` resets to the immutable
  default. Only the application UI preview inherits the scale; workbench instructions and
  status remain fixed and visible.
- Editable values are x/y, a root or valid active-layout container parent, class-compatible
  style, transition metadata, and focus/effect metadata.
- Ctrl+N clones an existing reusable element/animation into the active layout; Backspace
  confirms detachment from that layout. Clone updates the active layout and master preload list;
  detach preserves the reusable source file and rejects referenced parents/targets.

## Data and rendering contract

Legacy element files default to `style=plain`, `transition=none`, and `focus_effect=none`.
Unknown or class-incompatible presets reject the element.

Implemented styles:

- button: `plain`, `bracket`, `inverse`;
- container: `plain`, `frame`;
- text: `plain`, `bright` (resolved from the accepted `text.primary` token).

Recognized transitions are `none`, `center_out`, `perimeter_burst`, and `local_glitch`.
Recognized effects are `none`, `focus_pulse`, `focus_glitch`, and `input_hold_short`.
The pause glitch is an ordinary `animation_pause_glitch` unit in the pause layout. Reusable
animation units own preset, target, trigger, orientation, loop, randomize, position, and bounds.
Pause glitch, center-out, perimeter burst, and local glitch share one evaluator in normal run and
workbench. Center-out copies actual rendered target glyphs and evacuates them from target center.
`focus_pulse` and
`focus_glitch` are also consumed by normal application-menu rendering as presentation-only
decoration around the focused button. Stable `>`/`<` markers, content, bounds, focus, and
activation remain unchanged, and reduced motion removes the animated decoration immediately.
`input_hold_short` is explicitly metadata-only.

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
- free-form creation, source-asset deletion, undo/redo, drafts, scripting, timeline editing, or
  audio;
- writable custom themes or preference-format migration;
- target-specific activation identity: `activate` currently fires for an action in the containing
  layout; it does not delay the action.

## Verification evidence

Passed after the final hierarchy/render corrections:

- strict default SMC-stream application build;
- strict `USE_NO_STATE_TRACKER=1` application build;
- UI element runner: 19/19; the confirmation-layout regression now derives positions from the
  current live-authored assets while retaining centered/in-bounds requirements;
- focused production focus-effect regression: 1/1, covering pulse/glitch deterministic phases,
  stable focus markers, reduced-motion suppression, metadata-only input hold, and invalid-time
  nonmutation;
- workbench controller suite: 6/6, including value cycling without move mode, animation property
  filtering, chooser/removal modes, and isolated real-filesystem clone/add/reload/detach with
  reusable source preservation;
- shared UI-preferences suite: 6/6, covering default/user precedence, 100/125/150/200%
  transitions and endpoints, reset-to-default, atomic persistence, and save failures;
- shared animation suite: 4/4, including exact pause destination parity, actual-glyph center-out,
  trigger filtering, looping, deterministic bounded random placement, and Reduced Motion;
- workbench parser/store suite: 6/6 including animation schema round trip and layout/master
  membership add/remove/failure preservation;
- input suite: 15/15;
- application-options suite: 11/11;
- `make test-ui-standards` with all eighteen owners;
- complete `make test` functional aggregate;
- cppcheck style analysis, test inventory, and project-structure checks;
- complete ASan aggregate;
- complete UBSan aggregate;
- rebuilt dummy-video-driver normal-run and `--ui-workbench` smokes remained alive for the
  three-second bound with no stderr diagnostic.

One aggregate attempt failed because it was run concurrently with a separate `make clean`,
which removed a UI-preferences test temporary file under `build/`. The same UI aggregate and
complete suite passed serially with no source change for that test.

## Rollback boundary

The mode is isolated behind one run-mode branch. Removing `ui_workbench*`, its Make targets and
tests, the three optional element metadata fields, and the local preset render cases restores
the prior application behavior without touching authored Menu documents or saved game data.