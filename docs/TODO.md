# Project Follow-up TODO

This list records intentionally deferred work so completed plans and historical
handoffs do not become accidental backlogs. Items are unordered within each
section unless a dependency is stated.

## Unified editor product roadmap (deferred)

- [ ] Add map-cell construction and deletion through undoable commands.
- [ ] Design a format/model for per-face wall materials before implementing UI.
- [ ] Add floor and ceiling selection/editing.
- [ ] Add material creation, modification, naming, and deletion.
- [ ] Integrate `decal_painter` into the unified editor with SDL UI/input while
      reusing `decal_io` for persistence.
- [ ] Add decal and sprite placement in the in-world editor.
- [ ] Add animation authoring.
- [ ] Add placed objects, lights, triggers, and spawn-point editing.
- [ ] Introduce stable entity IDs before persistent placed-entity editing.

## Benchmark methodology (deferred)

- [ ] Add percentile telemetry (at least p95/p99) instead of relying on the
      current single-outlier heuristic.
- [ ] Exclude a documented warm-up window from average render time as well as
      worst-frame collection.
- [ ] Emit measured/effective FPS directly in benchmark JSON.
- [ ] Keep representative raycast acceptance and full-change stress diagnostics
      as clearly distinct targets/results.
- [ ] Add regression coverage for benchmark result classification.
- [ ] Add regression coverage for tracker selection: default stream, explicit
      no-tracker baseline, and conflicting-mode rejection.

## Documentation maintenance

- [x] Establish a stable editor requirements/regression contract and mark old
      plans/handoffs as historical or superseded.
- [ ] When editor behavior changes, update
      `docs/EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md`, tests, and `README.md`
      in the same change.
- [x] Preserve historical measurements and failed attempts while pointing stale
      plans/handoffs to current authoritative records.
