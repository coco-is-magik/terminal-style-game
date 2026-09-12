# R11 I5 Sprite Animation — Decision Record — 2026-09-03

## Status

**Runtime/data scope Verified on 2026-09-03.** Automated gates and manual visual
acceptance passed. Timeline and animated-folder authoring are a later increment.

## Locked decisions

1. Animation is strictly in the sprite domain. It does not animate object
   transforms or introduce a generic animation/component framework.
2. Existing `assets/sprites/<id>.txt` files remain static sprite assets. An
   animated sprite uses `assets/sprites/<id>/animation.txt` and ordinary sprite
   files in that folder. One numeric ID cannot use both representations.
3. Scene sprites and object instances continue to reference the same numeric
   sprite ID. No scene schema/version change is needed.
4. The manifest requires `fps=0.1..120`, defaults `loop` to `true`, and contains
   one to 256 repeated `frame=<local-name>.txt` entries in playback order.
   Unknown/duplicate scalar keys, duplicate frames, path components, malformed
   frame files, and static/folder conflicts reject that sprite ID.
5. Playback uses elapsed seconds. Each runtime instance owns transient current
   frame and remainder time, starts at frame zero after runtime construction, and
   advances independently. Playback phase is not scene-authored or persisted.
6. The renderer owns no time. It consumes the current resolved frame selected by
   the narrow animation player.
7. Pattern Load may assign an animated folder to a selected instance and then
   closes the static-only Pattern menu for world preview. Opening Pattern on an
   already animated instance rejects the static painter. Animated timeline/folder
   authoring must be planned after runtime playback is verified.

## Explicitly deferred

- Timeline/folder creation, frame add/remove/reorder, and FPS editing UI
- Multiple named clips or state machines
- Per-frame durations, events, blending, ping-pong, and gameplay callbacks
- Persisted playback phase or per-instance playback-rate overrides
- Object-transform animation

## Spawn scope decision

Game-mode spawn expansion is deferred and unnecessary for R11 closeout: the
project has no game modes and the editor is not mature enough to specify or test
mode-dependent spawn selection meaningfully. The existing single authored spawn
remains the verified baseline.