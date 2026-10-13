# The surface inspector as a corner panel: wall, ceiling and floor edit submenus

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 (the marks should be simple; the
behaviour should carry the richness), §1.3, §4.2 and its scope note (the editor interface), §4.4 (motion
never covers a control), §4.7 (nothing is hidden), §5.1 (citation), the **locked identity of 2026-10-09**
("Black, White, Red, Green, Blue and flowy, smooth motion"), and the **change of direction recorded
2026-10-13** ("edit-mode submenus are panels, not display surfaces"). This is the first edit-mode submenu
slice: the ones that come up while editing a **wall, ceiling or floor**.

## Summary

The owner asked for the UI language built for the menus, the HUD and the transitions to be applied to
the unified editor's **edit mode**, starting with the wall / ceiling / floor edit submenus. Those
submenus — the surface inspector and everything nested under it (the material picker, the decal list,
the optics and transparency lists, the height/gravity/movement rows) — were drawn as a ragged list of
`grid_print` rows at column 1 over the world frame: hardcoded markers and roles, no bed, no title.

Three corrections followed, in the owner's words: the panel "still got yellow/faint blue from the
previous color scheme"; the background should be "a more muted version ... It shouldnt distract from
actual text but it should still feel like theres motion", or the hard frame should go and the menu should
end where motion "froths/splashes"; and the entries were not aligned because "some stuff on the right
column is too long".

The panel is now:

- **a corner panel in the identity**, fitted to its content, holding the corner the ragged list held
  (column 1, bed from row 4) so the world stays visible while you edit;
- **black bed, white and neutral-grey text, and nothing else**: no `border` frame, no `warning` yellow,
  no `focus`/`accent` mint. The only colour is the material's three additive primaries as chromatic
  aberration — the deliberate retirement of the editor-interface hues the 2026-10-09 identity note left
  open;
- **edged by motion, not by a border, and dark inside but not flat**: the new **`living_shore`** backdrop
  is bound to the panel's own rectangle. One radial swell travels outward from its centre; in the body its
  crest ring is drawn as sparse one-pixel marks in a **dark shade of the material's own neutral** (an
  intensity of a palette colour, never a colour of its own), so the panel is a dark sheet whose rings
  ripple outward instead of a solid black; the outer band breaks into foam. The panel therefore reads as a
  darker area of the frame with a soft, breathing edge;
- **two aligned columns**: the label, then its value and keys, with the panel measuring the label column
  (`EDITOR_MENU_COLUMN_GAP`) instead of every source string padding itself by hand;
- **one indicator moves**: the live cursor carries the perimeter *with* its chase, and the entry whose
  submenu is open keeps the same frame **still** (`ui_ele_focus_perimeter_draw_still`);
- **unchanged in order**: the entries stay in the order the cursor walks. They were not re-sorted, because
  re-sorting the display would make the cursor jump around; the alignment is what fixed the raggedness.

The **selected surface** in the world also stopped being plain white: its projected outline (walls,
floors, ceilings, trigger regions) now **shifts its whole frame's characters** along the outline, every
cell of it carrying a step of the material's three additive primaries.

**Behaviour is untouched.** Which keys open, move, confirm and close each submenu, and every value it
edits, is unchanged. The whole change is the form the list is drawn in.


## What changed

- **`src/ui_effect.h` / `src/ui_effect.c`.** New primitive **`UI_EFFECT_LIVING_SHORE`** / preset
  `living_shore` (one registry row, per the registry's own rule): an ambient backdrop
  (`trigger=while_visible`, `loop=0`, `randomize=0`, any orientation), `backdrop = true`, and
  `covers_content = false` — it is a substrate, so it never passes over authored content.
- **`src/ui_animation.c`.** New **`render_living_shore`** and **`living_shore_shade`**, plus its dispatch
  case. One radial swell, sampled once per cell of distance and looked up, so the per-cell cost stays a
  lookup and a hash. Its **body** draws the crest ring as sparse one-pixel marks (`UI_SHORE_BODY_RATE`) in
  a **shade of the material's own neutral** (`living_shore_shade`: a percentage of a palette colour, so no
  literal colour enters the interface, §5.2), stepped from `UI_SHORE_BODY_SHADE` to `…SHADE_GAIN`; its
  **shore** shows foam at a density the swell drives, and a crest that breaks takes one of the identity's
  three primaries. Deterministic, explicit time, nothing drawn under reduced motion.
- **`src/unified_editor.c`.**
  - **`EditorMenuRow` / `editor_menu_collect`** now carry a **detail** as well as a label: the collector
    splits each line at its first run of two or more spaces, and drops the old `"> "` marker, which the
    perimeter carries.
  - **`editor_menu_panel_bed`** (the black bed), **`editor_menu_panel_material`** (the `living_shore`
    element, bound to the panel's rectangle) and **`editor_menu_panel_render`** (the two-column panel,
    the adaptive two-row rhythm, the margins that keep text out of the frothing band, and the perimeter).
  - The surface-inspector branch keeps collecting its rows through the local `#define grid_print(...)`
    that is `#undef`ed at the end of the branch, so the light/decal/trigger/object inspectors below are
    unaffected; its format strings were normalised so label and value split where they should (a second
    space before the value, and values moved out of the label).
