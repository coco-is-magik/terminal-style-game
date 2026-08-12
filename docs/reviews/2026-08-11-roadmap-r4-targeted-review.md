# Targeted Review — R4 Surface Data and Basic World Construction

**Review date:** 2026-08-11  
**Review type:** Targeted R4 architecture and phase-gate review  
**Disposition:** No architecture blocker; automated gates and amended interactive acceptance pass; R4 Verified

## Summary

R4 Increments A–F and the manual-review follow-up are implemented. The representative
workflow, all automated closeout gates, and the amended real-video checklist pass. The
targeted review found no duplicate authored map, controller ownership expansion, or
cross-domain format/runtime coupling, so escalation to a full Q4 review was not
required. R4 was marked Verified on 2026-08-12.

## Architecture findings

### Ownership — pass

`SceneFormatCandidate` temporarily owns parsed/migrated cells before transactional commit.
Ownership then transfers to `SceneDocument`, which is the sole authoritative owner.
`Map`, `WorldState`, light maps, and renderer caches are derived. `SceneSurfaceView` is a
const borrowed pointer/count/dimension view and performs no allocation or ownership.

### Dependency direction — pass

Only `scene_document.c` couples document persistence to `scene_format`. The renderer
depends on `surface_view.h` and authored value types, not `SceneDocument` or format code.
The application composes the borrowed view at the render boundary. Format, document,
runtime adapter, renderer, and controller responsibilities therefore remain directional.

### Mutation boundary — pass

`command_system` remains the sole production caller of `scene_document_internal_*`
mutations. `editor_domain` creates typed requests; `unified_editor` routes inputs,
executes history, and transactionally rebuilds runtime state. Construction did not turn
appearance commands into geometry commands or broaden controller ownership.

### Compatibility and failure atomicity — pass

Native v1 and legacy inputs migrate to complete v2 candidates without source rewrite.
Missing references remain visible and Save-blocking. Existing malformed input, OOM,
runtime-build, durable-save, dirty prompt, and failed-switch regressions remain green.

## Findings and disposition

| Severity | Finding | Disposition |
|---|---|---|
| Blocker | None in architecture or automated behavior | Closed |
| Documentation | README deferred list still named completed construction and horizontal surface work | Fixed during Increment F closeout |
| Acceptance | Human-observed R4 visual/input workflow | Closed 2026-08-12; amended checklist passed |

## Automated evidence

- Strict aggregate suite: passed.
- Focused fixture/document suite: 38/38 passed.
- Focused unified-editor suite: 53/53 passed.
- ASan and UBSan full suites: passed without diagnostics.
- Feature matrix: 8/8 passed.
- Deterministic surface and editor-highlight benchmark/stability gates: passed.
- Dummy SDL smoke and strict application build: passed.
- Detailed numbers: `../R4_INCREMENT_F_IMPLEMENTATION_RECORD_2026-08-11.md`.

## Interactive acceptance checklist — required

Run in a real SDL video environment:

```sh
make clean && make
./build/ascii-fps
```

1. Open `r4_surface_workflow.tscene`. Confirm floor and ceiling regions visibly use
   different materials, ambient and the point light shade them, and wall/floor/ceiling
   decals remain correctly ordered and occluded.
2. Confirm floor/ceiling hover and selection use border-only `.`/`#` outlines and the
   material remains visible. Use Enter to open/apply Material, Esc to return, and verify
   only the active row has an arrow while persistent context remains green.
3. Edit wall/floor/ceiling materials and ambient. Place/remove a safe wall. Confirm
   west/north Remove Wall is visible but unavailable. Remove east and south boundary
   walls; confirm the previous edge is copied outward, then refill/contract and undo/redo.
4. Remove the decal-bearing wall at `(4,2)`. Confirm its wall decal is removed and
   undo restores both wall and decal exactly. Spawn/current-player refusals remain safe.
5. Save As, restart, and reopen. Confirm materials, ambient, growth provenance,
   occupancy, light, decals, and camera-safe spawn persist. Also open a v1 scene and
   import a legacy map; confirm migration/Save As behavior remains understandable.
6. Continuously alternate wall, light, floor, and ceiling selection/editing. Exercise
   Escape, dirty Reload, dirty Open/New, Save/Discard/Cancel, and UI scale presets;
   confirm no visible corruption, stale inspector, hitch, or interaction degradation.

Record the date, environment, and each item as pass/fail below. Any failure reopens R4.

**Interactive result:** Passed
**Environment:** Real SDL video session (user-run)
**Findings:** All six amended items passed; west/north removal visibly unavailable,
east/south copy-growth and refill-shrink confirmed, decal cascade and exact undo/redo
confirmed, border-only highlights preserved material visibility, and no corruption or
interaction degradation observed. Hover-outline visibility remains a tracked research
item (`../TODO.md`), not an acceptance blocker.

## Conclusion

The targeted architecture review recorded no blocker and did not escalate to Q4.
Increment F, the manual-review follow-up, and the amended interactive checklist are
complete. **R4 is Verified.**
