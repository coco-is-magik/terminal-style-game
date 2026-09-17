# Unified UI Scene System — Phase 0 Baseline and Decisions — 2026-09-17

## Status

**Phase 0 implemented and focused baseline tests verified on 2026-09-17. No production format,
runtime, asset, application-route, or editor-host behavior changed.**

The authoritative migration plan is
[`../UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md`](../UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md).
This record captures the migration inventory, baseline protections, schema decisions required to
enter Phase 1, failures encountered during this increment, and the next safe action.

## Scope completed

Phase 0 performed only the work authorized by the plan:

1. inventoried legacy application UI layouts, elements, actions, styles, transitions, effects,
   and reusable Animation assets;
2. added deterministic 80x40 production-renderer baseline coverage;
3. added direct-render versus runtime-render equivalence coverage;
4. added a deterministic headless workspace trace that represents both future editor hosts;
5. froze the initial UI document v4 schema decisions and migration defaults below;
6. left production C modules, file formats, assets, runtime routing, and Make targets unchanged.

## Legacy application UI migration inventory

### Main bootstrap screen — protected

Legacy layout: `assets/ui_layouts/main_menu.txt`  
Legacy root: `main_menu_container`

| Element | Type | Content/action | Presentation |
|---|---|---|---|
| `main_menu_title` | Text | `ASCII FPS` | plain |
| `main_menu_start` | Button | `start_game` | `focus_glitch` |
| `main_menu_level_editor` | Button | `open_level_editor` | `focus_pulse` |
| `main_menu_settings` | Button | `open_settings` | `focus_pulse` |
| `main_menu_quit` | Button | `quit` | `focus_pulse` |

Migration target: protected `bootstrap_main.tui`. Legacy actions become strict system bindings:

- `start_game` -> `start_project`;
- `open_level_editor` -> `open_editor`;
- `open_settings` -> `open_settings`;
- `quit` -> `quit`.

### Settings screen — protected

Legacy layout: `assets/ui_layouts/settings.txt`  
Legacy root: `settings_container`

| Element | Type | Content/action | Presentation |
|---|---|---|---|
| `settings_title` | Text | `ACCESSIBILITY SETTINGS` | bright |
| `settings_ui_scale_value` | Text | dynamic scale value | bright |
| `settings_scale_decrease` | Button | `ui_scale_decrease` | plain |
| `settings_scale_increase` | Button | `ui_scale_increase` | plain |
| `settings_reduced_motion_value` | Text | dynamic Reduced Motion value | bright |
| `settings_reduced_motion_toggle` | Button | `toggle_reduced_motion` | plain |
| `settings_reset` | Button | `ui_scale_reset` | plain |
| `settings_back` | Button | `back` | plain |

Migration target: protected `settings.tui`. Dynamic text remains host-provided runtime state; it is
not persisted as a second source of truth. Required system bindings are
`ui_scale_decrease`, `ui_scale_increase`, `toggle_reduced_motion`, `ui_scale_reset`, and `back`.

### Quit confirmation — protected

Legacy layout: `assets/ui_layouts/confirm_quit.txt`  
Legacy root: `confirm_menu_container`

| Element | Type | Content/action |
|---|---|---|
| `confirm_quit_title` | Text | `QUIT GAME?` |
| `confirm_yes` | Button | `confirm_quit` |
| `confirm_no` | Button | `cancel` |

Migration target: protected `confirm_quit.tui` with strict `confirm_quit` and `cancel` bindings.

### Pause screen — editable project UI

Legacy layout: `assets/ui_layouts/pause_menu.txt`  
Legacy root: `pause_menu_container`

| Element | Type | Content/action | Presentation |
|---|---|---|---|
| `pause_menu_title` | Text | pause title | plain/default |
| `pause_menu_resume` | Button | `resume` | plain |
| `pause_menu_main` | Button | `return_to_main_menu` | plain |
| `pause_menu_settings` | Button | `open_settings` | plain |
| `pause_menu_quit` | Button | `quit` | plain |
| `animation_pause_glitch` | Animation | target `pause_menu_container` | `pause_glitch`, `context_enter`, horizontal, one-shot |

The root also carries legacy `transition=center_out`. Migration target: editable project
`pause_menu.tui`. `return_to_main_menu` becomes contextual `return_to_bootstrap`.

