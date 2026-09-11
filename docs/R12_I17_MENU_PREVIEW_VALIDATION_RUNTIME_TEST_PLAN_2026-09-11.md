# R12 I17 Menu Preview, Validation, and Runtime-Test Plan — 2026-09-11

## Status

**Implemented; focused automated verification passed on 2026-09-11.** I17 completes the
planned R12 implementation sequence with session-only responsive preview settings, runtime-like
Menu testing through the existing I9 host, current staged-reference diagnostics, and copied typed
target reports. It performs no target loading, graph rewrite, or application-state transition.

## Preview settings

`E` exposes Preview Settings for every hierarchy selection. The session-only picker provides:

- base resolution presets 40×15, 60×20, and 80×25;
- UI-scale presets 100%, 125%, 150%, and 200%; and
- semantic Up/Down field selection plus wrapping Left/Right adjustment.

The effective logical viewport is `base × 100 / scale`, using integer division. This exercises
the existing responsive I4 resolver at the logical cell density implied by the selected UI scale
without introducing another persisted field or duplicate pixel compositor. Settings never dirty
the Menu or consume history and reset to 80×25 at 100% on workspace open/reset.

## Runtime-like Test mode

`Tab` from an open Menu attempts to enter Test mode. Entry transactionally:

1. refreshes the authoritative project Scene/Menu catalog;
2. overlays the staged Menu's current exported ports onto a copied catalog;
3. loads a copied `<asset-root>/game.flow`;
4. validates the copied graph against the copied current catalog;
5. binds a copied flow session directly to the matching Menu node; and
6. activates the existing I9 `UiMenuRuntime` at the selected effective viewport.

Failure leaves Edit mode active and shows a typed diagnostic such as flow unavailable,
missing asset, type mismatch, or missing/stale port. Test mode borrows the staged `UiDocument`
and production assets/theme, but owns copied flow/catalog/session/runtime state. It does not
modify Menu history, flow-workspace history, scene history, or disk files.

In Test mode arrows use I9 directional focus and Enter performs deterministic confirm-down then
confirm-up. Rendering routes through I9, so focused theme states and non-color markers use tested
runtime behavior. Escape or Tab returns to Edit without losing staged changes. Display-backed
pointer activation is explicitly deferred after the second manual retest; keyboard Enter is the
accepted R12 activation path.

## Target reporting and reference boundary

A successful activation copies the typed target node ID, Scene/Menu type, and asset name into
editor report state, exits Test mode, and displays `reported only`. It does not load that asset,
change application state, or mutate the copied/on-disk graph. Test entry after an unsaved Button
port edit validates that current staged port and exposes stale `game.flow` edges immediately.

`flow_project_catalog_overlay_entry` is a narrow failure-atomic API that replaces or adds one
typed copied catalog entry. Invalid entries preserve the caller's catalog byte-for-byte.

## Preserved boundaries

I17 adds no schema fields, UI element types, HUD/game-state substitutions, target loading,
application-menu replacement, editor-shell self-editing, graph mutation, or cross-document
history. Application-owned `ui_layouts` and `ui_elements` remain outside authored-game discovery.
Preview/test colors continue to use the centralized transient theme; authored normal colors do
not become the only focus/pressed indicator.

## Final verification evidence

- strict project catalog: **3/3 passed**;
- `FlowDocument`: **7/7 passed**;
- shared nested-inspector/Flow workspace: **9/9 passed**;
- strict `UiMenuWorkspace`: **12/12 passed**;
- strict render adapter: **6/6 passed**;
- strict unified editor: **97/97 passed**;
- strict production compile: **passed** under C11 `-Wall -Wextra -Wpedantic -Werror`.
- clean optimized aggregate and final `make check`: **passed**. Clean invocations exceeded
  the 120-second command harness during compilation; exact no-clean resumes completed with
  status 0;
- full ASan/LeakSanitizer: **passed**, status 0, no reports after bounded compile resume;
- full UBSan: **passed**, status 0, no reports after bounded compile resume;
- canonical eight-mode `make matrix`: **8/8 passed**; conflicting tracker selections were
  rejected during Makefile evaluation with status 2;
- default `make smoke`: **passed** with `{"smoke":"ok","map_width":10,"map_height":6}`;
- legacy/current-renderer guards: **passed**;
- `make style`: status 0 with the documented `cppcheck`-unavailable skip; and
- `make leak`: Valgrind unavailable; the ASan/LeakSanitizer leak gate completed instead.

Automated Q4 confirmed authored discovery only uses `scenes/*.tscene` and `menus/*.tui`;
Test mode has no target-loading, app-state, graph-save, or cross-history path; and shared
`ui_nested_inspector` stepping, presentation, and actual parent/child Enter/Escape cursor
transitions are used by Flow and Menu controllers. No renderer hot path changed, so no new
benchmark gate applies.

The checked-in acceptance graph is now Start → Menu:`main_menu` → Scene:`testscene`. It validates
against the checked-in catalog and provides an immediate report-only Test-mode path. Flow
rejections preserve typed causes, and zero-port Scenes explain where to author an `exit_flow`
trigger. Successful direct Actions operations now pop shared nested depth before selecting their
result, preventing repeated hierarchy work from exhausting the nested cursor.

R12 is **Ready for Final Manual Retest**, not Verified. See
`reviews/2026-09-11-roadmap-r12-closeout.md`.