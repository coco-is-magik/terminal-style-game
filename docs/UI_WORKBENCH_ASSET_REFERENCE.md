# Application UI Workbench Asset Reference

This is the copy/paste and text-prompt reference for application-owned UI under
`assets/ui_elements/` and `assets/ui_layouts/`. It does not apply to authored
`assets/menus/*.tui` documents.

## Workbench controls

Run `make ui-workbench`.

- Up/Down: select an active element or animation unit.
- Enter: toggle move mode; arrows move the selected unit one cell.
- Tab: cycle editable properties.
- `[` / `]`: change the selected property and save immediately.
- Ctrl+N: open the existing-unit chooser; Up/Down chooses, Enter clones/adds, Escape cancels.
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
preset=<pause_glitch|center_out|perimeter_burst|local_glitch>
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

- Workbench and normal run call the same evaluator.
- Timings come from theme roles: enter 160 ms, exit 120 ms, focus/activation 80 ms,
  while-visible relationship treatment 120 ms. Explicit looping repeats the corresponding role.
- `focus` runs only while the target Button is focused.
- `activate` runs when an action in the containing layout activates; it never delays the action.
- `context_exit` uses a bounded departing-menu snapshot while destination controls appear
  immediately.
- `randomize=1` deterministically scatters pause-glitch glyphs inside x/y/width/height. The same
  unit name produces the same placement; there is no frame-dependent random state.
- Reduced Motion suppresses spatial animation immediately.

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

## Existing reusable templates

- `animation_pause_glitch.txt`
- `animation_center_out.txt`
- `animation_perimeter_burst.txt`
- `animation_local_glitch.txt`

Use Ctrl+N in the workbench to clone any existing UI element or one of these animation units.

## Adding one new preset later

A genuinely new rendering behavior requires only:

1. one named case in the shared `src/ui_animation.c` evaluator;
2. strict parser validation in `ui_ele_animation_is_valid()`;
3. deterministic normal/reduced-motion tests in `tests/test_ui_animation.c`;
4. one reusable `assets/ui_elements/animation_<name>.txt` template;
5. one example in this reference.

Do not add a workbench-only renderer. Both modes must use the shared evaluator.