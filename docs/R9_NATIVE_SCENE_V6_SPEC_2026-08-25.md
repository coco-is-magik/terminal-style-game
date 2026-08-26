# Native Scene `.tscene` Version 6 Specification — 2026-08-25

## Status and scope

Version 6 is the R9 optical-authorship format. It retains every v5 contract and
adds independent typed optical material defaults plus sparse per-cell overrides.
It adds no optical editor controls, presets, colored lighting, or entities.
Mirrors are a runtime optical-rendering effect enabled by positive wall
reflectivity; they are not a separate schema block.

Native v1–v5 remain accepted migration inputs. Loading v1–v5 derives exact legacy
optical behavior without allocating or persisting synthetic override records and
marks the document migration-pending. Canonical document Save writes v6; explicit
format serialization of a v1–v5 candidate retains that version's exact bytes.

## Authored model

Resolution order is:

1. legacy behavior derived from occupancy/hit blocking;
2. optional material default;
3. optional sparse cell override.

The six independent properties are:

- `player_blocks`: decimal `0` or `1`;
- `ray_blocks`: decimal `0` or `1`;
- `light_blocks`: decimal `0` or `1`;
- `opacity`: decimal `0`..`255`;
- `transmission`: decimal `0`..`255`;
- `reflectivity`: decimal `0`..`255`.

Property presence is the override bit. Omitted properties inherit. A block must
contain at least one property; empty blocks are rejected.

## Blocks

Material defaults use a material ID in `0..65535`:

```ini
[optical_material 12]
ray_blocks = 0
opacity = 96
transmission = 180
```

Cell overrides use a zero-based row-major flat cell index
`index = y * width + x`, bounded by `width * height`:

```ini
[optical_cell 42]
player_blocks = 0
reflectivity = 64
```

Input blocks and fields may be reordered. Duplicate material IDs, duplicate cell
indices, duplicate fields, unknown fields/blocks, empty blocks, malformed values,
and out-of-range IDs/indices reject the complete candidate transactionally.

## Canonical ordering

All unchanged v5 metadata, movement, grids, lights, and decals retain their v5
canonical order and encoding. Optical blocks follow decals:

1. nonempty `optical_material` blocks in ascending material ID;
2. `optical_cell` blocks in ascending flat cell index;
3. fields inside each block in this order: `player_blocks`, `ray_blocks`,
   `light_blocks`, `opacity`, `transmission`, `reflectivity`.

Material array slots with no properties are omitted. Sparse records may not have
an empty mask. Parsing normalizes sparse records into strictly increasing order;
serialization is deterministic and parse→serialize→parse→serialize byte-equal.

## Ownership and runtime view

`SceneFormatCandidate` and `SceneDocument` own the material-extension and sparse
override arrays. Commit transfers both arrays transactionally. The runtime view
borrows them, validates all masks/values/order/bounds, and carries a source
generation. Ownership-changing resize/load operations invalidate old views.

East/south copy-growth duplicates a source cell's sparse override onto the copied
target cell. Shrink treats an override in the removed ring as authored content and
refuses the shrink. Remapping and cell/map allocation commit atomically.

The application uses optical rendering only when a document produces a validated,
nonempty v6 optical view. Documents with absent optical blocks continue through
`raycast_render_height()` and retain exact legacy rendering/collision/checksums.

## Mirror reflection

Positive `reflectivity` on a wall enables one-bounce mirror reflection in the
optical renderer:

- Only vertical walls (x-normal or y-normal) reflect; floors and ceilings do not;
- The reflected view is computed from the mirror hit point using the existing
  selective heightfield tracer and is limited to a single bounce;
- A reflected miss, proven opening, layer-cap exhaustion, or a second mirror hit
  produces darkness before mixing with the direct mirror appearance;
- Reflected entities, sprites, and UI overlays are not rendered into the
  reflected view;
- Mixing is `((255 - reflectivity) * direct + reflectivity * reflected) / 255`
  per color channel; the reflected glyph replaces the direct glyph when
  `reflectivity >= 128`.

Reflectivity remains independently authored; it does not affect collision, sight
blocking, light blocking, opacity, or transmission unless explicitly overridden.

## Editor authoring

I6 edits complete material or cell extensions through command history. Each field
may be explicitly authored or returned to `inherit`; clearing the final field
removes the logical block. Runtime collision resolves `player_blocks`, selective
sight resolves `ray_blocks`, scalar-light shadowing resolves `light_blocks`, and
composition resolves opacity/transmission. Reflectivity remains persisted but has
no runtime effect until separately gated mirror work.