# Asset File Formats

All game data lives under `assets/` as plain text files.  Files are loaded by
either the runtime (`asset_loader.c`) or the editor modules (`decal_io.c`,
`ui_ele.c`).

## Maps (`assets/maps/<id>.txt`)

A simple digit grid.  Each row is a line of characters:

- `'0'` — empty/void tile (passable)
- `'1'`–`'9'` — solid wall using that material ID
- any other character — solid wall using `default_material_id` from `config.ini`

Rows must be non-empty. Ragged rows are accepted for compatibility and padded
with empty material cells to the width of the longest row. Maps are limited to
512 columns and 256 rows; empty or oversized maps are rejected without partially
populating the world. Example:

```
1111111111
1000000001
1022003001
1020003001
1000000001
1111111111
```

## Palettes (`assets/palettes/<id>.txt`)

Three colour stops for distance-based shading, one per line:

```
near=R,G,B,A
mid=R,G,B,A
far=R,G,B,A
```

Example:

```
near=255,255,255,255
mid=150,150,150,255
far=50,50,50,255
```

## Materials (`assets/materials/<id>.txt` or `<name>.txt`)

A key-value file describing which palette to use and the four distance glyphs:

```
palette=<palette_id>
glyphs=<4 characters>
```

The four glyphs are used from near to very far:

- distance ≤ 4 cells → first glyph
- 4 < distance ≤ 7 cells → second glyph
- 7 < distance ≤ 10 cells → third glyph
- distance > 10 cells → fourth glyph

Example:

```
palette=1
glyphs=#x-.
```

## Decals (`assets/decals/<name>.txt`)

A structured key-value file describing a surface decal.

Common fields:

```
surface=<0=wall, 1=floor, 2=ceiling>
x=<world x>
y=<world y>
z=<world z>
map_x=<map tile x>          # wall decals only
map_y=<map tile y>          # wall decals only
side=<0 or 1>               # wall decals only
u=<surface offset 0-1>      # wall decals only
v=<surface offset 0-1>      # wall decals only
width=<world or uv width>
height=<world or uv height>
glyph_step_u=<optional>     # override world-space anchor spacing on U
glyph_step_v=<optional>     # override world-space anchor spacing on V
depth=<offset from surface>
rotation=<radians>
pattern_cols=<integer>
pattern_rows=<integer>
default_material=<material_id>
pattern_0=<glyph string>
pattern_1=<glyph string>
...
material_0=<csv material ids>
material_1=<csv material ids>
...
```

Whitespace in the pattern rows is transparent.  Each `pattern_N` line must be
exactly `pattern_cols` characters long.  Each `material_N` line is a comma-
separated list of `pattern_cols` material IDs.  Example:

```
surface=0
map_x=2
map_y=2
side=0
u=0.2
v=0.4
width=0.6
height=0.2
pattern_cols=15
pattern_rows=1
default_material=4
pattern_0=This is a decal
material_0=4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
```

Pattern dimensions must be positive, with at most 255 columns and 64 rows.
Both structured rows and inline `art=` rows use the same parser and limits.
Invalid dimensions, truncated inline art, allocation failure, or malformed
files reject that decal; loading continues with other decal files. A loaded
decal owns its pattern until successfully inserted into a `WorldState`. On a
failed insertion, ownership remains with the caller; `world_clear()` releases
patterns accepted by the world.

## Lights (`assets/lights/<id>.txt`)

A point light source:

```
x=<world x>
y=<world y>
color=R,G,B,A
intensity=<brightness; negative for anti-light>
radius=<reach in grid cells>
is_god_ray=<0 or 1>
```

Example:

```
x=4.5
y=2.5
color=255,255,255,255
intensity=1.0
radius=4.0
is_god_ray=0
```

## UI elements (`assets/ui_elements/<name>.txt`)

A single data-driven UI widget.  Supported types are `container`, `text`, and
`button`.

```
name=<unique name>
type=<container|text|button>
parent=<parent element name>  # omit for root containers
coords=<absolute|relative>
x=<integer>
y=<integer>
width=<integer>
height=<integer>
align=<left|center|right>     # text/button only
fg=R,G,B,A                    # optional
bg=R,G,B,A                    # optional
action=<action name>           # button only
content=<text>                 # optional; may be empty
```

Containers use `x`/`y` as their absolute screen position; children with
`coords=relative` are offset from the parent.  Example:

```
name=main_menu_start
type=button
parent=main_menu_container
x=2
y=4
coords=relative
width=20
height=1
align=center
action=start_game
content=START GAME
```

## UI layouts (`assets/ui_layouts/<name>.txt`)

A screen composed of a list of element names.  The names reference files in
`assets/ui_elements/`.

```
name=<layout name>
type=layout
elements=<comma-separated element names>
```

Example:

```
name=main_menu
type=layout
elements=main_menu_title,main_menu_start,main_menu_asset_editor,main_menu_quit
```

## Sprites (`assets/sprites/<id>/` — optional)

