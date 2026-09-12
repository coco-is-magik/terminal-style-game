# Native Scene `.tscene` Version 1 Specification — 2026-07-31

## Status and scope

This is the approved R2 persistence contract. Version 1 is a compatibility
foundation for current map, lighting, spawn, and decal rendering. It intentionally
does not represent future height-aware geometry or independent R4/R8 surfaces.

## Resource limits

| Resource | Version 1 limit |
|---|---:|
| File bytes | 2 MiB |
| Physical line bytes, excluding terminator | 4096 |
| Scene basename and display name | 1–64 ASCII characters |
| Accepted scene-name characters | `A-Z`, `a-z`, `0-9`, `_`, `-` |
| Path or provenance bytes | 1024 |
| Width / height | 1–512 / 1–256 |
| Lights / decal instances | 64 / 256 |
| Repair diagnostics | 256 |
| Decal pattern asset ID | 1–255 |
| Scene instance ID | decimal `uint64_t`; `0` invalid |

All products and byte additions are checked before allocation.

## Lexical rules

- Input is byte-oriented text with LF or CRLF line endings. Canonical output is LF.
- Blank lines are allowed.
- A line whose first non-whitespace byte is `#` is a comment. Inline comments are
  not accepted, so quoted values and data cannot be truncated ambiguously.
- Properties use `key = value`. Section headers use `[kind id]` or `[cells]`.
- Keys and section kinds are lowercase ASCII and case-sensitive.
- Quoted strings accept only `\\` and `\"` escapes. Other escapes, controls, and
  unterminated strings reject.
- Decimal integers have no sign unless the field permits negative values. A strict
  decimal float may contain a sign, integer and/or fraction, and decimal exponent.
  The complete token must be consumed and the result must be finite and in range.
- Float conversion is independent of the process locale. Canonical output uses the
  C numeric locale and sufficient significant decimal digits (`%.17g`) to round-trip
  binary `double`; negative zero is written as `0`.
- Readers permit known fields and instance sections in any order. Unknown and
  duplicate current-version fields or sections reject.

## Required metadata

```ini
scene_type = terminal_scene
scene_version = 1
name = "room_1"
width = 10
height = 6
origin_x = 0
origin_y = 0
next_instance_id = 3
ambient_intensity = 0.20000000000000001
spawn = 1.5,1.5,0
legacy_source_path = "assets/maps/1.txt"
```

All properties except `legacy_source_path` are required exactly once. In v1,
`origin_x` and `origin_y` must both be zero. The explicit fields reserve a future
migration seam without adding origin behavior now. Ambient intensity is finite in
`[0,1]`. Spawn X/Y must be finite, in map bounds, and in a passable cell. Spawn angle
is finite and is preserved as radians; no player/camera Z is stored.

`legacy_source_path` is optional informational provenance. It is never a destination
and never changes import or Save behavior.

## Cells

```ini
[cells]
001 001 001 001
001 000 000 001
001 001 001 001
```

There are exactly `height` rows, each containing exactly `width` three-digit decimal
material IDs separated by one or more spaces. Values are `000` through `255`.
Canonical output uses one space. The representation avoids the legacy digit limit
without introducing surface records. The map allocation owns authored cells;
runtime `light_map` data is never serialized.

## Lights

```ini
[light 1]
position = 4.5,2.5
color = 255,255,255,255
intensity = 1
radius = 4
```

Every light section ID is a nonzero scene instance ID. Position is finite and within
map bounds. RGBA channels are decimal integers `0..255`. Intensity is finite and may
be positive, zero, or negative to preserve anti-light behavior. Radius is finite and
strictly positive. No separate `anti_light` flag is stored because sign already has
accepted runtime meaning.

## Decal instances

Every decal stores a typed reusable reference and current compatibility placement:

```ini
[decal_instance 2]
asset_kind = decal_pattern
asset_id = 6
surface = wall
anchor = 2,2,0
uv = 0.2,0.4
size = 0.6,0.2
glyph_step = 0,0
depth = 0.1
rotation = 0
```

Floor and ceiling instances replace `anchor` and `uv` with:

```ini
surface = ceiling
position = 7,1.5,0
```

`asset_kind` must be exactly `decal_pattern` in v1 and `asset_id` is `1..255`.
Wall anchors contain map X, map Y, and the current side value `0` or `1`; UV values
are finite in `[0,1]`. Floor/ceiling positions are finite and X/Y are in map bounds.
Size components are finite and positive. Glyph steps and depth are finite and
nonnegative. Rotation is a finite radian value. Surface-inapplicable fields reject
rather than being ignored.

These fields populate the current `Decal` runtime representation. They do not define
new tangent bases, wall faces, height-aware anchors, or projection semantics.

## Identity validation

Light and decal IDs share one namespace. IDs must be nonzero and unique across all
instance kinds. `next_instance_id` must be nonzero and greater than every persisted
instance ID. The largest allocatable instance ID is `UINT64_MAX - 1`;
`next_instance_id = UINT64_MAX` is a persisted exhausted sentinel and is never
allocated. Creation then fails without mutation. Deletion, undo, redo-branch
truncation, save, and reload never make a consumed ID available to another logical
instance.

## Missing references

A structurally valid decal whose pattern asset is absent commits in repair mode:

- retain `{decal_pattern, asset_id}` unchanged;
- append `TSG-SCENE-INPUT-0011` with bounded context;
- render the reserved conspicuous fallback through the derived runtime view;
- reject normal Save with `TSG-SCENE-INPUT-0012` until explicit repair.

The fallback is never serialized and never substituted into authored state.

## Canonical ordering

The writer emits: metadata in the order shown above; optional provenance; one blank
line; `[cells]`; lights sorted by numeric ID; decals sorted by numeric ID. Within
each section, fields use the order shown here. Output uses fixed spaces around `=`,
LF endings, no comments, and exactly one final LF.

## Format discrimination and rejection

Native detection requires the parsed `scene_type` and `scene_version`, not just the
extension. Unsupported type/version, syntax, absent/duplicate/unknown fields,
dimensions, arithmetic, numeric values, IDs, or structural references reject the
candidate transactionally. Only unresolved reusable asset references enter repair
mode.
