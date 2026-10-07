# Application UI Workbench Asset Reference

This is the copy/paste and text-prompt reference for application-owned UI under
`assets/ui_elements/` and `assets/ui_layouts/`. It does not apply to authored
`assets/menus/*.tui` documents.

## Workbench controls

Run `make ui-workbench`.

- Up/Down: hover an element while unselected.
- Enter: select; arrows move the selected unit one cell. Escape deselects.
- Tab: MENU Transition/Add without selection; ELEMENT Style/Text/Visible/Align/Remove with selection. Both scopes remain visible, unavailable categories dimmed and skipped.
- P: cycle Normal/Focused/Compare runtime card states. Compare shows normal above focused. Runtime arrows replace brackets; editor outlines are not part of the samples.
- `[` / `]`: change the selected property and save immediately.
- Ctrl+N (unselected), or brackets on Add: open the existing-unit chooser; Up/Down or brackets choose, Enter clones/adds, Escape cancels.
- Backspace: request removal from the current layout; Enter confirms, Escape cancels.
- Ctrl+Left/Right: change application context.
- Ctrl+Enter: invoke the selected Button action where safe in the workbench.
- Ctrl+`-` / Ctrl+`+` / Ctrl+`0`: session-local workbench scale; the preview retains the persisted application scale.
- F9: five-page in-editor help; Up/Down changes page and Escape returns to the edit.
- F5: reload the active context; a failed load preserves the existing session.
- F10: session-local reduced motion toggle.
- Ctrl+Z: reverse the latest add/remove membership operation; this is not general edit undo.

Add clones the selected reusable asset to the current layout under a deterministic unique name.
Remove detaches the selected direct member from the current layout and preload list; its source
file remains available for reuse. Referenced parents/targets cannot be removed.

Membership writes publish a synchronized recovery record before replacing files. The next
workbench load reconciles interrupted writes before loading the cache. A persistent filesystem
failure or conflicting external master edit keeps recovery data intact and blocks loading;
it is never silently treated as a successful save. This assumes one authoring writer.

## Element types

Container, Text, and Button use the fields documented in `assets/README.md`. Animation is a
non-interactive presentation unit:

```text
name=<unique name>
type=animation
x=<offset from target>
y=<offset from target>
coords=absolute
width=<bounded region width>
height=<bounded region height>
visible=1
z_index=-1
align=left
preset=<pause_glitch|center_out|perimeter_burst|local_glitch|edge_trace|chromatic_register|command_flash|button_reassemble|panel_register|living_field>
effect=<reusable effect from assets/ui_effects/, e.g. ambient_field>   # optional alternative to preset=
target=<element name in the same active layout>
trigger=<context_enter|context_exit|focus|activate|while_visible>
orientation=<horizontal|vertical|radial>
loop=<0|1>
randomize=<0|1>
style=plain
transition=none
focus_effect=none
content=
```

Rules:

- **Reusable effects.** `assets/ui_effects/<name>.txt` defines a named, parameterised effect
  (`primitive=` plus optional `orientation`, `trigger`, `loop`, `randomize`). An animation element may
  bind one with `effect=<name>` instead of `preset=`; the element's own fields override the definition,
  so a menu can share an effect and still retune a value. `animation_main_field` and
  `animation_living_field` both bind `ambient_field`, and the main menu overrides `orientation` — so
  making the main menu use the pause menu's background is a one-line change. Effect identity and its
  constraints live in one registry (`src/ui_effect.c`); adding a primitive is one registry row plus one
  render function, and it appears in the workbench chooser automatically.
- Workbench and normal run call the same evaluator.
- Timings come from theme roles: enter 160 ms, exit 120 ms, focus/activation 80 ms,
  while-visible ambient treatment 1800 ms. Explicit looping repeats the corresponding role.
- `living_field` is an ambient backdrop (`trigger=while_visible`, `loop=0`, `randomize=0`, any
  orientation). It fills only empty cells within its bounded region, so it never obscures authored
  text, a control, or a focus marker; `orientation` selects the axis its travelling waves emphasise
  (`horizontal`, `vertical`, or `radial`, which folds them about the region centre).
- `focus` runs only while the target Button is focused.
- `activate` runs when an action in the containing layout activates; it never delays the action.
- `context_exit` uses a bounded departing-menu snapshot while destination controls appear
  immediately.
- `randomize=1` deterministically scatters pause-glitch glyphs inside x/y/width/height. The same
  unit name produces the same placement; there is no frame-dependent random state.
