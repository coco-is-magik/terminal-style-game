# Current Status

## Current verified foundation

R12 is Verified and closed as of 2026-09-11. R0–R12 are verified roadmap
foundations, not a claim that the editor or game-making experience is
feature-complete.

The latest closeout evidence is in
[`reviews/2026-09-11-roadmap-r12-closeout.md`](reviews/2026-09-11-roadmap-r12-closeout.md).
R12 implemented the authored game-flow and responsive UI/menu-authoring
foundation through I1–I17, including the post-review correction rounds. Final
manual hierarchy and Flow display-backed checks passed on 2026-09-11. Test-mode
pointer activation remains deferred; keyboard Enter is the accepted R12 Test-mode
activation path.

## Next safe action

No post-R12 implementation bucket is currently selected. Future implementation
should not begin until one deferred/future-work bucket is chosen, requirements are
narrowed, and a focused plan is written.

The current future-work inventory is [`TODO.md`](TODO.md). Roadmap sequencing and
phase status belong in [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md).

## Current authority

- [`README.md`](../README.md) — project overview and user/developer entry points.
- [`README.md`](README.md) — documentation index and authority map for this
  directory.
- [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md) — roadmap sequencing and phase
  status.
- [`TODO.md`](TODO.md) — unordered future/deferred work inventory.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — current architecture notes and data-flow
  ownership context.
- [`EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md`](EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md)
  — accepted unified-editor regression contract.

## Deferred source-level TODOs known after R12

- `src/assets.h`: sprite ID widening remains deferred.
- `src/lighting.c`: lighting cache key map/world revisions remain hard-coded to
  `1`.

## Current documentation cleanup status

Pass 1 added this current-state summary and the documentation index. Pass 2A
rewrote `ARCHITECTURE.md` as current-state authority. Passes 2B-2D created
`docs/archive/`, moved obvious historical phase/planning clusters, archived the
long roadmap, and demoted `handoff.md` to a compatibility pointer.

Historical phase records now live under [`archive/`](archive/). The long R0-R12
roadmap was preserved at
[`archive/roadmap/FEATURE_ROADMAP_LONG_R0_R12_2026-09-11.md`](archive/roadmap/FEATURE_ROADMAP_LONG_R0_R12_2026-09-11.md),
and the prior long handoff was preserved at
[`archive/handoffs/handoff_R12_docs_cleanup_2026-09-11.md`](archive/handoffs/handoff_R12_docs_cleanup_2026-09-11.md).