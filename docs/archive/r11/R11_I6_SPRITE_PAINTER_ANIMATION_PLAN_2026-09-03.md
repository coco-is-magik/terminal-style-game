# R11 I6 Sprite Painter Animation Plan — 2026-09-03

## Status and authority

**Verified on 2026-09-04.** Automated verification and manual editor acceptance
passed. This document is the authoritative I6 requirements and
implementation record for the sprite painter animation-authoring increment. It
supersedes the earlier mixed static-file versus animated-folder storage decision.

The governing decisions remain:

- `R11_DECISION_RECORD_2026-08-28.md`
- `R11_I5_SPRITE_ANIMATION_DECISION_RECORD_2026-09-03.md`
- `R11_INCREMENT_I5_IMPLEMENTATION_RECORD_2026-09-03.md`

If this plan conflicts with those records, the newer I6 plan controls the authoring
work for the sprite painter, but the implementation must preserve the verified R11
runtime guarantees unless a later decision record explicitly changes them.

## I6 objective

Extend the existing sprite painter so one UI can author both static and animated
sprite folders.

The intended model is:

- every sprite asset lives in a folder;
- every folder contains `animation.txt`;
- the literal file content `static` means the folder contains exactly one frame;
- animation metadata contains `fps`, optional `loop`, and at least two ordered
  `frame` entries.

Static example:

```text
assets/sprites/1/
  animation.txt       # exactly: static
  frame_000.txt
```

Animated example:

```text
assets/sprites/4/
  animation.txt
  wide.txt
  tall.txt
```

```ini
fps=4
loop=true
frame=wide.txt
frame=tall.txt
```

This avoids stale dual-format sprite storage and makes collapse between static and
animated states explicit and reversible.

## Required behavior

### Navigation and focus

- The sprite painter must support arrow-key navigation between the canvas and a
  frame-management menu.
- Focus can move from the canvas to the menu and back without changing the existing
  direct-typing control model.
- The menu must stay inside the sprite painter rather than becoming a separate
  editor mode.

### Frame editing

- Adding inserts a new empty frame immediately after the selected frame and selects
  the inserted frame.
- Removing always deletes the selected frame, then selects the previous circular
  position: deleting index zero selects the new last frame; otherwise it selects
  the preceding index.
- Frame selection is circular within the current sprite asset.
- A frame is edited through the canvas while the left and right neighbors are shown
  as previews.
- For 2-frame animations, the left and right previews both resolve to the other
  frame.
- Preview resolution must wrap correctly when the asset is animated.

### Static and animated conversion

- A sprite asset becomes animated only when it has at least two frames.
- An animation may not persist with one frame in animation mode.
- If deleting the currently selected frame would leave one frame, the asset must
  collapse back to literal `static` mode.
- When the asset collapses back to static, the manifest must no longer retain stale
  animation-only data.
- The surviving frame becomes the static sprite representation for that ID.

### Persistence and identity

- Numeric sprite IDs remain the identity for both static and animated folder assets.
- Existing scene/object references continue to point to the same numeric sprite ID.
- Save, reopen, refresh, and runtime rebuild behavior must remain deterministic.
- The save path must be atomic enough that failed writes do not leave a partial or
  mixed static/animated sprite state on disk.

## Explicit non-goals

- No object-transform animation.
- No generic animation framework, clip graph, or state machine.
- No scene-format change for sprite references.
- No separate animation editor outside the sprite painter.
- No new global control-lock system.
- No persisted playback phase.

## Planned data model impact

The current static-sprite file model is replaced for I6 planning by a folder-only
sprite asset model.

The implementation is expected to define:

- folder layout for static and animated sprites;
- the literal-static and animated `animation.txt` schemas;
- frame-file naming and ordering rules;
- conversion rules between static and animation states;
- save/reload behavior when the asset changes mode.

## Planned editor behavior

The sprite painter should behave like a single authoring surface with two panes of
attention:

1. the canvas for editing the selected frame; and
2. the menu for frame operations and mode-related actions.

The menu must be navigable with arrow keys only. The existing direct typing used by
the painter must continue to work where it already applies.

Implemented focus model:

- canvas focus owns direct typing, Backspace, material cycling, and cursor arrows;
- Right at the canvas edge transfers focus to the frame menu;
- menu Up/Down selects Previous, Next, Add, Remove, FPS−, FPS+, or Loop;
- menu Left returns focus to the canvas;
- Enter invokes the selected menu action;
- Ctrl+S saves the complete staged document; Escape discards staged edits by
  reopening the unchanged registry-backed sprite and returns to Pattern actions.