- Reduced Motion suppresses spatial animation immediately.
- `edge_trace` and `chromatic_register` require a Button target, `trigger=focus`,
  `orientation=horizontal`, `loop=0`, and `randomize=0`.
- `command_flash` requires a Button target, `trigger=activate`, `orientation=horizontal`, `loop=0`,
  and `randomize=0`; action dispatch remains immediate.
- Choosing one of these button presets in the workbench normalizes its constrained metadata and
  selects a Button target. Cloning one into a menu targets that menu's first Button. A menu without a
  Button rejects the clone without creating a file.
- `button_reassemble` requires a Button target; `panel_register` requires a Container target. Both
  require `trigger=context_enter|context_exit`, `orientation=radial`, `loop=0`, and `randomize=0`.
  Workbench preset selection normalizes them to `context_enter`; cloning selects the first compatible
  target and rejects without creating a file when none exists.

## Copyable units

### Reuse the exact pause glitch

```text
name=my_pause_glitch
type=animation
x=0
y=0
coords=absolute
width=24
height=18
visible=1
z_index=-1
align=left
preset=pause_glitch
target=pause_menu_container
trigger=context_enter
orientation=horizontal
loop=0
randomize=0
style=plain
transition=none
focus_effect=none
content=
```

### Center-out glyph evacuation

```text
name=my_center_out
type=animation
x=0
y=0
coords=absolute
width=24
height=18
visible=1
z_index=-1
align=left
preset=center_out
target=main_menu_container
trigger=context_enter
orientation=radial
loop=0
randomize=0
style=plain
transition=none
focus_effect=none
content=
```

`center_out` copies actual rendered target glyphs and displaces them away from the target center.
The target controls and hit behavior stay immediate and stable.

### Local focus glitch

```text
name=my_focus_glitch
type=animation
x=0
y=0
coords=absolute
width=20
height=3
visible=1
z_index=-1
align=left
preset=local_glitch
target=main_menu_start
trigger=focus
orientation=horizontal
loop=1
randomize=0
style=plain
transition=none
focus_effect=none
content=
```

### Deterministic random region

```text
name=my_random_glitch_region
type=animation
x=-4
y=-2
coords=absolute
width=32
height=22
visible=1
z_index=-1
align=left
preset=pause_glitch
target=settings_container
trigger=while_visible
orientation=vertical
loop=1
randomize=1
style=plain
transition=none
focus_effect=none
content=
```

### Palette-native edge trace

```text
name=my_edge_trace
type=animation
x=0
y=0
coords=absolute
width=20
height=1
visible=1
z_index=-1
align=left
preset=edge_trace
target=my_button
trigger=focus
orientation=horizontal
loop=0
randomize=0
style=plain
transition=none
focus_effect=none
content=
```

### Palette-native chromatic registration

```text
name=my_chromatic_register
type=animation
x=0
y=0
coords=absolute
width=20
height=1
visible=1
z_index=-1
align=left
preset=chromatic_register
target=my_button
trigger=focus
orientation=horizontal
loop=0
randomize=0
style=plain
transition=none
focus_effect=none
content=
```

This uses palette accent/focus channels. The single literal red/cyan comparison remains isolated in
the approved motion specimen; neither treatment communicates warning, failure, success,
confirmation, or destructive meaning.

### Immediate command flash

```text
name=my_command_flash
type=animation
x=0
y=0
coords=absolute
width=20
height=1
visible=1
z_index=-1
align=left
preset=command_flash
target=my_button
trigger=activate
orientation=horizontal
loop=0
randomize=0
style=plain
transition=none
focus_effect=none
content=
```

### Button context reassembly

```text
name=my_button_reassemble
type=animation
x=0
y=0
coords=absolute
width=20
height=1
visible=1
z_index=-1
align=left
preset=button_reassemble
target=my_button
trigger=context_enter
orientation=radial
loop=0
randomize=0
style=plain
transition=none
focus_effect=none
content=
```

### Small-group panel registration

```text
name=my_panel_register
type=animation
x=0
y=0
coords=absolute
width=40
height=16
visible=1
z_index=-1
align=left
preset=panel_register
target=my_container
trigger=context_enter
orientation=radial
loop=0
randomize=0
style=plain
transition=none
focus_effect=none
content=
```

Both presets reverse deterministically for `context_exit`, end cleanly at 160/120 ms, use only
palette accent/focus material, and draw nothing under Reduced Motion.

### Ambient living field

