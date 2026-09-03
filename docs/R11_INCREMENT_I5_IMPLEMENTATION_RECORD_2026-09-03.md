# R11 Increment I5 Implementation Record — Sprite Animation Runtime — 2026-09-03

## Status

**Verified on 2026-09-03.** Automated verification and manual visual acceptance
passed. Timeline and animated-folder authoring remain a separate R11 increment.

## Delivered behavior

- `AssetRegistry` owns bounded ordered animation frames and playback metadata.
- `asset_loader` discovers noncontiguous numeric sprite files/folders, validates
  strict animation manifests, reuses the existing frame parser, and rejects
  ambiguous file/folder identity.
- `sprite_animation_player` deterministically advances each `SpriteEntity` from
  elapsed seconds, with loop and terminal-frame behavior.
- The renderer resolves the instance's current frame without owning time.
- Normal world rendering and unified-editor preview tick playback. Runtime rebuild
  resets phase to frame zero; no authored scene state changes.
- Existing static sprites and numeric scene/object references remain compatible.
- The static sprite document refuses animated IDs so Save cannot create a
  conflicting `<id>.txt` beside `<id>/`.
- Bundled sprite ID 4 provides a two-frame 4 FPS manual-acceptance fixture.

## Focused automated evidence

- Sprite animation player: 3/3
- Asset refresh/loader: 6/6
- Sprite document: 3/3
- Sprite renderer: 6/6
- Unified editor: 84/84
- Strict application build: passed with `-Werror`
- Full strict `make test`: passed
- `make smoke` and `make check-current-renderer`: passed
- Build matrix: 8/8 variants passed
- Combined ASan+UBSan with leak detection: all five affected focused runners passed
- Sprite benchmark: baseline 2.280791 ms, 128 sprites 2.660765 ms, both below the
  6 ms gate; checksums remained deterministic at `1846712605617511097` and
  `5403392855886966484`

## Manual acceptance — passed 2026-09-03

The user manually accepted the complete I5 runtime/data checklist:

1. Sprite ID 4 can be assigned through Pattern Load or an object's Sprite picker.
2. Its wide/tall frames alternate at approximately the authored 4 FPS rate.
3. Separate instances animate without corrupting each other.
4. Mode changes and runtime rebuilds remain safe; rebuild starts at frame zero.
5. Opening animated ID 4 in the static Pattern painter reports an invalid sprite
   operation and does not create `assets/sprites/4.txt`.