# Native Scene `.tscene` Version 3 Specification — 2026-08-12

## Scope

Version 3 is the R4 manual-review follow-up format. It retains the complete v2 cell,
light, decal, identity, diagnostic, and canonical ordering contracts and adds only
persisted provenance for automatic east/south map growth.

Native v1 and v2 remain readable migration inputs. Canonical editor Save writes v3.
Loading v1 or v2 is migration-pending and dirty until a successful v3 Save. Loading
never rewrites the source.

## Required growth metadata

Version 3 requires exactly one of each metadata field after `spawn`:

```ini
east_growth = -
south_growth = -
```

`-` means no automatic growth. Otherwise the value is a comma-separated ordered
list of zero-based trigger coordinates:

```ini
east_growth = 2,4
south_growth = 3
```

- East entries are trigger rows and must be in `[0,height)`.
- South entries are trigger columns and must be in `[0,width)`.
- Order is creation order; only the final matching entry may shrink.
- Counts are bounded by `SCENE_MAX_WIDTH` / `SCENE_MAX_HEIGHT`.
- Empty tokens, trailing commas, negative/out-of-range values, duplicates of the
  metadata field, and growth metadata in v1/v2 are rejected transactionally.

## Growth semantics

Removing an east/south boundary wall copies the complete old edge row/column outward
cell-for-cell, records the selected trigger, then empties the selected old-edge cell.
West/north removal remains unavailable. Refilling the current outermost trigger places
the wall and removes the copied ring only when the ring still matches its source and
contains no light or decal. Edited/occupied rings remain expanded.

All resizing is one command-history transaction. Undo/redo restores dimensions,
authored cells, compatibility map, light map, provenance, and cascaded wall decals.
`origin_x`/`origin_y` remain zero; existing coordinates do not shift.

## Canonical ordering

Metadata retains v2 order, with `east_growth` then `south_growth` immediately after
`spawn`, followed by optional legacy provenance, the four authored grids, lights, and
decals. Output uses LF and one final LF.