```text
name=animation_living_field
type=animation
x=-30
y=-9
coords=absolute
width=80
height=40
visible=1
z_index=-1
align=left
preset=living_field
target=pause_menu_container
trigger=while_visible
orientation=radial
loop=0
randomize=0
style=plain
transition=none
focus_effect=none
content=
```

A white-dominant flowing fabric travels across the bounded region over the 1800 ms ambient role and is
drawn as a substrate beneath unchanged controls. Occupancy is the product of two travelling axis waves
(so lit cells form coherent rectangular patches that migrate and breathe as one material) modulated by
a slower diagonal fold wave; the structure stays a neutral tone (`text_secondary`). Saturated colour is
**only an RGB chromatic-aberration fringe** of the weave: it rides the *edges* of the lit forms — where
the fabric meets a gap, the region border, or a drawn glyph — and is **only the three additive
primaries**, red on left edges, blue on right edges, green on the horizontal edges, so colour reads as
fringing dust on white edges and never as a fill, a secondary colour, a spatial hue ramp, or per-cell
confetti. The field is a **substrate**: it paints only blank cells, so it fills the surface *behind*
text and controls and its fringes run up against the glyphs, keeping the menu one continuous material
rather than elements on a black plate. It draws nothing under Reduced Motion and is bound to the main
and pause contexts as `animation_main_field` (`assets/ui_layouts/main_menu.txt`) and
`animation_living_field` (`assets/ui_layouts/pause_menu.txt`); see
`docs/reviews/2026-10-07-pause-living-field.md`.

## Existing reusable templates

- `btn_plain_technical.txt`
- `btn_bracket_command.txt`
- `btn_inverse_filled.txt`
- `btn_bracket_feedback.txt`
- `btn_plain_compact.txt`
- `btn_bracket_compact.txt`
- `btn_inverse_hero.txt`
- `btn_plain_marker.txt`
- `btn_confirm_affirm.txt`
- `btn_plain_cancel.txt`
- `animation_pause_glitch.txt`
- `animation_center_out.txt`
- `animation_perimeter_burst.txt`
- `animation_local_glitch.txt`
- `animation_edge_trace.txt`
- `animation_chromatic_register.txt`
- `animation_command_flash.txt`
- `animation_button_reassemble.txt`
- `animation_panel_register.txt`
- `animation_living_field.txt`
- `animation_main_field.txt`
- `assets/ui_effects/ambient_field.txt` — reusable effect definition (`primitive=living_field`,
  bound by both field animations)

Use Ctrl+N in the workbench to clone any existing UI element or one of these animation units.

### B1.1 button-template conventions

The `btn_<style>_<role>.txt` prefix identifies reusable button templates. The checked-in templates
use only the existing `plain`, `bracket`, and `inverse` renderer styles, existing focus effects, and
the accepted palette values. When cloned into an editable menu, the workbench gives the clone a
unique menu-owned name and reparents it to that menu's container; the source template remains
unchanged.

`focus_perimeter` is a focus effect: the focused element gets a cell frame one cell outside its
bounds, with a short chase of the shared chromatic-aberration colours (`ui_theme_chroma_fringe`)
running around it, advancing with explicit time. Only the focused element is framed. The main and
pause menu buttons use `focus_effect=focus_perimeter` in place of the `>`/`<` markers. Under Reduced
Motion the full frame is drawn without the chase, so focus is communicated by shape rather than
motion.

`assets/ui_layouts/button_gallery.txt` is a bounded diagnostic composition used by automated
load/render checks. It is registered in `master_map.txt` so its source elements are preloaded and
available to the workbench clone chooser. It is deliberately not a fifth workbench menu context and
not a shipping application screen.

Current data-only limitations:

- panel-integrated buttons need a separately authored container composition;
- segmented controls need multiple coordinated elements and interaction semantics that the current
  single-button asset does not provide;
- destructive meaning is not represented by decorative chromatic traces; a destructive template
  should be added only with an approved semantic-color treatment;
- B1.1 added no renderer behavior; B1.2/B1.3 add only shared evaluator presets and no authored `.tui`
  behavior.

## Adding one new preset later

A genuinely new rendering behavior requires only:

1. one named case in the shared `src/ui_animation.c` evaluator;
2. strict parser validation in `ui_ele_animation_is_valid()`;
3. deterministic normal/reduced-motion tests in `tests/test_ui_animation.c`;
4. one reusable `assets/ui_elements/animation_<name>.txt` template;
5. one example in this reference.

Do not add a workbench-only renderer. Both modes must use the shared evaluator.