- **`tests/test_unified_editor.c`.** **`assert_corner_panel`** now locks the placement (bed from row 4 of
  column 1), that the panel is bounded, and the identity: **no cell of the panel may carry the `border`
  grey-blue, the `warning` yellow or the `focus` mint.** New **`panel_detail_offset`** locks the aligned
  columns across two labels of different length. The re-spaced assertions were updated to the panel's
  label/detail split.

- **`src/ui_theme.c` / `src/ui_theme.h`.** The chromatic chase is now the material's own, exposed as
  `ui_theme_chase_head` (`UI_THEME_CHASE_LAP_MS`, `UI_THEME_CHASE_WIDTH`) and
  `ui_theme_chase_colour`, so the focus perimeter and the editor's selection outline carry one identical
  band instead of two rules.
- **`src/ui_ele.c` / `src/ui_ele.h`.** `focus_perimeter_draw_core` now samples that shared chase, and the
  **shape-only form** is a named entry point: **`ui_ele_focus_perimeter_draw_still`**.
- **`src/unified_editor.c`.** The panel takes the **last** focused row as its live cursor and draws that
  row's perimeter with the chase, every other focused row with the still form.
- **`src/editor_highlight.c` / `src/editor_highlight.h`.** The outline is built as **one closed path**
  (far edge left to right, the right side, the near edge right to left, the left side). A **selection**
  shifts its **whole frame's characters** along that path, one step per `EDITOR_HIGHLIGHT_SHIFT_MS`, from
  a ramp of dense glyphs that starts at the selected glyph, with each cell's colour taken from the shared
  material sampler -- so the frame is chromatic throughout. A hover stays the plain dashed outline.
  `now_ms` and `reduced_motion` are threaded through the render entry points, so the shift is explicit
  time and absent under Reduced Motion. The column chunk is one whole surface wide (512) so the editor's
  outline is a single path, and the side runs are drawn through the same path rather than as per-chunk
  bars.
- **`Makefile`.** `test-editor-highlight` and `benchmark-editor-highlight` link `src/ui_theme.c`, which
  now provides the chase.
- **Tests.** `test_edit_mode_submenu_moves_only_the_cursor` (two frames drawn, one band travelling),
  `test_focus_perimeter_runs_around_focused_button` (the still form draws the frame with no chase), and
  `test_selected_outline_carries_the_chromatic_chase` (a selected wall and floor outline holds a primary;
  Reduced Motion holds none). The highlight tests that are about the *contrast* outline pass
  `reduced_motion` so the chase does not stand in for the cell they sample.

## Failures found and fixed during the work

1. **The whole-frame version was the wrong placement**, and the bordered version was the wrong edge.
   Recorded by the owner and corrected here; both are recorded in the Reference of Record.
2. **The material drowned the text.** Drawn first over a cleared bed, the shore filled every cell and the
   text was written on top of it, which left grain in every gap between the letters. The material is now
   drawn last and the body keeps two cells clear of any glyph, so grain sits in the openings of the panel.
3. **The first threshold painted whole rings.** Quantising the swell to whole cells made each crest a
   complete ring of glyphs — a lattice over the panel. Both amplitudes are now broken by a static
   per-cell value, at very different rates, so the body is grain and the shore is froth.
4. **The label column was inflated by values.** Splitting on two spaces only works if the sources put two
   spaces before the value: `"Cell grav dir inherit  Left/Right"` made `"Cell grav dir inherit"` a label.
   Those format strings were normalised, which is what made the columns line up.
5. **A fixed panel width truncated the hint**, and a short fixture lost the tail of the list. The panel is
   bounded only by the display, and the rhythm is adaptive (2 → 1), both §4.7.
6. **The bed did not reach the frame.** The editor layer is copied cell by cell into the composited canvas
   by `ui_canvas_copy_grid_region`, which skips a **zero glyph** as "nothing here". A bed cleared to zero
   glyphs therefore left the world showing through the panel on a real display — the panel had no bed at
   all. The bed now carries a *space*, and `assert_corner_panel` asserts that every cell the panel painted
   is non-zero, whatever the material painted over it.
7. **The first ripple was a full field of glyphs**, which made the panel a busy striped sheet rather than a
   dark one; the second was a solid dark sheet, which was still not the *ripple* the owner asked for. The
   body now draws the crest ring only (sparse one-pixel marks in a dark shade), which is dark, moving, and
   readable as rings travelling outward.
8. **The material marked a space inside a label.** Drawn *after* the text so it could keep clear of it, the
   material saw a space inside a label as a blank cell and put a mark in the middle of the word — the dump
   caught `floor:surface`. The material is now drawn *under* the text, so a label's spaces are written over
   it and every word stays intact.