The project pause UI is selected by the deterministic conventional asset name `pause_menu` for
the initial migration. A general project-role assignment system is outside Phases 1-6; absence or
invalidity activates the emergency pause fallback.

### HUD overlay — deferred editable UI Scene

Legacy layout: `assets/ui_layouts/hud_overlay.txt`  
Legacy root: `hud_container`

The layout contains `hud_grid`, `hud_frame`, `hud_mode`, `hud_prompt`, `hud_sep`,
`hud_stats_title`, `hud_target_fps`, `hud_actual_fps`, `hud_avg_frame`, `hud_worst_frame`,
`hud_min_spare`, and `hud_status`. Most values are runtime-derived text.

The HUD must ultimately use the same UI Scene document and renderer, but HUD migration is not a
Phase 1 schema requirement and is not required before pause/protected screen parity. Runtime text
bindings need their own focused design rather than persisted duplicate values.

### Reusable legacy Animation templates

| Asset | Preset | Trigger | Orientation | Loop | Randomize |
|---|---|---|---|---:|---:|
| `animation_pause_glitch` | `pause_glitch` | `context_enter` | horizontal | 0 | 0 |
| `animation_center_out` | `center_out` | `context_enter` | radial | 0 | 0 |
| `animation_perimeter_burst` | `perimeter_burst` | `context_enter` | horizontal | 0 | 0 |
| `animation_local_glitch` | `local_glitch` | `focus` | horizontal | 1 | 0 |

These names describe reusable templates, not permanent singleton runtime objects. The future typed
Add workflow may instantiate their field values into a staged v4 Animation element.

### Current authored `.tui` collision

`assets/menus/main_menu.tui` is an editable flow Menu named `main_menu`, while the application also
owns a legacy bootstrap main menu. Phase 6 must not silently rewrite it. Before protected bootstrap
migration, the author must choose a non-conflicting project UI name or explicitly remove the
project flow Menu if it is no longer wanted.

## UI document v4 decisions

These decisions are frozen for Phase 1. Reopening one requires updating the authoritative plan and
this record before implementation.

### Version and compatibility

- Current write version becomes `ui_version=4`.
- Versions 1-3 remain readable and migrate in memory to v4.
- Save writes canonical v4 only.
- Existing `kind=menu` remains accepted and migrates to `role=screen`.
- v4 writes `kind=ui_scene` and a required `role` field.

### Initial role vocabulary

The persisted v4 roles are:

- `screen`: focusable full UI context suitable for project flow or protected screens;
- `overlay`: UI composited over another runtime context, initially used by pause and later HUD-like
  interfaces.

Both roles share the same document, renderer, workspace, and runtime modules. Role controls host
composition and valid contextual actions; it does not select a separate renderer.

Migration default for v1-v3 `kind=menu` is `role=screen`.

### Button binding model

Each Button has exactly one binding kind:

- `flow`: required non-empty `port`, empty `action`;
- `system`: required non-empty validated `action`, empty `port`;
- `none`: empty `port` and `action` for non-activating presentation Buttons.

V1-v3 Button migration:

- non-empty `port` -> `binding=flow` with the same port;
- an empty port -> `binding=none`;
- no old file is inferred to have a system binding.

Only `binding=flow` Buttons contribute ports to `UiFlowReferenceView`.

### Initial system-action vocabulary and context policy

The document validator recognizes this fixed initial vocabulary:

- `start_project`;
- `open_editor`;
- `open_settings`;
- `quit`;
- `confirm_quit`;
- `cancel`;
- `back`;
- `ui_scale_decrease`;
- `ui_scale_increase`;
- `ui_scale_reset`;
- `toggle_reduced_motion`;
- `resume`;
- `return_to_bootstrap`.

Recognition does not grant permission. A separate host policy validates requests:

| Context | Allowed actions |
|---|---|
| protected bootstrap | `start_project`, `open_editor`, `open_settings`, `quit` |
| protected settings | scale actions, Reduced Motion toggle, `back` |
| protected confirmation | `confirm_quit`, `cancel` |
| editable pause overlay | `resume`, `open_editor`, `open_settings`, `return_to_bootstrap`, `quit` |
| ordinary project screen | no system actions by default |
| emergency startup | compiled recovery actions only |
| emergency pause | compiled recovery actions only |

