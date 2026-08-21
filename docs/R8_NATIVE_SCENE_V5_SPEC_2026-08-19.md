# Native Scene `.tscene` Version 5 Specification — 2026-08-19

## Status and scope

Version 5 is the R8 heightfield format. This unreleased version is amended by the
2026-08-20 heightfield remediation. It retains every v4 contract and adds:

1. Per-cell `floor_heights` and `ceiling_heights` grids (fixed-point ⅟₂₅₆ world
   unit steps).
2. Per-cell `floor_presence` and `ceiling_presence` grids.
3. Per-cell gravity overrides: `gravity_scales` and `gravity_orientations`.
4. A per-map `[movement]` parameter block.

v5 does **not** add:

- Per-face wall materials (still reserved for a future appended block).
- Sprite instances.
- True angular pitch, stacked traversable intervals, or arbitrary inclined
  planes (R1/R8 decisions).

Native v1, v2, v3, and v4 remain readable migration inputs. Canonical editor
Save writes v5. Loading v1–v4 is migration-pending and dirty until a successful
v5 Save. Loading never rewrites the source file.

The maximum accepted scene source/canonical output grows from 2 MiB to **8 MiB**
so a canonical v5 scene at the existing 512×256 map limit remains representable.
The parser/writer retain checked sizes and transactional allocation failure.

Decisions are authoritative in `R8_DECISION_RECORD_2026-08-19.md`; the exact
limits below come from this spec and the R8 requirements/implementation plan.

## Version metadata

`scene_version` is `5`.

`scene_version = 5` must appear in the metadata header before any grid section.

## World-unit convention

- One cell is one horizontal world unit (R1). Vertical values use the same
  world unit; the default room spans `floor_h = 0` to `ceiling_h = 1`.
- Per-cell heights are fixed-point steps of ⅟₂₅₆ world unit, encoded with the
  existing 4-hex block grammar.
- Movement/gravity parameters are scalar double values serialized with the
  existing `%.17g` writer convention (deterministic round-trip).

## Height token grammar

Each height value is one **block** of exactly four uppercase hex digits:

```
XXXX        e.g.  F800 = -8.0   FF80 = -0.5   0000 = 0.0   0100 = 1.0
```

- Tokens encode signed `int16_t` two's-complement fixed point.
- Valid height range: `F800`..`0800` when interpreted as signed
  (`-8.0..+8.0` world units).
- Values outside that signed range are rejected transactionally.
- `0000` is a valid height here (floor at 0); the block-codec null meaning is
  material-specific and does not apply to heights.

## Cell sections

v5 canonical requires exactly one of each section in this fixed order:

```ini
[occupancy]
[wall_materials]
[floor_materials]
[ceiling_materials]
[floor_heights]
[ceiling_heights]
[floor_presence]
[ceiling_presence]
[gravity_scales]
[gravity_orientations]
```

Row count and column count must match `width` and `height` exactly in every
grid. `[cells]` remains forbidden.

### floor_heights / ceiling_heights

4-hex height blocks as above. Default migrated floor = `0000`, ceiling = `0100`.

### floor_presence / ceiling_presence

Per-cell decimal tokens `0` (surface removed) or `1` (surface present):

```
[floor_presence]
0 0 1 0
1 1 1 1
```

Migration from v1–v4 initializes both grids to `1`. A removed floor is an opening
downward to darkness; a removed ceiling is an opening upward. An opening is
terminal and does not expose a stacked traversable interval. Physics rejects entry
to a cell missing either finite surface.

### gravity_scales

Per-cell fixed-point multipliers of the map-wide gravity magnitude, inherited
from the map by default:

- `0000` — inherit map-wide gravity magnitude for this cell.
- `0001`..`0100` .. `FFFF` — effective magnitude = map magnitude × value/256
  (bounded by `SCENE_MAX_GRAVITY_MAGNITUDE` at validation time).

### gravity_orientations

Per-cell direction overrides; `0` inherits the map-wide orientation:

| Token | Meaning |
|---|---|
| `0` | inherit map default |
| `1` | down (normal gravity) |
| `2` | up |
| `3` | north (−Y) |
| `4` | south (+Y) |
| `5` | east (+X) |
| `6` | west (−X) |

