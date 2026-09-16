# V1-1 Application Menu Palette Adapter — 2026-09-16

> **Superseding amendment:** The current palette mapping was subsequently manually approved.
> The pending-review and stop-gate text below preserves this increment's original status.

## Outcome and status

The first bounded V1-1 presentation consumer is implemented and automatically verified.
Application menu selected/unselected colors now consume the provisional semantic palette
through a narrow adapter. **The visual result remains provisional pending manual review.**

No authored Menu presentation, application UI asset, geometry, label, action, traversal,
pointer behavior, persistence, format, or motion behavior changed.

## Implementation

Added `src/ui_app_theme_adapter.h` and `src/ui_app_theme_adapter.c`. The adapter:

- borrows the immutable provisional `ui_theme` values;
- maps selected foreground to `focus`;
- maps selected background to `selection_background`;
- maps unselected foreground to `text_secondary`;
- maps unselected background to `canvas`;
- converts those four values to the existing `SDL_Color` boundary;
- allocates nothing and owns no state;
- rejects a null output without side effects.

`menu_sync_button_colors()` in `src/app.c` now consumes that mapping. Its existing call to
`ui_layout_set_focus()` and the `ui_ele_render()` `> <` markers are unchanged. The old
four procedural literals were removed.

Added `tests/test_ui_app_theme_adapter.c` with three focused tests for exact semantic-role
mapping, alpha preservation and repeated stability, and null-output rejection. The runner
is registered in `make test` and `make test-ui-standards`.

## Preserved requirements

- Application UI and authored `UiDocument` ownership remain separate.
- Authored normal colors remain author-owned and untouched.
- Focus remains keyboard-driven and color-independent through `> <` markers.
- Menu actions, labels, geometry, asset bytes, ordering, clipping, activation, and scale
  composition remain unchanged.
- Existing one-row application controls do not gain pointer behavior, and pointer target
  conformance is not claimed.
- No user theme setting, storage, migration, or fallback file was added.

## Verification

- Strict focused adapter compile and run: **3/3 passed**.
- Strict application build: pass.
- `make -j2 test-ui-standards`: pass across all eleven focused UI owners.
- Focused adapter ASan/LeakSanitizer: **3/3 passed**, no diagnostics.
- Focused adapter UBSan: **3/3 passed**, no diagnostics.
- `make -j2 check`: pass; strict application, complete functional suite, real cppcheck,
  static-analysis policy, and structural standards all passed.
- Native Linux X11 display/input acceptance: pass with presentation, resize, keyboard,
  synthetic pointer telemetry, and main-menu-to-playing transition.
- `git diff --check`: pass.

Native evidence is under
`build/display-acceptance/linux-x11/`. The captured main-menu frame at 100% contains the
provisional selected/unselected palette and both focus markers without an obvious clipping
or placement failure. This observation is diagnostic, not final visual approval.

## Mandatory manual evaluation

Review main, pause, settings, and confirm-quit menus at 100%, 125%, 150%, and 200% for:

1. focused versus unfocused legibility;
2. `> <` marker clarity without relying on color;
3. mint focus and dark-green selection character;
4. secondary text readability against the near-black canvas;
5. label clipping or alignment regressions;
6. focus continuity while navigating, changing scale, backing out, and confirming/canceling.

Approval applies only to this palette mapping. Geometry, pointer targets, status colors,
editor overlays, authored Menu states, and motion remain separate provisional work.

## Rollback

Restore the four former literals in `menu_sync_button_colors()` and remove the isolated
adapter/runner registration. No data restoration or migration is needed.

## Stop gate

Do not migrate another component family until the application-menu palette is manually
approved, revised, or rejected. Automated and native-display checks do not close that
product decision.