A recognized but disallowed action produces a typed no-mutation policy error at the host boundary.

### Effect slots and class compatibility

Every v4 element serializes four effect slots for canonical completeness, but validation requires:

- Container: `entry_effect`, `exit_effect`; focus and activate must be `none`;
- Text: `entry_effect`, `exit_effect`; focus and activate must be `none`;
- Button: all four slots are allowed;
- Animation: all four slots must be `none` because the element itself defines presentation
  behavior.

V1-v3 migration sets all four slots to `none`.

Initial slot presets reuse the fixed accepted vocabulary: `none`, `center_out`,
`perimeter_burst`, `local_glitch`, `focus_pulse`, `focus_glitch`, and
`input_hold_short`, with a compatibility table enforced by slot and element class. The exact
execution status of `input_hold_short` remains metadata-only until separately designed; Phase 1
must preserve that explicit limitation.

### Animation target identity

Animation targets are persisted as stable numeric `target_id`, not names. Names remain editable and
must not silently retarget animations. The target must exist in the same document and must not be
an Animation element. Removing a referenced target is rejected until references are changed or the
referencing Animation is removed in the same explicit command.

V1-v3 migration creates no Animation elements and therefore needs no target default.

### Animation region coordinates

An Animation element remains a root-level presentation element with `parent=0`.

- `x` and `y` are signed offsets from the resolved target rectangle's top-left cell.
- positive `width` and `height` define the bounded effect region;
- `width=0` and/or `height=0` inherit the corresponding resolved target extent;
- negative extents reject validation;
- the resolved region is clipped to the target's resolved clip and then to the preview canvas;
- Animation elements do not participate in interaction, focus order, or flow ports;
- Animation painter order follows document order among presentation operations.

This keeps coordinate semantics explicit without adding a second anchor system to Animation.

### Animation fields and defaults

Required v4 Animation fields are `preset`, `target_id`, `trigger`, `orientation`, `loop`, and
`randomize`, in addition to common identity/layout/visibility fields.

Initial enums:

- presets: `pause_glitch`, `center_out`, `perimeter_burst`, `local_glitch`;
- triggers: `context_enter`, `context_exit`, `focus`, `activate`, `while_visible`;
- orientations: `horizontal`, `vertical`, `radial`;
- loop/randomize: strict `0|1`.

Typed Add defaults:

- preset `center_out`;
- target is the current selected non-Animation element;
- trigger `context_enter`;
- orientation `radial`;
- loop and randomize false;
- x/y zero and width/height zero (inherit target extent);
- visible true.

If no valid target is selected, Add Animation is unavailable rather than inferred.

### Editable roots and migration window

- Phase 1-6 continue to read editable documents from `assets/menus/`.
- `assets/system_ui/` is a separate protected root introduced only in Phase 6.
- `assets/ui/` becomes canonical only in Phase 7.
- During Phase 7 compatibility, duplicate UI Scene names across `menus/` and `ui/` are an error;
  one root never silently shadows the other.
- Original files are preserved until the author accepts migration and the flow catalog validates.

## Baseline tests added

### Exact production-sized renderer baseline

`test_production_80x40_preview_is_deterministic` renders the same staged document twice into an
80x40 `UiCanvas` and compares all cells and touched flags. It locks current centered geometry and
focused Button markers.

The test exposed a current rendering fact: a default left-aligned Button draws content beginning at
its left edge, then the focused `>` state marker replaces that first content cell. The baseline
therefore renders `>TART`, not `START`. This is confirmed current behavior, not an approval that the
future UI Scene renderer must retain content-marker overlap. Any intentional correction requires a
separate requirement and updated visual acceptance.

### Runtime/direct renderer equivalence

`test_runtime_render_matches_direct_production_render` constructs the runtime's exact element
states and proves `UiMenuRuntime` rendering matches direct `ui_render_document` output cell-for-cell
and touched-flag-for-touched-flag.

### Shared workspace host trace

`test_same_workspace_trace_is_host_independent` loads the same document into two independent
workspace instances and applies the same logical trace:

1. select the Button;
2. enter Properties;
3. change X;
4. move to Y;
5. change Y;
6. Undo;
7. Redo;
8. return to Hierarchy.

