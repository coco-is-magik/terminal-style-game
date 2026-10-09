# Editor scene browser: from a framed panel to a display surface

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 (keep the controls dependable, let the
display feel alive), §1.3 (display material inside a surface), §2.1 (§5.2 alignment: no independent
flicker), §3.1 (all colour from tokens), §4.2 plus its **2026-10-07 scope note** (the white-dominant
rule governs the editor interface's own panes, footers, tooltips and status text; authored menus, the
HUD, the world and visualizations are *display surfaces* and are governed by §1), §4.4 (nothing in
motion displaces or obscures a control), §5.1 (the sanctioned editor-literal-to-token work), §5.2
(auto-rejections: a new hardcoded colour; a pane renderer created beside the existing ones).
Target document: UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §3.5.
Supersedes: [`2026-10-08-editor-scene-browser.md`](2026-10-08-editor-scene-browser.md).

## Summary

The editor's **OPEN SCENE / IMPORT LEGACY MAP** browser was rebuilt on 2026-10-08 as a *framed panel*
inside the editor's 100x40 interface surface. Presented beside the authored main menu it was still not
the language the menus now speak: the menus are **full-display surfaces** — the living-field material
fills the frame, and a centred block of title and controls sits on it.

The browser is now that same surface: the material fills the whole display, and the title, the entry
count, the catalog rows and the hint form **one centred block**, with the focused row taking the shared
focus perimeter **with its chromatic chase** (exactly what an authored focused button does).

Behaviour is untouched: the same modal, keys (Up/Down, Enter, Ctrl+I, Ctrl+O, Esc), catalog, selection
and scroll semantics, and the same title, hint and empty-case strings.

Authoring decision recorded here: the browser is treated as a **mode-selection display surface**, not as
an editing pane, so §4.2's white-dominant restraint (its own panes, footers, tooltips, status text) does
not apply to it. Its *editing* surfaces (inspectors, pickers, status block, prompt modals) remain
restrained and are unchanged.

## What changed

- **`src/unified_editor.c` — `editor_render_scene_browser()`.** Full-display material, centred layout
  and the hint moved inline:
  - `editor_browser_field()` builds an `editor_scene_browser` container plus a `living_field` animation
    element with `extent = UI_EXTENT_SURFACE`, i.e. the **same shared evaluator the authored menus
    use**, filling the surface it renders into. No asset carries a size and no grid size is hardcoded
    (`src/ui_animation.c` resolves a surface-extent unit against the render surface).
  - The block is the title (row `grid->height / 2 - (block + 4) / 2`), the `n / total` count on the next
    row, the rows at the menu's three-row rhythm, and the hint two rows below the last row. The material
    is drawn first and the labels over it: the field writes only blank cells, so the text keeps its
    interior spaces and stays crisp while the material fills behind it.
  - The focused row is `primary_text`; the other rows are `secondary_text`; the perimeter is drawn by
    `ui_ele_focus_perimeter_draw()` **with** `now_ms`/`reduced_motion`, so the editor's list reads
    exactly like an authored focused button. Selecting a row is a state, so it takes `focus` rather than
    a hue of its own (no green, no literal).
- **`src/ui_ele.c/.h`.** `ui_ele_focus_perimeter_draw()` now takes `now_ms` and `reduced_motion` and
  draws the perimeter *with* the chroma chase. The 2026-10-08 shape-only form is withdrawn: it existed
  only because the browser was classified as an editor pane, and that classification is what this
  rework reverses. The element path is unchanged.
- **`src/unified_editor.h`.** `unified_editor_scene_browser_active()` reports that the browser is the
  surface on screen, so the application can composite it like a menu without reaching into the editor's
  internals. `UNIFIED_EDITOR_INTERFACE_COLUMNS/ROWS` (100x40) remain the editor's *design* size and the
  application's minimum-frame check, but no longer bound what the browser draws into.
- **`src/app.c`.**
  - `ui_resources.editor` is the frame (`grid_width x grid_height`) rather than 100x40, so a
    full-display surface has the whole frame to fill.
  - The editor branch clears the whole staging area (the surface is the frame now), and composites the
    editor layer at `UI_ANCHOR_CENTER` **while the browser is active** and `UI_ANCHOR_TOP_LEFT`
    otherwise: the browser's content is centred in its surface, so the surface must be centred on the
    frame for that content to land on the middle of the screen. The workspace panel keeps its top-left
    placement.
  - While the browser is active the **footer copy is not composited**. The footer layer shows the
    surface's own bottom rows at the frame's bottom-left, which is correct for the top-left workspace
    (that is why it exists: it guarantees the interface's last two rows stay readable) but wrong for a
    centred surface: it re-shows those rows unshifted, which is the failure recorded below.
- **`tests/test_unified_editor.c`.** `test_scene_browser_panel_is_palette_driven` is rewritten for the
  display-surface geometry; it also now asserts the hint is *inside* the centred block, not merely
  present somewhere on the surface.
- **`Makefile`.** `TEST_UNIFIED_EDITOR_SRC` carries `ui_animation.c` (the browser renders through the
  shared animation/effect path, as the menus do).

## Failure recorded: the hint at the surface's foot was cropped

The first display-surface version put the hint on the staging grid's last row (`grid->height - 2`), the
way the editor's other screens do. That is wrong for a centred surface, and the composed frame showed
it: the hint appeared detached at the right of the frame, cut off mid-string (`Up/Down  Enter=open  Ct`).

The cause was established by measurement, not guesswork:

- The application's active UI scale in this environment is **125%** (`ui_scale_presets`, applied by
  `ui_compositor_compose()`), so the editor surface is 2600x1600 px while the frame is 1366x768.
