# Frozen-frame underlay: a menu over a paused game

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 (keep the controls dependable, let
the display carry the life), §2.1 (deterministic, explicit time, stable controls, colours from tokens
rather than literals), §4.1 (faithful preview — scoped below), §4.4 (nothing in motion displaces,
obscures, or delays a control). Target document: UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §3.1.

## Summary

A menu can now be shown over the frame that was on screen when it opened, instead of over a blank or
black surface. Pausing shows the paused game behind the menu, recessed by the theme's overlay
response.

This is the first slice of the menu-surface work. It is deliberately additive: no existing behaviour
changes unless a layout asks for it.

## What a layout asks for

`underlay=<dim|none>` on a layout (`assets/ui_layouts/*.txt`). `dim` is the default; `none` paints
the plain background exactly as before. `pause_menu`, `settings`, and `confirm_quit` are `dim`;
`main_menu` is `none` because it is never reached over a live frame. An unknown value fails the load
instead of silently reverting.

## What changed

- **`src/ui_underlay.c/.h` (new).** A pure snapshot module with a narrow API: `ui_underlay_init`,
  `ui_underlay_capture`, `ui_underlay_forget`, `ui_underlay_has_frame`, `ui_underlay_apply`, and
  `ui_underlay_response`. No clock, no randomness, no global state; the same captured frame and
  strengths always produce the same cells.
- **`ui_theme`.** An additive `UiThemeOverlay { dim_percent, grey_percent }` (currently 45 / 60).
  Strengths, not colours: the response applies to whatever was on screen, so one rule covers every
  surface instead of a literal per layout.
- **`ui_ele`.** `UiUnderlayMode` on `UiLayout`, zero-initialised to `UI_UNDERLAY_DIM` so the friendly
  default is also the safe one, plus `ui_ele_underlay_parse()` for the strict value.
- **`app.c`.** The frozen frame is captured once per surface session, on the frame a menu opens,
  while the world frame is still the live one, and forgotten when the last surface closes. The menu
  branch calls `app_apply_underlay()` and falls back to its plain background when the layout says
  `none`, no frame was captured, or the capture is refused.

## Why the frame is captured once

Capturing per session keeps a later frame from quietly replacing the frozen one, and keeps the
underlay from being applied to an already-recessed frame (which would double the dim on the second
menu of a stack). A frame whose cells are all empty is refused: it carries no background colour, so
it is not a background — that keeps startup and main-menu-reached surfaces on the plain background
instead of depending on the alpha of a zeroed cell.

## Evidence

- `test-ui-underlay` (new, 8 tests): capture size contract; identity restore at 0/0
  (`assert_memory_equal` against the captured frame); an empty frame is refused and an existing
  capture survives it; apply without a frame; the response's dim/grey values and alpha preservation;
  clamping and monotonic dim; theme strengths are usable; apply recesses and repeats exactly.
- `test-ui-ele` (23 tests, was 22): `test_ui_layout_underlay_default_and_opt_out` covers the strict
  value parser, the real assets (`pause_menu`, `settings`, `confirm_quit` = `dim`; `main_menu` =
  `none`), and that a layout file with `underlay=blur` fails to load.
- **Live display capture** on the native X11 session (`--display-acceptance 15`; Return starts the
  game, Escape pauses), window 1366x768, grey mean of a 220x160 world-only region at the top left:

  | State | Corner mean | Meaning |
  |---|---|---|
  | Playing | 0.2006 | the world |
  | Paused, `underlay=dim` | 0.0531 | the same frame, visibly recessed |
  | Paused, `underlay=none` | 0.0000 | pure black — the previous behaviour, preserved exactly |

  Artifacts (session-local): `/tmp/underlay_playing.png`, `/tmp/underlay_paused.png`,
  `/tmp/underlay_paused_none.png`. The paused capture shows the menu over the still-visible, greyed
  corridor.
- `make` (application) builds clean under `-Werror`. `check-ui-workbench-frame`,
  `test-ui-workbench-frame` (10), `test-ui-workbench-chrome` (24), `test-ui-workbench` (12), and
  `test-ui-workbench-store` (12) all pass **unchanged** — no fixture re-record was needed, which
  confirms the change is confined to the application's runtime composition.

## Scope statements

- **The underlay is runtime state, not authored content.** There is no authored definition of "what
  was on screen", so the workbench preview draws the authored surface over the plain background. §4.1
  (faithful preview) is about authored content, and nothing authored changed, so the recorded frame
  fixtures stay valid. A preview of the recessed frame would need a synthesised background and is not
  claimed here.
- **Decoration only.** The underlay repaints background material; it never moves a control, a glyph, a
  focus marker, or a hit target. It also does not persist transient overlays that are not part of the
  frame: the diagnostics HUD is a separate layer, so it does not appear over a paused menu.
- **Reduced motion.** No special case, because nothing here reads time; the response is a static
  transform of a frozen frame.
- **Not implemented, recorded so it is not mistaken for a capability:** the workbench has no control
  for `underlay`. It rewrites the `elements=` line and preserves other lines, so a hand-authored value
  survives a save, but it cannot be changed from the editor.

## Related owner decision (2026-10-08)

**One glyph size per frame.** Within the game frame — authored menus, the world, and the HUD — every
cell is the same size; the accessibility scale scales the frame as a whole. The centre crosshair and
the workbench's own editor chrome are explicit carve-outs (different instruments). This rules out
per-layer scale policies in the menu path: the visible menu box is therefore not to be fixed by
scaling a backdrop, but by making the menu surface the display.

## Deferred (not implemented)

- **Bounded-field (floating panel) layouts.** The box a menu occupies today is the 80x40 menu canvas
  (`APP_UI_MENU_WIDTH/HEIGHT`), a crop of the authored design space, not authored content. Slice 2
  makes the menu surface the display and gives an element a way to cover the surface, so a layout can
  choose a bounded field instead. Until that lands, the box remains a cropping rule and a
  "floating panel" layout cannot be authored.
- **Menu-surface coverage and scale policy.** Rejected by the one-glyph-size rule above: no
  `FIXED_100` backdrop, no coverage/scale policy enum, no per-layer scale in the menu path.

