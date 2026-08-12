# R4 Follow-up — UX and World-Editing Refinements from the R4 Review — 2026-08-11

## Provenance and status

This scope springs directly from the **R4 targeted review interactive testing**
(`../reviews/2026-08-11-roadmap-r4-targeted-review.md`). The headless/human review of
R4 Increments A–F found the core workflow "mostly working" and surfaced five focused
refinements to the surface editor. This is **documentation only — no implementation
has started.**

- R4 remains **Active**; the interactive acceptance checklist remains **Pending**.
- Several items amend the approved R4 contract (attachment policy, inspector
  controls, construction edge semantics). R4 Verified cannot complete until those
  land and the interactive checklist is re-run against the amended behavior.
- No new phase, origin semantics, or per-face/height geometry is introduced. The
  approved persisted-growth requirement now introduces native scene v3 while keeping
  v1/v2 as migration inputs.

## Interrupted implementation status — 2026-08-12

Implementation began after the manual R4 review and was interrupted before the
edge-growth format work. The working tree is intentionally uncommitted. Do not mark
R4 Verified from this state.

### Completed in the working tree

1. **Horizontal highlight and selection refactor:** floor/ceiling highlights use
   border-only `#`/`.` rendering, preserve interior material glyphs, and share
   `editor_project_horizontal_cell()` between selection and highlighting.
2. **Surface inspector hierarchy:** Enter opens/applies the material submenu,
   Up/Down navigates the active level, Esc ascends, surface Left/Right editing is
   removed, ambient uses inline entry, and west/north Remove Wall is visibly
   unavailable and skipped by field navigation. The light inspector is unchanged.
3. **Wall-decal cascade:** wall removal snapshots attached wall decals in command
   history, removes them atomically, and restores exact indices/IDs on undo.

### Focused evidence from a clean rebuild

| Runner | Result |
|---|---:|
| `test-editor-selection` | 21/21 passed |
| `test-editor-highlight` | 17/17 passed |
| `test-editor-domain` | 8/8 passed |
| `test-command-system` | 29/29 passed |
| `test-unified-editor` | 53/53 passed |
| `git diff --check` | passed |

The prior `test_load_success_resets_history_and_selection` segfault is resolved. Its
fixture had manually assigned `history.count`/`cursor` without allocating command
entries, which became invalid once commands gained an owned decal payload. The test
now creates three real ambient commands through the public API and undoes one before
verifying load reset. This preserves the original intent and exercises valid ownership.

### Implemented after restart

- Native scene v3 parsing, serialization, validation, v1/v2 migration, canonical
  fixture, specification, and persisted growth provenance.
- East/south copy-growth, refill-triggered safe shrink, exact undo/redo, dimension
  limits, copied-ring equality guards, and light/decal content guards.
- Obstructed-decal benchmark/stability evidence with exact no-decal checksum parity.
- Strict `make check`, ASan, UBSan, 8/8 matrix, benchmark, and stability pass.
- Amended interactive acceptance remains the only open gate.

### Product decisions made during implementation

- Auto-growth provenance must survive Save/reopen; therefore canonical native Save
  moves to **scene v3**. Native v1/v2 remain accepted migration inputs.
- East/south growth copies the entire previous edge row/column outward cell-for-cell.
- Persisted provenance uses ordered east trigger rows and south trigger columns.
  Refill shrink applies to the current outermost trigger; nested growth shrinks in
  reverse order.
- Proposed canonical v3 metadata is `east_growth = -|row[,row...]` and
  `south_growth = -|column[,column...]`, required once in v3 and forbidden in v1/v2.

### Next action

Run and record the amended real-video checklist. Do not mark R4 Verified before it
passes.

## Workstream 1 — Inspector navigation hierarchy + disabled options

**Origin:** navigating surface inspectors with Left/Right for sub-items (materials)
felt unintuitive, and two `>` arrows (top-level field + sub-item) rendered at once
were confusing.

Decisions:

1. **Two-level hierarchy** for the three surface inspectors (wall material, floor
   surface, ceiling surface):
   - Up/Down navigate within the current level.
   - `Enter` on an item with a submenu (Material) descends into the submenu.
   - `Esc` inside a submenu ascends one level.
   - `Esc` at the top level deselects the targeted surface (closes the inspector,
     existing behavior).
2. **Markers:** exactly one arrow (`>`) on the single actively selected item. Green
   highlight persists as the selection memory — the parent row (e.g. "Material")
   stays green while its submenu is open, and the highlighted submenu row carries
   both arrow and green. Arrow moves with Up/Down inside the submenu.
3. **Left/Right are removed from the surface inspector entirely.**
4. **Ambient:** `Enter` opens inline numeric entry; `Enter` commits; `Esc` returns
   to the field list.
5. **Construction (Place/Remove Wall)** remains a direct-action row: `Enter` executes.
6. **Disabled options — reusable feature:** a row can be rendered greyed,
   unselectable, and still visible in its usual position. First use: "Remove Wall"
   on west/north boundary walls is greyed instead of producing a refusal or empty
   void. Navigation skips disabled rows. The mechanism is reusable for future menu
   items.
7. **Scope:** the light inspector is unchanged (Up/Down fields, Left/Right step,
   inline value).

Doc/test impact: `editor_domain.c:44,51` control strings and `test_editor_domain.c:190`,
`test_unified_editor.c:1708` ("Up/Down=field"), README key-map table, and the
surface-inspector navigation tests (`test_input.c`, `test_unified_editor.c`).

## Workstream 2 — Edge-wall growth (east/south) + disabled west/north removal