It compares the complete staged documents, mode, selection, property, history cursor/count, nested
cursor depth, and before/after command snapshots. This is the minimum reusable trace both future
hosts must drive through one shared action vocabulary.

## Failed attempts during Phase 0

1. The first focused compile failed because the existing workspace test registration uses commas
   at the beginning of subsequent entries and the new registration omitted one. The registration
   was corrected; no production code was involved.
2. The first renderer assertion expected vertically centered text on the middle row of a 3-row
   Button. The current renderer writes Button content on the resolved rectangle's first row.
3. The second renderer assertion expected the full `START` content, exposing that the focus marker
   replaces the first left-aligned content cell. The test was corrected to preserve the observed
   current baseline rather than changing production behavior in Phase 0.

## Verification evidence

Focused strict builds use `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`.

Verified after corrections:

- `test-ui-render-adapter`: **7/7 passed**;
- `test-ui-menu-runtime`: **7/7 passed**;
- `test-ui-menu-workspace`: **15/15 passed**.

Also verified after the focused suites:

- complete `make test-ui-standards`: **passed**;
- cppcheck through `make style`: **passed**;
- `make check-test-inventory`: **passed**;
- `make check-project-structure`: **passed**.

## Phase 1 entry decision

Phase 1 may begin. Its first code change must be confined to
the versioned `UiDocument` model, parser, validator, serializer, migration tests, and flow-reference
adapter behavior. It must not yet:

- route production application UI through v4;
- create protected assets;
- create the standalone editor host;
- migrate pause;
- change runtime rendering or interaction;
- remove legacy files or workbench code.

## Next safe action

Begin Phase 1 test-first by adding v4 document fixtures and migration expectations before modifying
`UiDocument` production code. Keep production application routing, runtime rendering, assets, and
both editor hosts unchanged until the Phase 1 document gate passes.

## Phase 1 slice A implementation evidence

**Implemented after the Phase 0 gate on 2026-09-17; Phase 1 remains incomplete.**

The first document-only slice adds:

- canonical `ui_version=4` writes with `kind=ui_scene` and `role=screen|overlay`;
- in-memory v1-v3 migration to `role=screen`;
- explicit Button `binding=none|flow|system` plus strict `port`/`action` compatibility;
- the frozen recognized system-action vocabulary;
- canonical entry, exit, focus, and activation effect fields;
- strict effect and element-class compatibility validation;
- flow-reference export from flow-bound Buttons only;
- compatibility behavior in `ui_document_set_flow_port()` that selects `binding=flow` and clears
  any system action;
- canonical v4 serialization and transactional v4 parsing.

Not implemented in this slice:

- Animation element type or fields;
- editor controls for role, binding, system action, or effects;
- system-action runtime policy or dispatch;
- effect execution in the `.tui` renderer/runtime;
- protected roots, protected assets, pause migration, standalone host, or legacy removal.

New document regressions cover:

- v3 role/binding/effect migration defaults;
- v4 overlay and system-binding round trip;
- system-bound Button exclusion from flow ports;
- unknown system action rejection;
- non-Button focus-effect rejection;
- unknown effect rejection;
- canonical v4 header and fields;
- none-bound Button promotion through the existing flow-port mutator.

Verified after the v4 slice:

- `test-ui-document`: **17/17 passed**;
- `test-ui-layout-resolver`: passed;
- `test-ui-render-adapter`: **7/7 passed**;
- `test-ui-interaction`: passed;
- `test-ui-menu-runtime`: **7/7 passed**;
- `test-ui-menu-workspace`: **15/15 passed**;
- `test-flow-project-catalog`: **3/3 passed**;
- `test-unified-editor`: **99/99 passed**.

Final checkpoint after documentation synchronization:

- strict default application build: **passed**;
- complete `make test-ui-standards`: **passed**;
- cppcheck through `make style`: **passed**;
- `make check-test-inventory`: **passed**;
- `make check-project-structure`: **passed**.

The next safe Phase 1 slice is Animation document data only: type, target ID, preset, trigger,
orientation, loop/randomize, root-level/region validation, canonical parsing/serialization, typed
document mutators, and flow/render/interaction exclusion. Runtime animation execution remains a
later phase.
