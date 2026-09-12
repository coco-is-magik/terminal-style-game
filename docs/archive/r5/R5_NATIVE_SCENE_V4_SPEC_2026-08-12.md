# Native Scene `.tscene` Version 4 Specification — 2026-08-12

## Status and scope

Version 4 is the R5 capacity-widening format. It replaces the 3-digit decimal
per-cell material tokens with 4-hex-digit fixed-width blocks, widening material
IDs from `uint8_t` (1..255) to `uint16_t` (1..65 535).

v4 retains all v3 contracts:
- Cell occupancy separated from surface materials
- One wall, one floor, and one ceiling material per cell
- Lights, decals, spawn, identity validation, diagnostics
- Canonical ordering and transactional load / save
- Growth metadata (`east_growth`, `south_growth`)

v4 does **not** add:
- Per-face wall materials (reserved for a future appended block)
- Sprite instances (sprite feature not yet implemented)
- Variable heights, slopes, or Z geometry

Native v1, v2, and v3 remain readable migration inputs. Canonical editor Save
writes v4. Loading v1 / v2 / v3 is migration-pending and dirty until a
successful v4 Save. Loading never rewrites the source file.

## Version metadata

`scene_version` is `4`.

`scene_version = 4` must appear in the metadata header before any grid section.

## Token grammar

Each material reference is a single **block** of exactly four uppercase
hexadecimal digits:

```
XXXX
```

where `X` is `0-9` or `A-F`.

- `0000` — null reference. The cell surface has no material assigned.
- `0001`..`FFFF` — valid material ID (1..65 535).

Blocks are space-separated within a row. Row count and column count must match
`width` and `height` exactly.

## Cell grids

v4 requires exactly one of each section below. `[cells]` remains forbidden.

```ini
[occupancy]
1 1 1 1
1 0 0 1

[wall_materials]
0001 0001 0001 0001
0001 0000 0000 0001

[floor_materials]
0002 0002 0002 0002
0002 0002 0002 0002

[ceiling_materials]
0003 0003 0003 0003
0003 0003 0003 0003
```

### Semantics by occupancy

- `occupancy = 1` (wall): `wall_material` must be non-null (`0001`..`FFFF`).
  `floor_material` and `ceiling_material` may be null.
- `occupancy = 0` (empty): `wall_material` may be null or non-null.
  A non-null `wall_material` on an empty cell is a **latent material** —
  it will be used if the cell is later converted to a wall.
  `floor_material` and `ceiling_material` apply to the floor / ceiling
  planes regardless of wall occupancy.

### Latent materials and the picker shortlist

The editor material picker shortlist includes latent wall materials
(materials assigned to empty-occupancy cells) so that converting an empty
cell to a wall does not require a search.

## Block-codec module

A new module `src/scene_block_codec.{h,c}` owns all token-grammar rules.

Intended public interface:

```c
bool block_parse(const char *text, uint16_t *out_value);
bool block_format(uint16_t value, char out_text[5]);
bool block_is_null(uint16_t value);
size_t block_count(const char *row_text);
```

- `block_parse` accepts exactly four hex digits, rejects lowercase, rejects
  non-hex characters, rejects overflow.
- `block_format` writes uppercase 4-hex plus a null terminator.
- `block_is_null` is true for `0x0000`.
- `block_count` returns the number of valid blocks in a row string.

`scene_format.c` delegates block lexical parsing to this module. Version-specific
field assembly remains in `scene_format.c`: v4 requires exactly its known grids
and row cardinality. A future parser may read an older version by supplying
defaults for fields introduced later; a v4 parser does not silently accept
unknown future fields.

## Reserved-block policy

No reserved blocks are defined in v4.

A future version that adds per-face wall materials will append one block per
face, producing seven blocks per cell:

```
WWWW FFFF CCCC NNNN SSSS EEEE WWWW
```

(The new blocks are north, south, east, and west wall materials.)

A future-version parser reading v4 supplies defaults for its newly introduced
fields. Migration rewrites the file to that version's canonical form using the
existing pending / repair machinery. A v4 parser rejects future-version input.

## Migration from v1, v2, and v3

v1 and v2 are first migrated to v3 (growth metadata) using existing machinery,
then v3→v4 rewrites material tokens:

- Each 3-digit decimal token `NNN` becomes 4-hex `0NNN`.
  Example: `001` → `0001`, `255` → `00FF`.
- The v3 missing-material repair pass validates that every non-null token
  resolves to a loaded material after migration.

The migration path is:

```
v1 ──► v2 ──► v3 ──► v4
       ▲      ▲      │
       │      │      └── canonical save
       │      └───────── migration input (repair if needed)
       └──────────────── migration input (repair if needed)
```

## Validation

- `parse_uint_range` caps for material IDs: `> 65535U` rejected.
- `material_id_is_loaded` range: `1..65535`.
- `first_free_material_id` range: `1..65535`.
- `SceneAuthoredCell` material fields: `uint16_t`.
- Default material / palette IDs in `config.c` and `scene_document.c`:
  `1..65535`.

## Canonical ordering

v4 canonical save ordering matches v3 exactly, with `scene_version=4`
and 4-hex blocks in the three material grids.

## Error catalog

| Diagnostic | Condition |
|---|---|
| `SCENE_DIAGNOSTIC_INPUT_SYNTAX` | Malformed 4-hex block (wrong length, bad character) |
| `SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING` | Non-null block references an unloaded material |
| `SCENE_DIAGNOSTIC_INPUT_DIMENSIONS` | Grid row / column count mismatches `width` / `height` |

## References

- `R2_NATIVE_SCENE_V1_SPEC_2026-07-31.md`
- `R4_NATIVE_SCENE_V2_SPEC_2026-08-10.md`
- `R4_NATIVE_SCENE_V3_SPEC_2026-08-12.md`
- `R5_ASSET_IDENTITY_DECISION_RECORD_2026-08-12.md`