Effective gravity at a cell = map magnitude × scale, along the override
orientation (or the map orientation when inherited). Gravity applies to
airborne bodies; grounded bodies rest on `floor_h`; the camera never rotates
with gravity (R8 Decision 5).
## Movement-parameter block

One `[movement]` section is required, placed immediately after the growth
metadata and before the first grid section. Keys are lowercase, one per line,
`key = value`, doubles serialized with `%.17g`:

```ini
[movement]
gravity_magnitude = 9.8
gravity_orientation = down
step_height = 0.25
jump_impulse = 3.2
air_control_scale = 1.0
eye_height = 0.5
head_clearance = 0.75
```

### Parameter table

| Parameter | Type | Default | Range | Meaning |
|---|---|---|---|---|
| `gravity_magnitude` | double | 9.8 | `(0, 256]`, finite | Map-wide gravitational acceleration magnitude (world-units/s²) |
| `gravity_orientation` | enum | down | down, up, north, south, east, west | Map-wide gravity direction |
| `step_height` | double | 0.25 | `[0.0625, 1]` | Max floor delta auto-stepped without a jump |
| `jump_impulse` | double | 3.2 | `(0, 16]` | Launch speed applied opposite the effective gravity vector; default apex is about 0.52 world units at normal gravity |
| `air_control_scale` | double | 1.0 | `[0, 1]` | Horizontal control factor while airborne |
| `eye_height` | double | 0.5 | `(0, 8]`, finite | Eye height above `floor_h` used by the horizon view |
| `head_clearance` | double | 0.75 | `[SCENE_MIN_CLEARANCE, 8]` | Runtime minimum standing room; must be ≥ the fixed structural minimum |

`SCENE_MIN_CLEARANCE` is the fixed, config-independent structural constant
(guardrail G2). The authored `head_clearance` parameter may only tighten the
runtime requirement above that constant; it can never lower it.

### Structural constants (fixed, not serialized)

| Constant | Value | Meaning |
|---|---|---|
| `SCENE_MIN_CLEARANCE` | 0.25 world units | Config-independent minimum interval; validation floor (G2) |
| `HEIGHT_STEP` | ⅟₂₅₆ world unit | Fixed-point height resolution |
| `SCENE_HEIGHT_MIN` / `SCENE_HEIGHT_MAX` | -8.0 / 8.0 world units | Signed two's-complement tokens `F800`..`0800` |
| Default `floor_h` | 0.0 (`0x0000`) | Migration default; today's fixed floor plane |
| Default `ceiling_h` | 1.0 (`0x0100`) | Migration default; today's fixed ceiling plane |
| Effective-gravity bound | magnitude ≤ 256 | Validated on map and per-cell overrides |

### Movement-block occupancy

All seven keys are required in canonical v5 output. Older scenes migrating to
v5 supply default values (the table above) exactly.

## Semantics

### Occupancy and heights

- `occupancy = 1` (wall): `wall_material` must be non-null; `floor_h`/`ceiling_h`
  still bound the cell's interval (used when the wall is visible at a column).
- `occupancy = 0` (empty): when both surfaces are present, the interval
  `[floor_h, ceiling_h]` is the cell's traversable space and `floor_h < ceiling_h`
  is mandatory. A missing surface makes the cell a terminal opening, not another
  traversable interval.
- A wall cell with `floor_h < ceiling_h` is a vertical span; heights are derived
  only from authored values — no implicit per-cell height conversions.

### References and repair

None of the v5 additions (height grids, presence grids, gravity overrides, or the
`[movement]` block) introduces an asset reference. The v4 missing-reference
detection and repair machinery is therefore unaffected: repair-required state
still originates only from unresolved material or decal-pattern references.

### Surface openings

`floor_presence = 0` removes the finite floor and opens downward to darkness.
`ceiling_presence = 0` removes the finite ceiling and opens upward. Rendering
terminates at the opening, and physics rejects traversal into the cell. These
flags never create stacked or hidden intervals (guardrail G1).

### Gravity overrides

