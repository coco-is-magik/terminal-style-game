# Planned Animation Improvements — 2026-09-04

## Status and scope

**Planned follow-up candidates; not part of completed R11 I6.** These observations
were recorded during the successful I6 manual review on 2026-09-04. None blocked
acceptance of the planned folder-only sprite and animation-authoring increment.

Each item requires investigation or a separate requirements decision before source
changes. This document records user-visible needs without assuming an implementation
or silently expanding the verified R11 boundary.

## 1. Investigate subtle animation flicker or jitter

### Observation

Animated sprites show a brief, slight, barely perceptible visual flicker during
frame changes. The effect creates a sense of jitteriness, but the manual review did
not establish its cause.

### Current evidence

- The effect was observed in a real display session during I6 manual acceptance.
- Existing deterministic animation-player and sprite-render tests pass.
- Playback phase remains per runtime instance and advances from explicit elapsed
  seconds; the renderer only consumes the resolved frame.
- No timing trace, capture, or frame-by-frame display evidence was collected during
  the manual review.

### Investigation requirement

Do not assume a delayed frame update is the cause. Measure and distinguish at least:

- animation tick cadence versus rendered/presented frame cadence;
- elapsed-time accumulation and multi-step transitions;
- frame dimension, transparent-cell, anchor, or projected-size changes;
- registry/runtime rebuilds or editor preview-specific state resets;
- renderer/compositor/presentation artifacts unrelated to animation scheduling.

Capture a minimal reproducible asset, authored FPS, display refresh conditions,
runtime mode, frame-time samples, and—if practical—frame captures. A fix must retain
per-instance phase, explicit-delta determinism, and the renderer-owning-no-time
boundary.

### Disposition

**Open investigation; non-blocking for R11 I6 closeout.** Cause and remedy are
unverified.

## 2. Add-frame-as-copy convenience

### Observation

Animation authoring would be substantially faster if a new frame could begin as a
copy of the previous/current frame instead of always starting empty.

### Candidate behavior to decide

A likely narrow operation is **Add frame copy**: deep-copy the selected frame,
insert it immediately after that frame, and select the inserted copy. The existing
**Add frame** operation should remain available for an empty frame unless a later UX
decision replaces it explicitly.

Before implementation, decide the label, shortcut/menu placement, capacity and
allocation-failure feedback, and whether “previous” means the currently selected
source or the frame preceding the insertion point. Preserve staged ownership and
make allocation failure non-mutating.

### Disposition

**Planned convenience candidate; not yet specified or implemented.**

## 3. Sprite creation and editing from object/entity workflows

### Observation

The sprite painter is currently reached through world-sprite placement/selection.
An object can choose an already-loaded sprite, but the flow “place object → create
new sprite → design sprite → assign it to the object” is clunky and indirect.

### Desired direction

Bring sprite asset discovery, creation, and painter entry to object/entity sprite
selection, analogous to material search behavior:

- direct typing filters numeric sprite choices;
- when no matching sprite exists, a visible create-new action is available;
- an existing sprite can open the same staged sprite painter;
- a newly saved sprite can be assigned to the object/entity through the existing
  undoable per-instance sprite-reference command.

### Boundaries to preserve

- `SpriteDocument` owns unsaved sprite bytes and metadata; object/scene documents do
  not absorb them.
- The object instance stores only its numeric sprite reference.
- Creating or saving an asset must not silently alter scene command history.
- Assigning the saved sprite to an object remains a separate undoable scene command.
- Cancel, failed save, and failed runtime rebuild must preserve the prior object
  reference and staged asset state according to an approved workflow.

The reusable nested-inspector/menu question may overlap R12, so requirements should
decide whether to implement a local object-sprite flow first or wait for that shared
UI foundation.

### Disposition

**Planned authoring-workflow improvement; separate requirements required.**

## 4. Sprite stacking and multiple-angle sprites

### Observation

Future sprite work should consider both sprite stacking and multiple-angle views.

### Questions for a future decision package

Sprite stacking:

- Is a stack one reusable composite asset, multiple scene/object sprite references,
  or multiple independently transformed instances?
- How are layer order, offsets, depth, lighting, selection, and missing layers
  represented?
- Does a stack animate as one synchronized unit or allow independent layer clips?

Multiple-angle sprites:

- Which authored direction owns each view, how many angle buckets exist, and how are
  boundary ties resolved deterministically?
- Is view selection based on camera-relative object front direction, world-relative
  direction, or another explicit transform?
- Does each angle contain a static frame or its own ordered animation?
- How do mirroring/fallback rules work when an angle is missing?
- How do angle selection and per-instance animation phase interact?

These concepts must not be encoded by overloading the current frame order. They need
explicit typed data and deterministic lookup semantics. Object front direction is
already authored and persisted, but current billboard rendering intentionally uses
one camera-facing sprite at every angle.

### Disposition

**Exploration/Q1 candidate.** No stacking or directional-sprite schema, runtime
selection, renderer behavior, or editor UX is approved by this note.

## Recommended order

1. Investigate the flicker with measurements before changing timing code.
2. Specify and implement add-frame-as-copy as the smallest independent convenience.
3. Define the object/entity sprite creation/editing workflow and its transaction
   boundaries, coordinating with R12 only if shared UI work is actually required.
4. Research stacking and multiple-angle sprites together only far enough to identify
   their interaction, then split implementation increments if their ownership or
   rendering models differ.

## Acceptance impact

R11 I6 manual acceptance passed despite these observations because I6 delivered its
approved format, painter, frame-management, preview, Save/Discard, and persistence
objectives. This document is the authoritative follow-up list for the four recorded
animation/sprite improvements.