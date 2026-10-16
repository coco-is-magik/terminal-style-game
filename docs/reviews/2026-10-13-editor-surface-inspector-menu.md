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

### Every menu, kept consistent (same day, owner follow-up)

The owner asked for the same treatment across the rest of the menus: "lets apply the current menu
appearance scheme to the rest of the menus we can pull up and have it consistently".

- **`src/unified_editor.c`.** The rows, the collector and the panel renderer are hoisted so the whole
  inspector chain shares them: `grid_print` is redefined to collect for the length of the chain and
  restored straight after it, so the workspace panels above and the modal prompts below keep drawing
  their own lines. The light, decal, sprite, trigger and object branches each set a title and a hint
  from their `EditorInspectorPresentation` and render the same panel; their format strings gained the
  second space that lets the panel split label from value. The object picker and the sprite menu's list
  stages collect as nested rows, exactly as the material picker does.
- **The document warning is the panel's note.** "document has material ID > 9 (unsavable)" was a line
  drawn over whatever was open; it is now the open panel's own note, and keeps its own line when no
  menu is open.
- **The sprite paint stage is its own screen.** `editor_render_sprite_pattern_menu` became
  `editor_collect_sprite_menu_rows` (the actions and load lists, collected) plus
  `editor_render_sprite_paint_screen` (the painter's own screen). The row model moved above both helpers
  so the sprite lists can fill the same rows the panel draws.
- **The painter's controls are the panel, top left, and its canvases are top right.** The paint screen
  draws its title, hint, the frame it is on, the seven frame actions and its paint keys in the shared
  panel, every line about as long as the others (the keys are wrapped, and `fps` and `loop` are their own
  rows) so the panel is not a bowtie; `editor_menu_panel_render` returns the first row clear below the
  panel, past its own edge and the one cell the material may splash past. The three canvases are aligned
  to the top right, a margin in from the frame's edge and a fixed gap apart, each in its own frame: the
  active canvas framed in bright white with bright content, the previews framed and drawn dimmer. Each
  canvas has a blank black background, a **character cell grid** so the drawable cells are visible before
  anything is painted, and nothing else — no bed, no material, no effect. The cursor is the identity's
  inverse, so it reads on an empty cell and a painted one alike. The fields panel is still not drawn
  while painting. *(Superseded by both 2026-10-14 follow-ups below: "top right" means the top right of the
  region the screen shows, a raised UI scale crops the frame's own far edge, the grid is a character
  lattice rather than a mark per cell, and the row this helper returns is used by every caller.)*
- **The arrangement is the navigation.** With the menu to the left of the canvas, Left off the canvas's
  first column now reaches the menu and either horizontal key returns to the canvas, while the canvas's
  own far edge still enters the menu. The hint says so.
- **Tests.** The light and sprite inspectors now assert `assert_corner_panel`; the assertions that looked
  for a `"> "` marker became assertions about the panel (one column, the perimeter on the focused entry,
  no marker), and the paint-screen test asserts that the fields are *not* drawn while painting.

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

- A **painter dump** shows the layout: the shared panel with its title, hint, frame line, actions and
  paint keys, its frothing edge, and then the canvas labels and the canvas below them, untouched:
  ```
   6| = SPRITE PAINT       ::                  :        :       :    =|
   7|   Right=edge enters menu  Left=canvas  Esc=discard        :     |
   9|   Frame 1/2                  :: :::                 :      :   -|
  11| - fps 4  loop true:       ::        ::              :      :   -|
  13| - Previous frame :       :            :                     :   |
  25|   Toggle loop       :                              :            |
  27| = Type=paint  Backspace=erase  [ / ]=material:1  Ctrl+S=save    |
  29|  --   **=-   ***= - == *****        *****   --= ***  - =* * -   |
  31|      previous   active                             next|
  32|      B          A                                  B|
  ```
- `test_sprite_animation_painter_focus_frames_save_and_discard` now asserts that no cell below the
  panel's canvas label carries the material's colour at all, which is what "nothing on the canvas"
  means in a test.

## Validation

- `make` (`-Werror`) clean; `./build/test-unified-editor` 100/100.
- `make test`, `make test-ui-standards`, `make check-ui-workbench-frame`, `make standards` — all exit 0.

## Not done here / next

- **No native visual capture was reviewed on a display**; the form is verified by the tests and the cell
  dumps above. The material's marks, shades and densities are named constants (`UI_SHORE_BODY*`,
  `UI_SHORE_FOAM*`), so tuning the ripple or the foam is one value.