**Origin:** removing a wall at the map edge opened a view outside the map ("hole to
nowhere"); preferred behavior is a map that grows instead.

Decisions:

1. **East and south edges:** removing the boundary wall grows the map by 1 unit in
   that direction. The complete prior edge row/column is copied outward cell-for-cell,
   preserving occupancy and all three surface materials. The selected old-edge wall
   becomes empty interior; its copied cell behind it remains a same-material wall.
2. **Refill shrinks:** placing a wall back in the emptied interior cell shrinks the
   map by 1, discarding the outer ring cell.
3. **West and north edges:** removal is not allowed; "Remove Wall" is greyed and
   unselectable (Workstream 1 feature). No void view, no error message.
4. **Bounds:** growth refuses with a status line at `SCENE_MAX_WIDTH` (512) /
   `SCENE_MAX_HEIGHT` (256). Shrink refuses with a status line if the dropped ring
   contains a light or a floor/ceiling decal (data-safety guard).
5. **Design notes:** a new structural mutation resizes `authored_cells[]`,
   `Map.width/height/cells/light_map`, `SceneSurfaceView`, and
   `authored_cell_count` in one undoable command; undo/redo restores exact
   dimensions and contents. East/south append/trim keeps every existing coordinate
   (spawn, lights, decal world positions, wall-decal refs) stable; `origin_x/y`
   remain 0. Native scene v3 persists ordered east trigger rows and south trigger
   columns so refill shrink survives Save/reopen. If the removed east/south wall
   carried a wall decal, Workstream 3 cascade deletion applies (not transfer).

Doc/test impact: R4 plan verification inventory and "rejected approaches" note;
README editor-limits; new command-system structural-mutation tests.

## Workstream 3 — Decal cascade removal on wall removal

**Origin:** the user could not verify what happens when a wall with a decal is
removed; requirement: the decal is also removed, with no leaked resources.

Current behavior — a deliberate, approved R4 design: removal returns
`CMD_RESULT_WALL_ATTACHMENT_BLOCKED` (`command_system.c:188-196`), asserted at
`test_command_system.c:984`; the R4 plan's rejected approaches rejected decal
deletion; the review checklist item 3 expects the refusal on the fixture wall decal
at `(4,2)`.

Decisions (amendment to the R4 contract):

1. Removing a wall also removes **all wall decals attached to that cell** in the
   same undoable command.
2. Undo restores each decal exactly, including its original stable instance ID (no
   ID churn); redo re-applies removal.
3. Floor/ceiling decals are world-space and are not removed.
4. Resource safety: document decals own no heap (patterns live in the asset
   registry) — removal is array compaction; the runtime world is rebuilt
   transactionally after the command and `world_clear()` already frees every runtime
   decal pattern, keeping the existing failure-atomicity path intact.

Doc/test impact: new mutation type `REMOVE_DECAL` (by instance ID) plus a grouped
remove-wall+remove-decal command; update `test_command_system.c:984`, the R4 plan
rejected-approaches line, review checklist item 3 (now a cascade assertion), and
README editor-limits.

## Workstream 4 — Floor/ceiling highlight becomes a border outline + dedupe

**Origin:** floor/ceiling selection/hover visually differed from walls, and the fill
highlight occluded the material being edited; the user dislikes duplicated code.

Decisions:

1. Floor/ceiling highlight is a **border outline** (perimeter of the projected cell
   patch), exactly like walls. The interior authored material stays visible in both
   hover and selected states; the checkerboard hover fill is removed.
2. **Uniform glyphs `#` (selected) / `.` (hover)** — the `F/f/C/c` macros
   (`editor_highlight.h:22-25`) are deleted; surface type is identified by the
   inspector title. Light markers keep `@`/`o` (point markers, not surface fills).
3. **Dedupe (editor-side):** walls and horizontal surfaces share one outline
   renderer over per-column spans — a wall face slice and a floor/ceiling cell
   patch both reduce to a contiguous row span per column. One shared horizontal-plane
   helper (screen row → distance → map cell) serves `editor_highlight.c` and
   `editor_selection.c` (`editor_selection.c:180-224`).
4. `draw_horizontal_highlight_cell` (`editor_highlight.c:51-71`) is deleted.
5. The renderer hot loop (`raycast.c`) stays untouched (SMC-optimized); a parity
   regression test pins renderer floor/ceiling cell == editor highlight cell for the
   same camera/grid so the two copies cannot silently drift.

Doc/test impact: `test_editor_highlight.c` macro-based assertions (lines 468-572)
rewritten for `#`/`.`; add interior-material-visible-after-highlight assertions;
benchmark/stability re-run to confirm no frame-cost regression.

## Workstream 5 — Obstructed-decal render cost evidence

**Origin:** walls can be placed to cover decals; the user wanted assurance that
obstructed decals cost nothing to render.

Verified findings: `render_decals` z-buffer-culls per glyph
(`depth > z_buffer[col] + 0.001`, `raycast.c:162`), so fully occluded decal glyphs
are not drawn. The remaining per-frame cost is iterating decals and projecting
non-space pattern cells before the depth test. Floor/ceiling decals are world-space,
so no occupancy-only skip provably matches current visuals; wall decals cannot exist
on empty cells under the current/amended invariants.

Decisions:

1. Add benchmark evidence — frame cost with a fully obstructed decal behind a wall
   vs without — using the existing deterministic surface benchmark/stability
   harness; record numbers in the follow-up implementation record.
2. Optional defensive skip for wall decals whose cell occupancy is not `WALL`
   (cheap; provably no visual change by invariant).
3. No aggressive floor/ceiling occlusion prefilter in this slice (R6 authoring
   territory).

## Resolved defaults

- Material row `Enter` = apply and return to the field list.
- Light inspector unchanged.
- Growth cap and shrink-ring refusals shown as status text.
- Growth copies the entire previous edge row/column outward cell-for-cell.
- Native v3 persists ordered growth triggers; v1/v2 migrate to v3 on Save.

## Gates and files

Implementation gates (when started): strict build, `test-editor-domain`,
`test-unified-editor`, `test-command-system`, `test-editor-highlight`,
`make check` / `asan` / `ubsan` / `matrix`, `git diff --check`, then a follow-up
implementation record with per-workstream evidence.

Primary files: `editor_highlight.{c,h}`, `editor_selection.c`, `editor_domain.c`,
`command_system.{c,h}`, `scene_document_internal.h`, `scene_document.c`,
`unified_editor.{c,h}`, `input.{c,h}`, R4 plan / roadmap / README / review
checklist, and this handoff + the implementation record.
