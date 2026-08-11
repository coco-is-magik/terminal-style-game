# Native Scene `.tscene` Version 2 Specification — 2026-08-10

## Status and scope

This is the implemented R4 Increment A persistence contract. Version 2 separates
cell occupancy from one wall, one floor, and one ceiling material reference per
fixed-height X/Y cell. It does not add per-face wall materials, missing planes,
variable heights, slopes, player/camera Z, or independent optical policies.

Unless changed below, v2 retains v1 lexical rules, metadata, resource limits,
lights, decals, identity validation, diagnostics, and transactional load/save rules
from `R2_NATIVE_SCENE_V1_SPEC_2026-07-31.md`.

## Required metadata

`scene_version` is `2`. All other required metadata retains v1 syntax and bounds.
`origin_x` and `origin_y` remain zero. Spawn X/Y must be in bounds and occupy an
authored `empty` cell.

## Required cell grids

Version 2 requires exactly one of each section below and forbids `[cells]`:

```ini
[occupancy]
1 1 1 1
1 0 0 1

[wall_materials]
001 001 001 001
001 009 009 001

[floor_materials]
009 009 009 009
009 009 009 009

[ceiling_materials]
009 009 009 009
009 009 009 009
```

Each section contains exactly `height` rows and `width` tokens. Input permits one or
more spaces or tabs between tokens. Canonical output uses one space.

- Occupancy tokens are exactly `0` (empty) or `1` (wall).
- Material tokens are exactly three decimal digits in `001..255`.
- Material `000` is invalid in v2; it is not occupancy, missing-reference, or
  no-surface state.
- Every cell owns all three material references even when empty or occupied.
- The compatibility runtime `MapCell.material_id` is derived as wall material for
  occupied cells and `0` for empty cells. It is not separately serialized.
- `Map.light_map` remains derived and unsaved.

## Canonical ordering

The writer emits metadata in v1 order, optional provenance, then:

1. `[occupancy]`
2. `[wall_materials]`
3. `[floor_materials]`
4. `[ceiling_materials]`
5. lights sorted by numeric scene ID
6. decal instances sorted by numeric scene ID

Output uses LF, one final LF, canonical numeric formatting, and no comments.

## Native v1 migration

Loading a structurally valid native v1 scene creates a complete v2 candidate before
commit:

- old material `0` becomes empty occupancy;
- old material `1..255` becomes wall occupancy with the same wall material;
- an empty cell receives the captured configured default material as its latent wall
  material;
- every floor and ceiling receives that captured default material.

The default is captured once at the load boundary and must be `1..255`. Migration is
checked-size, allocation-fallible, and transactional. Failure preserves the live
document. A migrated native v1 document retains its native path but is dirty with
`migration_pending`; successful v2 Save clears that state. Loading never rewrites the
source automatically.

## Legacy digit-grid migration

Explicit legacy import retains its existing non-destructive provenance and Save As
workflow. The legacy grid is interpreted through the established digit/default rules,
then migrated using the same v1-to-v2 mapping. The legacy source is never overwritten.

The deprecated legacy-current compatibility loader also derives complete authored v2
cells so existing editor workflows can save canonically without synthesizing authored
state inside the serializer.

## Missing material references

When an `AssetRegistry` is available, every wall, floor, and ceiling material
reference—including latent wall materials on empty cells—is resolved.

An unresolved material:

- preserves its exact authored numeric reference;
- emits bounded `TSG-SCENE-INPUT-0011` context naming cell X/Y and surface field;
- commits in repair mode;
- blocks normal Save with `TSG-SCENE-INPUT-0012`;
- may be replaced only by an explicit loaded material through the typed surface
  repair boundary.

Repair does not substitute defaults or mutate the registry. Wall repair synchronizes
the derived compatibility map only when the cell is occupied. Diagnostic collection
remains bounded by `SCENE_MAX_REPAIR_DIAGNOSTICS` across decal and surface references.

## Transaction and ownership guarantees

- `SceneFormatCandidate` owns parse/migration arrays until a successful commit.
- `SceneDocument` then becomes the sole owner of `SceneAuthoredCell[]`.
- The compatibility `Map`, runtime `WorldState`, resolved assets, light maps, and
  renderer caches remain derived or borrowed, never duplicate authored truth.
- Parse, migration, path allocation, repair-diagnostic allocation, and validation
  complete before replacing the live document.
- Failed parse/migration/load/save preserves the prior live document and destination
  according to the v1 durable transaction contract.

## Verification

Focused format tests cover exact canonical bytes, parse/serialize/parse equality,
missing/duplicate/malformed grids, invalid occupancy/materials, exact migration, and
deterministic migration OOM rollback. Document tests cover v1 dirty migration, v2
save/reopen, legacy defaults/provenance, surface repair/Save blocking, derived map and
light-map behavior, and failed-load preservation.
