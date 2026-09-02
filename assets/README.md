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

## Sprites (`assets/sprites/<id>.txt` — optional)

Sprite patterns are camera-facing decorative billboards rendered by the world
overlay pass. They are light-map-lit, depth-tested against world geometry and
decals, and do not collide, block rays/light, or appear in mirrors. Patterns are
limited to 255 columns and 32 rows. The asset registry owns successfully loaded
patterns and releases them through `asset_registry_clear()`.

Each numeric file uses:

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
I1 renders existing `WorldState.sprites` only. Scene placement, selection, and
persistence arrive in R11 I2.