9. **The rotating band on the selection outline stuttered and was too thin.** The outline is one cell
   thick, so a five-cell band chasing it was barely a colour at all; and because the band's head advanced
   in whole cells along a path whose legs meet at corners, its steps strobed against the frame rate. The
   owner asked for the whole frame's characters to shift instead, which is what it does now: every cell
   of the frame changes together, one character and one colour a tick.

## Evidence

- `test-unified-editor` **101/101**, including the new `assert_corner_panel` (placement, bounded, identity
  hues) and `panel_detail_offset` (aligned columns) locks in the surface-inspector, decal-submenu and
  transparency-submenu tests.
- The rendering was reviewed as a **cell dump** of the real overlay. At the editor's own 260x160 frame the
  panel occupies rows 4-34 of the corner, entries two rows apart, the focused `Place Wall  Enter=apply`
  enclosed by the perimeter, every detail starting in the same column, body grain only in the openings,
  and foam along the top and bottom edges:
  ```
   6| - Inspector: floor surface                       |
   8|   Material         2                       . |
   9|  +------------------------------+  .|
  10|  |Place Wall       Enter=apply  ||
  11| -+------------------------------+|
  12|   Ambient          0.20                         -|
  14| - Decals           surface list  Enter=open  .|
  18| - Height           0.00  Left/Right|
  32|   Up/Down=choose  Enter=open/apply  Esc=back    *-|
  34|  *     ***   -  - =** ********   --  = *** - - **|
  ```
- A **ripple map** of the panel (a dump that marks the material separately from the text) shows the body
  rings curving out from the panel's centre, the foam along the shore, and the labels intact:
  ```
   5|..                   # ## ##  #          #              |
   7|       #                         #         #     .      |
  10|  #..... .... #     ...........  .  #                   |
  15|    #      #       #           #       #                |
  20| . ....... #      # .......  ............               |
  25|    #       #       ##       #                          |
  30|   ........... ..... ....... ... ... .... ....          |
  ```
- The body marks were measured at **`#2F3233`** — 28% of the fabric's `#A8B4B8` — so the ripple is dark
  by construction, and far below the text (`#F2F7F8`). `assert_panel_body_ripples` fails if the ripple
  stops being drawn.
- `test-editor-highlight` **21/21**, including `test_selected_outline_shifts_its_characters`: every cell
  of a selected wall outline and of a selected floor outline is one of the three primaries at a live time,
  one tick advances the character on a given cell, and Reduced Motion leaves no primary at all. A dump of
  the frame one tick apart shows the whole frame moving together:
  ```
  t=0        #%& @ %&8        t=1        %&8 # &8@
             #  8 #  %                   %  @ %  &
             @       &                   #       8
  ```
- A **frame dump** of the panel with the material picker open shows the two indicators behaving
  differently: the entry whose submenu is open is still white, and the submenu's own cursor carries the
  band (red and green corners at that instant):
  ```
    '+' ( 7, 2) fg=f2f7f8      <- the material row's frame: still
    '+' ( 9,19) fg=f2f7f8
    '+' (11, 2) fg=ff2b2b      <- the picker cursor's frame: the chase
    '+' (13, 2) fg=2bff4f
  ```
- A colour dump of the panel confirmed the retired hues are gone: only `#F2F7F8` (text), `#A8B4B8`
  (fabric and foam), `#05080A` (bed), the dark ripple shades and the primaries appear, where before it
  drew `#FFD166` (warning) on the notices and `#64757C` (border) around the bed.

## Validation

- `make` (`-Werror`) clean; `./build/test-unified-editor` 100/100.
- `make test`, `make test-ui-standards`, `make check-ui-workbench-frame`, `make standards` — all exit 0.

## Not done here / next

- **No native visual capture was reviewed on a display**; the form is verified by the tests and the cell
  dumps above. The material's marks, shades and densities are named constants (`UI_SHORE_BODY*`,
  `UI_SHORE_FOAM*`), so tuning the ripple or the foam is one value.
- **The other inspector kinds are still column-1 lists.** The light, decal, trigger, object and sprite
  inspectors (and the workspace panels) are unchanged; this slice is the wall / ceiling / floor submenus
  the owner named first. Converting the rest is the next slice, on the same helpers.
- **The panel is content-fitted, not windowed.** If a list is taller than the frame (only in the short
  test fixtures; the editor's frame is 160 rows) the panel takes what is left of the frame from its
  corner. Windowing the list would be the fix if it ever matters on a real display.
- **The submenus still open as cuts.** Applying the locked `TRANSITION` tide to the panel opening and
  closing is a separate slice; the panel currently appears and disappears immediately.
- **The entry order was not re-sorted** (the owner offered "length/alphabet"); the order is the cursor's
  order, so sorting it for looks would desync navigation. The alignment alone fixed the raggedness — if
  the owner still wants sorted entries, navigation has to follow the display order first.

