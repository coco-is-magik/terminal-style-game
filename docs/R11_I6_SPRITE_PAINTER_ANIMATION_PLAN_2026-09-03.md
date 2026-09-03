# R11 I6 Sprite Painter Animation Plan — 2026-09-03

## Status and authority

**Planned.** This document is the authoritative I6 planning record for the sprite
painter animation-authoring increment. It supersedes the earlier mixed static file
versus animated-folder model for I6 planning only.

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
- `animation.txt` records the asset mode;
- `mode=static` means the folder contains one frame only;
- `mode=animation` means the folder contains two or more frames and uses the same
  folder-based asset model.

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

- The menu must support adding frames and removing the currently selected frame.
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
  collapse back to `mode=static`.
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
- the `animation.txt` schema for `mode=static` and `mode=animation`;
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

## Notes for implementation planning

- Keep runtime playback code separate from authoring behavior.
- Keep the verified renderer contract intact: it consumes resolved frames rather
  than owning time.
- The sprite painter should remain the main interaction surface for this work.
- If any naming or storage detail must change during implementation, record that in
  a later decision record before editing source.

## Next safe action

Implement the folder-only sprite painter plan with tests before expanding scope to
any additional timeline or animation UX.