# R6 Decision Record — Decal Placement and Point-Light Authoring — 2026-08-13

## Authority

This record captures the product and architecture decisions made in the 2026-08-13
design thread before R6 implementation begins. Decisions are binding for all R6
increments and are grounded in existing code. The authoritative requirements and
increment plan is `R6_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-13.md`.

## Scope of R6

R6 is "Decal placement and point-light authoring" from `docs/FEATURE_ROADMAP.md`.
Two important corrections to the original TODO phrasing were grounded in code
before planning:

- `SceneDocument` **already owns** `lights[]`, `decals[]`, spawn, ambient, and
  `next_instance_id`.
- `scene_format` **already serializes and round-trips** the full light and decal
  model, including surface-projected decal fields.

So R6 is a **create/place/delete authoring layer**, not a new ownership or format
layer. See decision 5 below.

## 1. Increment ordering

**Decision:** Two increments — **lights first (I1), decals second (I2)**.

**Rationale:** Point lights already have selection and inspector edit UI
(position/RGBA/intensity/radius stepped through the wall-material undo/redo
history) and are already serialized. The only missing gap is creation and
deletion, which is small. Decal placement is surface-projected and carries more
new surface math and a richer placement flow, so it benefits from coming after
the smaller, already-partly-built light increment.

## 2. Light anchor model and placement

**Decision:** A new point light is placed **centered in the hovered cell**. When
the hover is a wall face, the light is placed centered in the **adjacent** cell
on that face's side. No dedicated placement mode and no perpendicular face-offset
offset anchor.

**Rationale:** The user expects the light to appear where they hover or selected,
and the existing tools already refine position afterward. `SceneLight` is a float
`(x, y)` position and floats live in `light_map`, so "centered in a cell" is a
free `.5, .5` center with no new positioning machinery.

**Accepted tradeoff:** a wall-adjacent light sits at the adjacent cell center, not
at an arbitrary perpendicular distance; mid-room free placement is achieved by
editing afterward, not by the placement gesture.

**Rejected alternatives:**
- Perpendicular face-offset anchor: extra math and a less predictable placement
  for no benefit given free position editing exists.
- Cell-snapped with no fractional center: conflicts with the existing fractional
  `SceneLight` model and free position editing.

## 3. Light interaction flow

**Decision:** Reuse the first-person crosshair/hover + Enter vocabulary, no
dedicated mode. Hover a cell → a new-light action → Enter drives placement into a
fresh inspector.

**Deletion:** mirror the existing **wall-removal confirm** flow. Select the light,
a **"remove" menu item** in the inspector asks to confirm, then one undoable
command removes it.

**Required command additions (do not exist today):** `scene_document_internal`
has `insert_decal`/`remove_decal` but **no insert/remove light**; `command_system`
has only `EDITOR_MUTATION_SET_LIGHT`. I1 must add insert-light and remove-light
command mutations.

**Plan details (not open decisions):**
- Light capacity is `SCENE_MAX_LIGHTS = 64`; insertion at full capacity rejects
  and reports, never silently drops.
- Face offset is replaced by the adjacent-cell center; the light `(x, y)` start is
  the neighboring cell center.

## 4. Decal model and placement (placement-first)

**Decision:** Decal placement is treated as **placing a straight-up canvas** on a
selected wall/floor/ceiling surface via the **surface submenu**. The flow sets
initial dimensions and defaults to a decal oriented straight up. Dimensions,
orientation, position, and size are edited after the fact with the existing
inspector tools. Pattern content is authored in its intended orientation, so
placement does not worry about the particulars of size/orientation.

**Selection:** to select an existing decal, select its **surface** then open the
**Decals** submenu, which lists one item per decal on that surface.

**Deferred (explicitly not in R6):** interactive decal-pattern painting UI. Pattern
content is authored via the existing headless `decal_painter` and `assets/decals/*`
files. R6 creates empty decal instances and places/orients them; painting stays
deferred past R6.

**Asymmetry accepted:** lights are wall-anchored-style via adjacent-cell center
(points), decals are surface-projected canvases (surface patches). This is
coherent — a point and a surface patch are different shapes — and keeps each flow
minimal.

## 5. Format version

**Decision:** **No scene version bump.** v4 already serializes every field R6
needs to author. Confirmed in `scene_format` and existing fixtures:

- Decals: `rotation`, `depth`, `glyph_step`, `size` (width/height), `uv`,
  `anchor`, `surface`, `asset_kind`, `asset_id` — all present in the existing v1
  fixture at `tests/test_scene_format.c:46-55`.
- Lights: `radius`, `intensity`, `color`, position — all present at
  `tests/test_scene_format.c:57-60`.

**Implication:** R6 work is purely command + placement + UI, with no migration
or fresh-format tests.

## Cross-cutting constraints

- All authored edits go through typed command mutations behind the single
  undo/redo history; UI never mutates authored structures directly.
- Reuse the existing surface submenu hierarchy (Enter opens/applies, Up/Down
  navigates, Esc ascends) as the host for the Decals flow.
- Reuse the existing wall-removal confirm-flow pattern for light (and decal)
  deletion.
- No defensive or hidden authored state; derived views stay derived.

## Open follow-ups (recorded for later, not blockers)

- Interactive decal painting UI (deferred past R6).
- Off-wall free-floating light placement gesture if needed later.
- Any future per-face wall material or sprite widening is separate from R6.