Sprite patterns are camera-facing decorative billboards rendered by the world
overlay pass. They are light-map-lit, depth-tested against world geometry and
decals, and do not collide, block rays/light, or appear in mirrors. Patterns are
limited to 255 columns and 32 rows. The asset registry owns successfully loaded
patterns and releases them through `asset_registry_clear()`.

Every numeric sprite folder contains `animation.txt` and ordinary pattern frame
files. Each frame uses:

```ini
cols=3
rows=5
default_material=1
pattern_0= @ 
pattern_1=/#\\
material_0=1,1,1
material_1=1,2,1
```

Whitespace/zero glyphs are transparent. Every visible pattern cell requires a
loaded, nonzero material; missing assets and invalid references are safe no-ops.
R11 I1 renders existing `WorldState.sprites`. R11 I2 adds authored
sprite placement, selection, persistence, and undo/redo (code present in commit
`277fb3c`; automated and bundled manual verification passed).
In the unified editor, `P` creates and places an 8×8 static canvas. Select a sprite
and open **Pattern...** to load another numeric sprite folder, save the current
sprite, or paint its selected frame. Pattern edits are staged and do not replace
the live registry asset until Save succeeds. Escape from the painter discards the
staged document.

A static sprite folder has exactly one frame and an `animation.txt` containing the
literal word `static`:

```text
assets/sprites/1/
  animation.txt
  frame_000.txt
```

An animated sprite folder has at least two frames:

```text
assets/sprites/4/
  animation.txt
  wide.txt
  tall.txt
```

`animation.txt` lists frames in playback order and sets the time-based rate:

```ini
fps=4
loop=true
frame=wide.txt
frame=tall.txt
```

`fps` is required and must be finite in `0.1..120`. `loop` is optional and
defaults to `true`. Two to 256 `frame` entries are required. Frame names are
unique local `.txt` basenames and each frame uses the ordinary sprite-pattern
format above. Unknown or duplicate metadata fields, missing/malformed frames,
paths, one-frame animation metadata, and static folders with zero or multiple frame
files leave that sprite ID unloaded. Root-level numeric sprite files are not loaded.
Scene and object instances continue to reference only the numeric sprite ID.
Playback phase is per runtime instance, starts at frame zero, and is not saved.

The Pattern painter authors static and animated folders. Right at the canvas edge
moves focus to the frame menu; Left returns to the canvas. The menu selects frames,
adds immediately after the selected frame, removes the selected frame, and edits
FPS/loop. Neighbor previews wrap circularly. Removing from two frames to one writes
literal-static mode and removes stale animation metadata/files on Save.

## Objects (`assets/objects/<id>.txt` — optional)

Object definitions are numeric, reusable assets. I4 accepts exactly these fields:

```ini
name=training_marker
sprite_id=1
front_direction=0
attributes=simple
```

`simple` objects are static sprite billboards with player collision. Their
direction uses 0=east, π/2=south, π=west, and 3π/2=north. They do not block rays
or light and do not interact with mirrors, triggers, animation, or destruction.

In the unified editor, the object inspector's Sprite row opens a searchable
picker over loaded numeric sprite IDs. Choosing one changes only the selected
object instance through scene command history; it does not mutate the reusable
object definition.

## Native scene v11 triggers and objects

Canonical `.tscene` v11 files may contain repeated trigger blocks after sprite
instances. Identity is the scene-wide stable ID in the block header:

```ini
[trigger 42]
min_x = 2
min_y = 3
max_x = 3
max_y = 4
condition = enter_region
action = set_flag
flag_id = 1
flag_value = 1
```

Actions have exact payloads: `set_flag` requires `flag_id` (`1..64`) and
`flag_value` (`0` or `1`); `teleport_to_spawn` has no payload; `toggle_light`
requires `target_id` resolving to a scene light; and `exit_flow` requires a unique
bounded `flow_port` identifier used by the authored progression graph. A scene may
declare at most 16 flow exits. Regions are finite positive
half-open rectangles contained by the map. Unknown/inapplicable fields, unknown
tokens, duplicate IDs, and dangling targets reject the scene transactionally.
Object instances use `[object <stable-id>]` with `asset_kind = object`, `asset_id`,
`sprite_asset_id`, `position`, and `front_direction`. The sprite field is
per-instance; old v10 records that omit it default to the definition's sprite on
asset-aware load. Versions 1–10 remain readable and migrate forward; canonical
Save writes v11.

## Authored project game flow

`assets/game.flow` is the conventional project progression document opened by `G` in
the unified editor. It uses the strict versioned `FlowDocument` format and references
authored Scene/Menu names, not files under `ui_layouts`. The checked-in baseline contains
only `Start -> Scene:testscene`, because no authored game Menu assets or scene exit ports
are checked in yet. The I10 workspace can browse nodes/connections and rewire existing
edge targets with Ctrl+Z/Y and Ctrl+S; graph node/port construction remains a later
catalog-backed workflow.
