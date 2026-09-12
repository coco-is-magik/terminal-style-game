# Documentation Index

This index is the starting point for project documentation. The repository keeps
historical plans, implementation records, reviews, and handoffs as evidence, but
only a small set of documents should be treated as current authority.

## Start here

- [`../README.md`](../README.md) — project overview, build/run basics, controls,
  asset-system overview, and maintainer entry points.
- [`CURRENT_STATUS.md`](CURRENT_STATUS.md) — concise current state, verified
  foundation, next safe action, and known deferred source-level TODOs.
- [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md) — concise roadmap authority,
  verified phase summary, current cleanup status, and next sequencing guidance.
- [`TODO.md`](TODO.md) — unordered future-feature ideas, deferred work, and design
  questions. This is not a roadmap or priority order.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — current module boundaries, ownership
  rules, data flows, persistent-data boundaries, and verification ownership.

## Current authority

- [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md) defines verified roadmap
  foundations, current cleanup status, and future sequencing guidance.
- [`TODO.md`](TODO.md) is the complete unordered inventory for future/deferred
  work. Items must be promoted into focused requirements/planning before
  implementation.
- [`EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md`](EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md)
  is the accepted unified-editor behavior and regression contract until a planned
  and tested change deliberately revises it.
- [`reviews/2026-09-11-roadmap-r12-closeout.md`](reviews/2026-09-11-roadmap-r12-closeout.md)
  records the latest verified roadmap closeout evidence.

R0–R12 are verified roadmap foundations. They should not be read as a claim that
the editor or game-making experience is feature-complete.

## Stable technical references

- [`C_STYLE_AND_OWNERSHIP.md`](C_STYLE_AND_OWNERSHIP.md) — C style, ownership,
  and resource-lifetime expectations.
- [`ERROR_CATALOG.md`](ERROR_CATALOG.md) — known error categories and handling
  notes.
- [`../assets/README.md`](../assets/README.md) — asset file formats and data
  layout.

## Reviews, handoffs, and historical records

- [`reviews/`](reviews/) contains roadmap reviews and phase closeouts.
- [`archive/`](archive/) contains historical phase plans, implementation records,
  investigations, archived roadmap text, and superseded handoffs.
- [`handoff.md`](handoff.md) is retained as a compatibility pointer. Use
  [`CURRENT_STATUS.md`](CURRENT_STATUS.md) as the current handoff authority.
- Top-level `docs/` is reserved for current authority, stable standards, active
  inventories, and small compatibility pointers.

## Documentation update rules

- Keep current authority small and easy to find.
- Archive or supersede historical documents instead of deleting them.
- Do not leave multiple documents claiming current authority for the same
  behavior.
- Record verification evidence with exact commands, reviews, or manual checks
  before calling behavior Verified.
- Update [`CURRENT_STATUS.md`](CURRENT_STATUS.md), [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md),
  and [`TODO.md`](TODO.md) according to their separate roles instead of mixing
  roadmap order, future ideas, and active handoff notes.