`SpriteDocument` owns deep-copied frames, selected-frame state, FPS, loop, path, and
dirty state. `AssetRegistry` is unchanged until Save completes and the document is
committed. Persistence writes a complete sibling temporary folder, moves an existing
folder to a backup, installs the candidate by rename, and restores the backup when
installation fails. Canonical saves use `frame_NNN.txt` names and naturally remove
stale animation metadata/files when an animation collapses to static.

## Migration

Bundled `assets/sprites/1.txt`, `2.txt`, `3.txt`, `5.txt`, and `6.txt` moved to
numeric folders as `frame_000.txt`, each with a literal-static `animation.txt`.
Animated ID 4 remains a numeric folder. Numeric scene and object references did not
change. Root-level numeric sprite files are no longer discovered by the loader;
external assets must be migrated to the folder shape above.

## Verification boundary

The I6 implementation must be validated with tests that cover:

- static folder load/save round-trip;
- animated folder load/save round-trip;
- add-frame and remove-frame operations;
- collapse from animation to static when the asset drops to one frame;
- circular neighbor preview behavior;
- 2-frame preview behavior;
- preservation of numeric sprite IDs and scene/object references;
- failure handling for malformed manifests and failed writes;
- regression protection for the existing sprite painter and runtime playback.

Focused automated evidence on 2026-09-04:

- sprite document: 4/4 passed;
- asset refresh/loader: 6/6 passed;
- unified editor: 85/85 passed;
- animation player: 3/3 passed;
- sprite renderer: 6/6 passed;
- all affected runners compiled with `-Wall -Wextra -Wpedantic -Werror`.

Broad evidence on 2026-09-04:

- full strict `make test`: passed;
- strict application build and `make smoke`: passed with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- `make check-current-renderer`: passed;
- `make benchmark-sprite-render`: passed with baseline `3.468657 ms`, 128 sprites
  `4.286846 ms`, both below the `6 ms` budget; deterministic checksums remained
  `1846712605617511097` and `5403392855886966484`.
- combined ASan+UBSan with leak detection: all five affected focused runners passed
  (`sprite-document`, `asset-refresh`, `unified-editor`, `sprite-animation-player`,
  and `sprite-render`).
- `make matrix`: all 8 variants passed (`USE_NO_STATE_TRACKER`, `USE_DIRTY_CELLS`,
  three SMC tracker alternatives, default SMC stream tracker, lighting cache, and
  glyph cache).
- `make style`: skipped because `cppcheck` is not installed; strict compiler
  warnings remained clean under `-Werror`.

During verification, the new narrow-frame preview test found overlapping
`previous`/`active` labels. Minimum-width preview slots fixed the overlap. Earlier
failed commands were also recorded: unsupported convenience Make target names, a
dependent build/run race that executed a stale test binary, and strict test compile
errors from a misplaced local declaration. None remains open.

Manual acceptance passed on 2026-09-04:

1. Open a static sprite Pattern painter and confirm only the active canvas appears.
2. Move Right from the canvas edge into the frame menu and Left back to the canvas.
3. Add a frame, type into it, and confirm the prior/next previews wrap.
4. With two frames, confirm both previews show the same other frame.
5. Remove the selected frame from two to one, Save, reopen, and confirm static mode.
6. Change FPS and loop, Save, and confirm runtime playback follows the saved values.
7. Make an unsaved frame edit, press Escape, and confirm the saved asset is restored.

The user confirmed the planned objectives were accomplished. Four non-blocking
follow-up observations were recorded separately in
`R11_PLANNED_ANIMATION_IMPROVEMENTS_2026-09-04.md`: subtle frame-change flicker,
add-frame-as-copy convenience, object/entity sprite authoring access, and future
sprite stacking/multiple-angle design.

## Notes for implementation planning

- Keep runtime playback code separate from authoring behavior.
- Keep the verified renderer contract intact: it consumes resolved frames rather
  than owning time.
- The sprite painter should remain the main interaction surface for this work.
- If any naming or storage detail must change during implementation, record that in
  a later decision record before editing source.

## Next safe action

I6 and R11 are closed and Verified. Final boundary findings are in
`reviews/2026-09-04-roadmap-r11-closeout.md`. Future animation work begins with the
planned-improvements document and a separate investigation or requirements decision.