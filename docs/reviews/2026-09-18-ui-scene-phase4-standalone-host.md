# UI Scene Phase 4 — standalone host and host parity

> ## ⚠️ WITHDRAWN — interface and parity claims not supportable
>
> This record's claims are withdrawn:
>
> - "**Shared hierarchy/inspector/preview/footer renderer**" — the shared module rendered the old
>   cramped stacked layout with hardcoded colours, a two-row footer whose save/undo line was
>   unconditionally overwritten, and a selection marker written inside the preview.
> - "**Host parity coverage**" — the parity tests rendered both hosts through the *same function*
>   and therefore could not detect an incorrect implementation.
> - Preview scale did not work: the standalone host applied no compositor or preference scale, so
>   the scale control only shrank the preview.
>
> What remains valid: the standalone lifecycle shell, the shared input-adapter seam, the staged
> workspace, and the fail-safe Save/Discard-on-close behaviour.
>
> Active plan: [`../APPLICATION_UI_EDITOR_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-18.md`](../APPLICATION_UI_EDITOR_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-18.md).
> Binding direction: [`../UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md`](../UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md).

Date: 2026-09-18

## Status

**Implemented; automated gates pass; manual 1920×1080 readability and smooth-interaction
acceptance remains pending.** Phase 4 must not be marked complete until that named manual gate is
recorded.

## Implemented

- `make ui-editor` launches the new `--ui-editor` run mode.
- `ui_editor_runtime` owns only standalone lifecycle, assets, coordinate conversion, explicit time,
  text-input synchronization, and frame presentation.
- The default standalone workflow opens the validated project UI Scene chooser rooted at `assets`.
- `ui_editor_host` is the single keyboard/pointer-to-workspace adapter used by standalone and
  embedded authoring.
- `ui_editor_presentation` is the single hierarchy/inspector/preview/footer renderer used by both
  hosts; the duplicate private embedded renderer was removed.
- Embedded report-only runtime-test data is passed as immutable diagnostics and an already-rendered
  bounded canvas. Presentation does not own flow policy or target loading.
- Editor chrome is rendered directly at host grid scale; workspace preview resolution/scale affects
  only the bounded production preview.
- Standalone close and Escape route through workspace Save/Discard behavior. There is no
  immediate-write path.
- Standalone text entry starts and stops SDL text input according to workspace mode.

## Host parity coverage

Equivalent keyboard traces verify identical:

- staged `UiDocument` bytes;
- history count and cursor;
- candidate state;
- explicit playback state and status;
- complete composed `Cell` arrays.

Equivalent pointer press/motion/release traces verify identical staged documents, one-entry history,
history cursor, and complete composed frames. Existing embedded tests continue to cover action panes,
text modes, remove prompts, detailed properties, drag/resize handles, runtime-test preview, and
report-only targets through the shared presentation module.

## Preserved boundaries

- No application UI Scene migration occurred.
- Preview cannot load flow targets, rewrite flow, or execute system actions.
- Protected assets, pause migration, and legacy removal remain deferred.
- The legacy `ui-workbench` command remains available as the explicitly legacy immediate-write
  application-UI tweak path; it is not used by `ui-editor`.
- Playback remains caller-time-driven; no hidden clock was added to workspace or presentation.

## Failures and corrections

1. Adding embedded runtime-test diagnostics directly as a `UiMenuRuntime` dependency made the
   shared presentation owner require the full runtime dependency graph. This was rejected. The host
   now supplies an immutable bounded `UiCanvas` snapshot instead.
2. The first shared presentation routing preserved most embedded frames but missed remove/rename
   modes, detailed visual values, and the resize handle. Those behaviors were moved into the shared
   presentation module; all 99 accepted unified-editor tests then passed unchanged.
3. The old embedded drawing body was temporarily excluded during parity stabilization and then
   removed after tests passed. No disabled duplicate remains.
4. Runtime inspection found that window close bypassed dirty Save/Discard and that text input was
   never started. Window close now routes through workspace escape/close prompts, and SDL text input
   follows workspace text modes.
5. Runtime inspection found that a clean chooser close could attempt one final render of an inactive
   workspace. The loop now exits before presentation.
6. Long aggregate commands exceeded the execution channel's observation window. They were run with
   explicit completion artifacts and polled; only completed exit codes are reported below.

## Automated evidence

Strict compilation uses `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`.

- `test-ui-editor-host`: **3/3 passed**;
- `test-ui-editor-presentation`: **2/2 passed**;
- `test-app-options`: **12/12 passed**;
- `test-unified-editor`: **99/99 passed** through shared host/presentation paths;
- complete `make test-ui-standards`: **passed**;
- complete `make test`: **passed**;
- `make test-build`: **passed**;
- strict default application build: **passed**;
- cppcheck through `make style`: **passed**;
- `make check-test-inventory`: **passed**;
- `make check-project-structure`: **passed**.

## Manual acceptance still required

Run `make ui-editor` on a native 1920×1080 display and record:

1. hierarchy, inspector, preview, and footer are readable;
2. preview scale changes do not resize editor chrome;
3. keyboard chooser/edit/candidate/save/discard operations are smooth;
4. pointer selection, move, resize, commit, and cancel are smooth and accurate;
5. window close with dirty edits visibly offers Save/Discard/Cancel;
6. no target is loaded and no flow is rewritten during preview/test interactions.

Until this review passes, Phase 5 must not begin.