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
- **edged by motion, not by a border**: the new **`living_shore`** backdrop is bound to the panel's own
  rectangle. One radial swell travels outward from its centre; the body shows only the crest, broken into
  faint grain that keeps two cells clear of every drawn glyph, and the outer band breaks into foam. The
  panel therefore reads as a darker area of the frame with a soft, breathing edge;
- **two aligned columns**: the label, then its value and keys, with the panel measuring the label column
  (`EDITOR_MENU_COLUMN_GAP`) instead of every source string padding itself by hand;
- **unchanged in order**: the entries stay in the order the cursor walks. They were not re-sorted, because
  re-sorting the display would make the cursor jump around; the alignment is what fixed the raggedness.

**Behaviour is untouched.** Which keys open, move, confirm and close each submenu, and every value it
edits, is unchanged. The whole change is the form the list is drawn in.


## What changed

- **`src/ui_effect.h` / `src/ui_effect.c`.** New primitive **`UI_EFFECT_LIVING_SHORE`** / preset
  `living_shore` (one registry row, per the registry's own rule): an ambient backdrop
  (`trigger=while_visible`, `loop=0`, `randomize=0`, any orientation), `backdrop = true`, and
  `covers_content = false` — it is a substrate, so it never passes over authored content.
- **`src/ui_animation.c`.** New **`render_living_shore`** and **`living_shore_gap`**, plus its dispatch
  case. One radial swell, sampled once per cell of distance and looked up, so the per-cell cost stays a
  lookup and a hash; two amplitudes over that swell (a thin crest in the body, a much lower threshold at
  the shore), each broken by a static per-cell value so the material drifts rather than flickers; the
  splash and the breaking crest take one of the identity's three primaries. Deterministic, explicit time,
  nothing drawn under reduced motion, and the body keeps two cells clear of any glyph.
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

## Evidence

- `test-unified-editor` **100/100**, including the new `assert_corner_panel` (placement, bounded, identity
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
- A colour dump of the panel confirmed the retired hues are gone: only `#F2F7F8` (text), `#A8B4B8`
  (fabric), `#05080A` (bed) and the primaries appear, where before it drew `#FFD166` (warning) on the
  notices and `#64757C` (border) around the bed.

## Validation

- `make` (`-Werror`) clean; `./build/test-unified-editor` 100/100.
- `make test`, `make test-ui-standards`, `make check-ui-workbench-frame`, `make standards` — all exit 0.

## Not done here / next

- **No native visual capture was reviewed on a display**; the form is verified by the tests and the cell
  dumps above. The material's densities (`UI_SHORE_*`) are named constants, so tuning it is one value.
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

