# Roadmap R11 Final Entity/Sprite Boundary Review and Closeout — 2026-09-04

## Outcome

R11 is **Verified** as of 2026-09-04. I1–I6 automated gates pass and their applicable
manual visual/input acceptance has passed. The verified single authored spawn remains
the required baseline; game-mode-dependent spawn expansion is explicitly unnecessary
without game modes and is not an R11 blocker.

The manual I6 review recorded four non-blocking future improvements in
`../R11_PLANNED_ANIMATION_IMPROVEMENTS_2026-09-04.md`: subtle animation flicker
investigation, add-frame-as-copy convenience, sprite creation/editing from object or
entity workflows, and research into sprite stacking and multiple-angle sprites.

## Scope reviewed

- I1 decorative billboard runtime and renderer integration
- I2 scene-owned sprite placement, stable identity, authoring, and persistence
- I3 typed minimal triggers and pure runtime session
- I4 reusable `simple` objects, scene-owned instances, collision, and sprite choice
- I5 folder-backed animation data and per-instance elapsed-time playback
- I6 folder-only static/animated assets and integrated staged painter authoring
- migration, malformed-input handling, failure restoration, input ownership,
  performance evidence, documentation, and deferred scope

## Final findings and dispositions

### Blockers

**None.** No finding requires reopening I6 or prevents R11 closeout.

### Accepted non-blocking observation

The user observed a slight, intermittent-looking flicker or jitter at animation frame
changes. The cause is unknown. Existing deterministic player/render tests pass, but
they do not reproduce or explain display cadence. The issue is accepted as a
post-R11 investigation item rather than misdiagnosed during closeout.

### Planned improvements

1. Add a convenience operation that inserts a copied frame after the selected frame,
   subject to a narrow UX and failure-atomicity decision.
2. Allow sprite search, creation, and painter entry from object/entity sprite
   selection while retaining separate asset and scene transactions.
3. Research typed sprite stacking semantics.
4. Research deterministic multiple-angle sprite data, runtime view selection, and
   interaction with animation phase.

The latter two were presented together by the user as one future area and remain one
section in the improvements record, while this review names their separate technical
questions. None is implemented or authorized by this closeout.

## Boundary review

### Asset versus scene ownership — preserved

- `AssetRegistry` owns loaded static frames and animation definitions.
- `SpriteDocument` owns deep-copied staged frame cells, selected frame, FPS, loop,
  path, and dirty state.
- `SceneDocument` owns placed sprite/object instances and numeric references only.
- Unsaved sprite cells or animation metadata do not enter scene persistence or scene
  command history.
- Object sprite assignment remains an undoable per-instance numeric-reference edit;
  I6 did not add sprite bytes to object definitions or instances.

### Authored versus runtime state — preserved

- Animation playback phase remains transient in each `SpriteEntity` as current frame
  plus elapsed remainder.
- Runtime construction/rebuild resets phase without changing authored scene or asset
  data.
- `sprite_animation_player` accepts explicit elapsed seconds and advances instances
  independently.
- No playback phase, wall-clock access, hidden randomness, or generic entity tick
  state entered persisted data.

### Renderer boundary — preserved

- `sprite_render` resolves the frame selected by runtime state and owns no time.
- Sprites remain decorative camera-facing billboards: they do not block rays/light,
  collide, appear in reflections, or acquire transform animation through I6.
- Existing optical, lighting, and depth-frontier contracts remain outside animation
  authoring.

### Editor and persistence boundary — preserved

- The existing sprite painter was extended; no separate animation editor or generic
  animation framework was introduced.
- Direct typing is canvas-owned; frame-menu focus prevents typed glyphs from leaking
  into document edits.
- Save writes a complete sibling candidate folder and uses rename/backup restoration;
  Escape reconstructs the staged document from the unchanged live registry.
- Numeric sprite IDs and scene/object references remain stable through static ↔
  animated conversion.

### Explicit non-goals — still absent

- no object-transform animation;
- no clips, state machines, events, blending, ping-pong, or gameplay callbacks;
- no persisted playback phase or per-instance playback-rate override;
- no sprite stacking or multiple-angle sprite schema;
- no generic component/animation framework;
- no game-mode spawn system without a concrete game-mode requirement.

## Verification reviewed

Final I6 evidence is recorded in
`../R11_I6_SPRITE_PAINTER_ANIMATION_PLAN_2026-09-03.md`:

- focused suites: sprite document **4/4**, asset refresh/loader **6/6**, unified
  editor **85/85**, animation player **3/3**, sprite renderer **6/6**;
- full strict `make test`: passed under `-Wall -Wextra -Wpedantic -Werror`;
- combined ASan+UBSan with leak detection: all five affected runners passed;
- eight-variant build matrix: passed;
- strict application build, smoke, and current-renderer checks: passed;
- sprite benchmark: baseline **3.468657 ms**, 128 sprites **4.286846 ms**, both under
  the **6 ms** budget with deterministic checksums unchanged;
- bundled static sprite migration and malformed/failure-restoration coverage: passed;
- optional `cppcheck` style check: skipped because the tool is unavailable; strict
  compiler warnings remained clean.

## Manual acceptance — passed 2026-09-04

The user completed the I6 live editor review and confirmed the planned objectives
were accomplished. Accepted behavior includes static and animated folder authoring,
canvas/menu focus movement, frame operations, circular neighboring previews,
two-frame preview behavior, animation-to-static collapse, FPS/loop persistence,
and staged discard.

The four improvement observations above were recorded separately and do not change
the pass result.

## Exit decision

The final broad-R11 closeout criteria are met:

- sprite runtime and authoring are verified;
- typed minimal triggers are verified;
- the concrete typed `simple` object baseline is verified;
- animation runtime, data, and authoring are verified;
- the existing single authored spawn is verified and broader game-mode spawn
  expansion is explicitly unnecessary;
- final boundary findings contain no blocker;
- automated, migration, failure-restoration, performance, and manual evidence is
  recorded.

R11 is released from this checkpoint. Future animation/sprite work starts from the
planned-improvements document and requires its own investigation or Q1 decision.