Effective per-cell gravity is map magnitude × `gravity_scale`, along the
override orientation or the map orientation. `gravity_scale = 0x0000` inherits
the map magnitude; `gravity_orientation = 0` inherits the map orientation.
Gravity affects airborne bodies; grounded bodies rest on `floor_h`.

## Validation

- Height tokens outside signed `-0x0800..0x0800` reject transactionally; values not
  full 4-hex blocks reject at lexical parse (block codec).
- When both surfaces are present, `floor_h ≥ ceiling_h`, or clearance below
  `SCENE_MIN_CLEARANCE`, rejects at scene validation and activates
  `TSG-SCENE-INPUT-0014`.
- Movement-block failures: unknown key, duplicate key, missing required key,
  non-finite value, or out-of-range value rejects the candidate transactionally.
  `head_clearance` below `SCENE_MIN_CLEARANCE` is rejected (G2).
- Presence tokens outside decimal `0` or `1` are rejected.
- `gravity_orientations` outside `0..6` and `gravity_scales` outside the
  magnitude bounds reject.
- All rejections preserve the live document and produce the exact catalog
  identifier with bounded context (line, grid, key, value, required range).

## Migration from v1, v2, v3, and v4

The existing pending/repair machinery migrates older versions in order, then
adds the v5 fields with defaults:

```text
v1 ──► v2 ──► v3 ──► v4 ──► v5
       ▲      ▲      ▲      │
       │      │      │      └── canonical save
       │      │      └───────── migration input (repair if needed)
       │      └───────────────── migration input (repair if needed)
       └──────────────────────── migration input (repair if needed)
```

v4 → v5 additions with defaults: `floor_heights = 0x0000`,
`ceiling_heights = 0x0100` (matching today's fixed planes), every
`floor_presence` and `ceiling_presence` = 1, `gravity_scales` = `0x0000`,
`gravity_orientations` = `0`, and the movement
block as in the parameter table. A migrated scene is dirty until a v5 Save.

## Canonical ordering

v5 canonical save ordering, fixed:

1. Metadata: `scene_type`, `scene_version = 5`, `name`, `width`, `height`,
   `origin_x = 0`, `origin_y = 0`, `next_instance_id`, `ambient_intensity`,
   `spawn`, growth metadata (`east_growth`, `south_growth`), optional
   `legacy_source_path`.
2. `[movement]` block (all seven keys, table order).
3. The ten cell grids in this order: `occupancy`, `wall_materials`,
   `floor_materials`, `ceiling_materials`, `floor_heights`, `ceiling_heights`,
   `floor_presence`, `ceiling_presence`, `gravity_scales`,
   `gravity_orientations`.
4. `[light <id>]` sections (existing v4 grammar).
5. `[decal <id>]` sections (existing v4 grammar).
6. LF line ending and one final LF (unchanged).

## Error catalog

Fields are activated as they are implementation-entered in the owning module.
Designed with R8 increments:

| Diagnostic | Condition |
|---|---|
| `TSG-SCENE-INPUT-0014` | Invalid clearance or floor/ceiling relation in v5 heights (activate) |
| `TSG-SCENE-INPUT-0015` | Height step token outside signed `-0x0800..0x0800` |
| `TSG-SCENE-INPUT-0016` | `[movement]` block malformed: unknown/duplicate/missing key, non-finite, or out-of-range value |
| `TSG-SCENE-INPUT-0017` | Reserved to preserve stable diagnostic numbering after ladder deferral |
| `TSG-SCENE-INPUT-0018` | Gravity override out of bounds (scale value, orientation token) |

Each entry is filled into `ERROR_CATALOG.md` (Planned → Active) in the increment
that emits it, with exact owner, detection, context, recovery, preserved state,
and a focused test.

## References

- `R8_DECISION_RECORD_2026-08-19.md`
- `R8_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-19.md`
- `R5_NATIVE_SCENE_V4_SPEC_2026-08-12.md`
- `R4_NATIVE_SCENE_V3_SPEC_2026-08-12.md`
- `C_STYLE_AND_OWNERSHIP.md` (diagnostics and state guarantees)