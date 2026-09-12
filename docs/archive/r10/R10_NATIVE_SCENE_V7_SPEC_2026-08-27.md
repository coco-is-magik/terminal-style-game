# R10 Native Scene v7 Specification — Spot Lights — 2026-08-27

## Status and compatibility

Scene v7 is the canonical native writer format for R10 I2. Parsers continue to
accept v1–v6. Loading v6 migrates every existing light to an exact point-light
default and marks the document migration-pending; the next successful native save
writes v7. Strict unknown, duplicate, required-field, numeric, and ordering rules
remain in force.

## Light block grammar

Every v7 `[light ID]` block requires exactly these fields:

```ini
[light 11]
position = 2.5,2.5
color = 255,128,64,255
intensity = 1
radius = 5
type = spot
direction = 0
cone = 1.5707963267948966
falloff = 1
```

- `type`: `point` or `spot`.
- `direction`: radians in `[0, 2π)`; zero points east (`+X`), increasing toward
  south (`+Y`) in map coordinates, matching `atan2(dy, dx)`.
- `cone`: full angular width in radians, `[π/180, 2π]`.
- `falloff`: radial exponent in `[0.1, 8]`.

Point lights carry the same explicit fields. Migration defaults are `type=point`,
`direction=0`, `cone=2π`, `falloff=1`; these preserve pre-v7 behavior exactly.

## Runtime semantics

For a tile center at distance `d <= radius`, radial attenuation is:

```text
pow(1 - d/radius, falloff)
```

Point lights accept every angle. Spot lights accept a tile when the normalized
signed angle difference from `direction` is at most `cone/2`; cone boundaries are
inclusive. Existing shadow/bounce and RGB/A weighting apply after this test.

New editor-created lights begin as points. Changing Type from Point to Spot gives
an inherited `2π` cone a useful 90-degree default; authored narrower cones survive
later Point/Spot toggles. `config.ini` `light_falloff_default` supplies the radial
exponent for newly placed lights and is validated against the same `[0.1,8]` range.