- `ui_compositor.c`'s `layer_origin()` places a `UI_ANCHOR_CENTER` layer by the *canvas's* size, so the
  surface is centred and **its outer rows and columns are cropped** — that is what keeps the browser's
  centred block on screen, and it is also why the hint row was off-screen (grid row 158 maps to y 1164).
- The application's footer layer then showed the *same* staging rows at the frame's bottom-left
  (`UI_ANCHOR_BOTTOM_LEFT`, origin x 0). That is aligned with a top-left surface and misaligned with a
  centred one, so the hint was re-drawn at its staging column, near the right of the frame, and clipped
  by the frame edge.

Rejected alternatives:

1. **Keep the footer row and move it.** Any y in the surface's bottom band is cropped at raised scale,
   and the footer copy's x mapping cannot agree with a centred surface. Rejected.
2. **Composite the browser top-left like the workspace.** Every unshifted surface of 260x160 at 125% is
   cropped on the right and bottom, so the centred block would be lost and the title would drift off the
   right edge. Rejected.
3. **Scale the surface down to fit the frame.** Not available: `ui_compositor_compose()` scales *cells*
   by the global UI scale and never fits a canvas; fitting would be a compositor behaviour change, and
   §4.7 says the editor interface obeys the same UI scale as everything else. Rejected.

Accepted fix: **the hint belongs to the surface's centred block**, not to a footer row, and the footer
layer is not composited while the browser is the surface. The layout is then scale-agnostic by
construction: whatever band of the surface the frame shows, a centred block of at most `4 + 3*9 = 31`
rows is inside it.

## Evidence

- `make build/test-unified-editor`: builds clean under `-Werror`.
- `make test` (the aggregate): exit 0, no failures; it links and runs the rebuilt `test-unified-editor`
  together with the rest of the suite.
- `make test-ui-standards`: 24 runners pass, no failures.
- `make check-project-structure`, `check-test-inventory`, `check-legacy-unused`,
  `check-static-analysis-policy`, `check-unsafe-calls`, `check-current-renderer`,
  `check-ui-workbench-policy`: all pass.
- `./build/test-unified-editor`: **100 tests pass**, including the rewritten
  `test_scene_browser_panel_is_palette_driven`, which asserts the title, the count row, the focused row's
  `primary_text` colour, the perimeter's `focus`-coloured corner on the focused row, the hint's exact
  characters on the hint row at the centred column, that the hint row is inside the upper three quarters
  of the surface (the crop-safety property this rework is about), and that **every drawn cell** on the
  whole surface uses a theme role or a decorative material token.
- `make build/ascii-fps`: builds clean.
- **Composed-frame artifacts**, session-local, captured from the running application on the native X11
  session (`DISPLAY=:0`, `import -window`; drive: main menu → `Down` → `Return`, each step captured):
  - `/tmp/drive_down.png` — the drive step: the focus perimeter moved from `START GAME` to `EDITOR`,
    which is what makes the next frame the browser rather than the menu.
  - `/tmp/browser_open2.png` — the accepted surface: full-display material, `OPEN SCENE`, `1 / 4`, the
    four scenes, the perimeter and its chase on the focused row, and the complete hint centred below.
  - `/tmp/menu_open.png` — the authored main menu captured the same way, for comparison.
  - `/tmp/browser_open.png` — the *failed* pre-fix frame that produced the hint finding above.
  - Pointer activation is *not* used to enter the editor: the project's accepted activation path is
    keyboard `Enter` (a click only moves selection), so the drive is keyboard-only.
- **Where the rows are** was established with a temporary harness (`tests/probe_browser.c`, built against
  the existing `TEST_UNIFIED_EDITOR_SRC` list, since removed): it renders the browser at the
  application's 260x160 grid and prints each row's text. Recorded here because it is the cheapest way to
  answer "which row and column does the browser draw this on" without a display.

## Scope statements

- **Behaviour unchanged.** Every key, mode, catalog rule, selection semantic, scroll rule and string is
  untouched; the tests that cover them still pass unchanged.
- **Direction.** Menus and mode-selection surfaces are one family: dense, chromatic, alive, with a
  centred block of controls that stay crisp and stable. The editor's editing surfaces stay restrained
  under §4.2. This rework does not start a redesign of the editor's other screens.
- **Verified in this environment at the active UI scale (125%).** The layout is scale-agnostic by
  construction (see the fix above) and the regression test pins the property, but a native capture at
  100% and at 150% was **not** taken.
- **Not yet done:** the rest of the editor interface (status block, flow workspace, save and prompt
  modals, pickers) still draws as column-1 text. The material, the perimeter and the palette are now
  shared, so the next surface is a smaller change.

## Next safe action

Rebuild the next editor screen — the status block is the largest — on the same surface: `living_field`
material through `editor_browser_field()`'s pattern, a centred block, the shared focus perimeter, and
`ui_app_theme_workbench_palette()` roles only. Re-capture the frame at the active scale before claiming
the look, and keep the footer rule in mind: a centred surface must not have its footer row copied
unshifted.