- **The modal prompts and the two workspaces are not panels.** A prompt (unsaved changes, material
  collision, overwrite, remove) is a dialog drawn *over* the menu while it is up, and the flow and
  menu-UI workspaces are panes rather than menus, so both keep their own drawing. Every *menu* a
  selection pulls up is the shared panel; if the prompts should wear it too, that is the next slice.
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

## Follow-up: the painter's canvas and the dialogs (2026-10-14)

Three defects the owner found on a display, all of them things the cell dumps of the time had not shown.

**The canvases were placed against the frame's far edge, so they were never drawn.** `x =
grid->width - SPRITE_PAINT_MARGIN - total` put the block at column 223 of the 260-column frame. The
compositor draws a UI layer at the *UI scale* into the display's own buffer: at the default 150%
(`UI_PREFERENCES_EMERGENCY_SCALE`, the scale used when no preferences file exists) the editor's 260x160
surface is 1.5x, so only its first ~173 columns and ~106 rows are inside the buffer and the rest is
cropped — the same fact that already made the scene browser a *centred* surface. A canvas at column 223
was therefore not "far away", it was off the screen. *(Superseded by the second 2026-10-14 follow-up
below: the crop is real, but the fix is to right-align to the region the screen shows, not to move the
block beside the panel — which is the "alignment backwards" the owner reported.)*

**The editor's own lines were drawn on the panel.** `editor_menu_panel_render` returns the first row
clear below its bed, and every caller ignored it: the inspector branches left `row` at the row the last
collected field would have been printed on, and the painter returned the row below its *canvas* (row 14)
rather than below its panel (row 38). The status line and every prompt were therefore printed *through*
the menu — a quit prompt read `Document is DIRTY instance`, `Save and Exit50`, the dialog's lines
interleaved with the panel's. Every branch now hands the panel's returned row back to `row`.

**A prompt is a dialog and now has its own bed.** The block is collected (`EditorPromptLine`,
`editor_prompt_collect`) and drawn on a bed exactly as wide as its longest line
(`editor_prompt_render`), sliding up only to clear the editor's two footer rows when a short frame has
left it no room. On the editor's own 160-row frame the rows it is given are kept, so nothing moved; on
the 30-row test fixtures the dialog covers the menu instead of reading through it. This is *not* the
prompts wearing the menu panel — they remain dialogs — so that slice is still open.

Verification: the cell dumps now show the three canvases at columns 42-80 beside the panel, and the quit
prompt on rows 36-41, clear of the panel's bed (which ends at row 35). `test-unified-editor` 101/101;
`make`, `make test`, `make standards`, `make test-ui-standards`, `make check-ui-workbench-frame` all exit
0. The painter's test no longer asserts "top right" but that the whole canvas block is inside the frame's
left half, which is the part of the surface a raised UI scale always shows — the regression guard for the
crop. *(That assertion is superseded by the second follow-up below, which asserts the block's right edge
against the region the screen actually shows.)*

**Behaviour is untouched.** No key, focus or editing path changed: this slice is drawing plus the
placement of the canvases the painter already drew.

## Follow-up: the painter's alignment and its grid (2026-10-14, second)

Two more defects the owner found on a display, both of them *the previous follow-up's fix read backwards*.

**The alignment was backwards.** The previous follow-up moved the canvases to sit immediately beside the
panel, reading "the menu is to the canvas's left, so Left off the canvas reaches the menu" as the reason
for the placement. The owner's direction was not that the two are adjacent; it was the two *edges of the
screen*: "Lets try having the menu top left aligned, canvas/previews top right aligned. Dont press right up
on the edge of the screen, have some padding and arrange it neatly with good alignment." The canvas
belongs at the **top right of the screen**, with a padding margin — which the previous fix had turned into
the middle-left of the frame. Both readings agree that the frame's far edge is unusable (the crop below is
real), so the fix is not "somewhere near the panel" but **right-aligned inside the region the screen
shows**. The editor cannot know that region on its own: it is the UI scale that decides it, and the scale
lives in the application's preferences. So the region is now an input —
`unified_editor_set_interface_viewport(editor, columns, rows)`, called by `app.c` each frame from
`ui_preferences_scale` before the editor's overlay is rendered, with 0 meaning the whole frame (the
headless and test default, so nothing that renders without a display changes). The painter block is laid
out inside those columns: right-aligned a `SPRITE_PAINT_MARGIN` of 2 in from their edge, and when the
region cannot hold the panel and all three canvases, the block takes the room the panel has left and the
**previews are given up before the canvas being painted is**.

**The canvas was a field of dots, not a grid.** `editor_draw_sprite_canvas` drew one glyph per sprite cell
— `'.'` for an empty cell, the cell's glyph otherwise — so a 16x16 canvas was 256 marks with no visible
cell boundaries at all: a screen of dirt, not a grid (owner: "The canvas also is filled with dots, not a
grid, which is wrong"). It now draws a **character lattice**: the canvas occupies `cells * 2 + 1` columns
and rows (`SPRITE_CANVAS_CELL` is 2 — one character of content plus one of line), a `+` at every crossing,
`-` along the rows, `|` along the columns, all in the divider role's colour, and the sprite's cells are the
**interior** of that lattice. Empty cells are now a *space*, so the canvas reads as blank black squares
walled off from each other and anything painted in a cell stays legible against the lines around it. The
lattice's outer ring is the canvas's frame, so the frame the owner asked for in bright white *is* the grid
rather than a border drawn around a field of marks. Every measurement the painter makes — the block width,
the cursor's cell, the row below the canvas — is derived from `editor_sprite_canvas_span(cells) = cells *
2 + 1`, never from a raw column count, so the two can never drift apart.

**Behaviour is untouched.** No key, focus or editing path changed: this slice is drawing, the placement of
the canvases, and the new viewport input. The painter's focus model is unchanged — the menu is still the
canvas's only neighbour, so either horizontal key that would leave the canvas enters the menu and either
horizontal key returns.

**The block is three fixed slots, so the canvas never moves when a frame is added or removed.** The first
pass sized the block from the frames that *exist*: with one frame the canvas was alone and right-aligned,
with two it had a neighbour and shifted left, with three it sat in the middle — so the work moved under the
hand that was adding to it (owner, 2026-10-14: "The canvas shouldnt move when adding more frames. It should
be in the same location no matter what. The location its in when there are frames added ... should be the
location it always sits"). The three slots — previous, the canvas being painted, next — are now always
reserved, each as wide as a canvas (`editor_sprite_canvas_span` of the document's own columns, which every
frame of a sprite shares), and the canvas being painted always takes the middle one. Absent neighbours
leave empty slots rather than closing them up. When the region cannot hold the panel and the three slots,
the slots keep their order and slide right rather than the canvas moving on its own; only when even the
middle slot cannot fit beside the menu is the fallback taken — the canvas keeps the room, the neighbour
slots are given up, and the canvas is right-aligned in what is left, which still does not depend on how
many frames there are. A neighbour wider than the canvas being painted (only a hand-edited file can make
one, since every frame the editor creates shares the document's dimensions) is left out rather than
allowed to cover the work.

Verification: the cell dumps now show the canvas being painted at exactly one column for 4, 3, 2 and 1
frames, at 8x8, 16x16, 24x24 and 30x30 — columns 92, 78, 79 and 67 respectively, unchanged by the frame
count (the 24x24 and 30x30 sprites are the fallback: their three slots cannot fit 130 columns, and the
canvas is still fixed). `test-unified-editor` 101/101, including a new assertion that renders the painter
again after a frame is removed and finds the canvas in the same columns with the slots either side of it
empty. `make`, `make test`, `make standards`, `make test-ui-standards` and `make check-ui-workbench-frame`
all exit 0.

Verification: `test-unified-editor` 101/101, including a rewritten painter test that sets the viewport to
the owner's 200% region (130 of the frame's 260 columns, 80 of its 160 rows) and asserts that the block's
right edge is the region's last column minus the padding margin (127), that the active canvas's frame is
bright and the preview's dimmer, that the pattern's painted cell holds its glyph, that the intersection
between cells is a `+` in the divider colour, that the neighbouring empty cells are spaces, and that no
cell of the canvas is a `'.'`. `make`, `make test`, `make standards`, `make test-ui-standards`,
`make check-ui-workbench-frame` and `git diff --check` all exit 0. The composited frame was re-dumped at
200% and reviewed at 200% scale: the panel sits at the top left, the three canvases are right-aligned at
the top right with the padding margin showing as two clear columns at the screen's edge, the active canvas
carries the bright frame and the inverse cursor while its neighbours carry the dim one, and the drawable
area reads as a lattice of `+`, `-` and `|` around blank black cells. The same dump is also the proof of
the defect it fixes: composed from a render whose block was pinned to the frame's far edge, the canvases
did not appear *at all* — the crop is not a cosmetic worry. **No native visual capture was reviewed on a
display**; that is still the owner's to confirm, and it is the step that would settle the one assumption
here — that "top right" means the top right of the region the screen shows (the docs' own wording, "Dont
press right up on the edge of the screen") rather than some other corner of the surface.

