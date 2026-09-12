# R7 Decision Record — Structural Editing and Scale — 2026-08-13

## Authority

This record captures the product and architecture decisions made in the 2026-08-13
design thread before R7 implementation begins. Decisions are binding for all R7
increments and are grounded in existing code. The authoritative requirements and
increment plan is `R7_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-13.md`.

## Scope of R7

R7 is "Structural editing and scale" from `docs/FEATURE_ROADMAP.md`: explicit map
expansion/contraction with defined origin semantics, bulk selection sets and
primary-selection behavior, atomic batch operations, stable selection/reference
behavior across resize, and validated map-size / command-memory / performance
limits.

Two findings from code ground these decisions:

- `SceneDocument` already implements **east/south copy-growth and safe
  refill-shrink** as undoable command-group mutations
  (`EDITOR_MUTATION_RESIZE_*` via `scene_document_internal_resize_east` /
  `resize_south`), with shrink **content-blocked**
  (`CMD_RESULT_RESIZE_BLOCKED` / `SCENE_RESIZE_CONTENT_BLOCKED`). The map keeps a
  fixed top-left origin and growth is recorded by LIFO `east_growth[]` /
  `south_growth[]` provenance vectors.
- Editor selection is currently a **single primary** `SelectionTarget`
  (`editor_selection.h`); there is no bulk set, and
  `editor_selection_is_valid_for_map` already guards validity after resize.

So R7 builds a bulk-selection and atomic-batch layer on top of an existing,
already-undoable resize model. R4 explicitly deferred the resize behavior that R7
now formalizes.

## 1. Origin model

**Decision:** **Option B — fixed top-left origin at `(0,0)`; the map spans
`[0..w) × [0..h)`.** Expansion and contraction occur only on the **east** and
**south** edges, in LIFO order. Shrink **blocks on content**: it is rejected
when the boundary ring is non-empty, so content is never silently deleted.
West/north expansion is not offered.

**Rationale:** The existing resize machinery already implements exactly this model
and is fully undoable. It preserves coordinate stability — existing content,
lights, decals, spawn, and surfaces keep their authored coordinates across resize.
No silent deletion (roadmap forbidden shortcut) holds because shrink is rejected
on non-empty content. This is the smallest change that satisfies "defined origin
semantics": the origin is always top-left and only the east/south edges move.

**Accepted tradeoffs:**

- West/north edges cannot grow and content cannot be cropped; shrinking an
  occupied boundary ring requires removing that content first (remove walls /
  move lights / remove decals), then shrinking.
- Coordinate-addressed surfaces (decals, lights, spawn) never shift, so no remap
  rule is required and R4's deferred "preserve or remap surfaces" constraint is
  satisfied by the **preserve** policy.

**Rejected alternatives:**

- Option A (all four edges, content coords stable): adds west/north growth and
  mirrored copy/shrink for no current need; deferred.
- Option C (explicit world origin/offset with full coordinate remap): would force
  a scene format version bump and a per-entity remap rule, conflict with "no ad
  hoc coordinate repair," and is rejected.

## 2. Selection-set model and multiselect gesture

**Decision:** Introduce an editor-transient **selection set** (a set of targets by
stable scene ID / surface coordinate) that always retains a single **primary**
(the last-selected face). The multiselect gesture is **Ctrl + arrow keys**:

- Selecting one face becomes the primary and starts the set.
- **Ctrl + arrow** extends the set on one axis.
- Extension is **constrained to the same surface family** (wall / ceiling / floor)
  and **same orientation** (a wall stays on its face orientation); a wall set
  cannot mix ceiling or floor, and opposite-face walls are not combined with a
  forward-facing run.
- Extension is **occlusion-respecting**: a face that is not ray-visible at its
  extent (hidden behind another opaque surface) is not added and stops the run.
- All selected faces are visually indicated, with the primary (last-selected)
  face shown distinctly via the existing face-selection highlight vocabulary.

**Rationale:** Ctrl+arrow is a low-cost, predictable extension; axis +
family + orientation constraint keeps the set geometrically simple; the
ray-based occlusion rule reuses the existing first-hit surface pick rather than
introducing a separate visibility test. Because selection is editor-transient
state and never serialized, it needs **no scene format bump**.

**Primary-selection behavior:** the primary is the face the user most recently
selected/extended onto; batch operations and single-face editing act on the
primary, while the reported count covers the whole set.

**Deferred (not in R7):** arbitrary free-form multi-select (box/region), mixed
family selection, and selection persistence. These remain unsequenced ideas.

## 3. Reduced operations and batch semantics

**Decision:** While more than one face is selected, the action set is reduced to
three atomic batch operations, each applied to the whole set as **one undoable
step**:

1. **Remove** — removes the selected wall material / surface content for every
   member (and attached wall decals) in a single command group.
2. **Apply material** — applies one material reference to every member in a
   single command group.
3. **Add decal** — places **one decal instance per selected surface**, all
   committed as a single undoable batch.

Because the constraint in decision 2 keeps a set homogeneous by family, batch
operations are unambiguous — no family-typed "ignore non-matching member" rule is
needed. Per-field property editing (width/height/rotation/etc.) remains available
only for a single-selected face, not across a batch.

**Decal batch size:** the placed-instance count is bounded by the selection set
size and by existing decal capacity checks; a batch that would exceed capacity is
rejected atomically and reports rather than partially applying.

## 4. Limits and scaling policy

**Decision:** R7 introduces explicit, validated limits plus stress testing:

- Map-size validation remains bounded (`SCENE_MAX_WIDTH = 512`,
  `SCENE_MAX_HEIGHT = 256`), with `checked_size_2d` used at every growth/shrink
  allocation; expansion rejects at the limit with an explicit result.
- Command history keeps its existing bounded-memory accounting; R7 adds explicit
  tests for large maps, deep resize histories, and forced allocation-failure
  rollback of resize and batch commands.
- Bulk selection set membership is bounded by the map extent; no unbounded
  allocation or unchecked dimension multiplication is introduced.

**Implication:** no scene version bump (selection is transient; resize already
round-trips through the existing v4 fields for dimensions and growth provenance).

## Open follow-ups (recorded for later, not R7 blockers)

- Free-form / region multi-select, and mixed-family selection.
- West/north expansion or explicit content-crop (offset-origin model), if a later
  phase needs it.
- Per-family axis mapping edge cases (see the R7 plan for the default mapping).
