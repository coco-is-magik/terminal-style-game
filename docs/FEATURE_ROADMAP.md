# Feature Roadmap

## Purpose and authority

This is the current roadmap authority. It summarizes verified foundation phases,
states the next safe sequencing direction, and points to archived evidence.

Detailed historical roadmap text was preserved at
[`archive/roadmap/FEATURE_ROADMAP_LONG_R0_R12_2026-09-11.md`](archive/roadmap/FEATURE_ROADMAP_LONG_R0_R12_2026-09-11.md).

For current implementation status, see [`CURRENT_STATUS.md`](CURRENT_STATUS.md).
For unordered deferred ideas, see [`TODO.md`](TODO.md).

## Phase status vocabulary

- **Verified foundation**: implemented and covered by named verification in the
  preserved closeout/history records.
- **Current cleanup**: documentation and authority cleanup that does not change
  runtime behavior.
- **Future work**: not yet implemented; ordering here is guidance, not a promise.
- **Deferred inventory**: captured in `TODO.md` but not sequenced here.

## Verified foundation summary

| Phase | Status | Current meaning | Evidence |
|---|---|---|---|
| R0 | Verified foundation | Repository/editor/UI baseline remediation and accessibility/performance fixes. | [`archive/r0/`](archive/r0/) |
| R1 | Verified foundation | Native-scene direction and early schema decisions. | [`archive/r1/`](archive/r1/) |
| R2 | Verified foundation | Native scene V1 implementation foundation. | [`archive/r2/`](archive/r2/) |
| R3 | Verified foundation | Typed editor-domain seams and implementation records. | [`archive/r3/`](archive/r3/) |
| R4 | Verified foundation | Authored surfaces, construction, and native-scene V2/V3 evolution. | [`archive/r4/`](archive/r4/) |
| R5 | Verified foundation | Asset identity and reusable asset-document foundations. | [`archive/r5/`](archive/r5/) |
| R6 | Verified foundation | Decal/light authoring foundation. | [`archive/r6/`](archive/r6/) |
| R7 | Verified foundation | Asset/editor integration continuation. | [`archive/r7/`](archive/r7/) |
| R8 | Verified foundation | Structural editing and vertical-world constraints. | [`archive/r8/`](archive/r8/), [`reviews/2026-08-21-roadmap-r8-phase-closeout.md`](reviews/2026-08-21-roadmap-r8-phase-closeout.md) |
| R9 | Verified foundation | Optical rendering research and native-scene V6 work. | [`archive/r9/`](archive/r9/), [`reviews/2026-08-26-roadmap-r9-implemented-phase-closeout.md`](reviews/2026-08-26-roadmap-r9-implemented-phase-closeout.md) |
| R10 | Verified foundation | Expanded lighting and native-scene V7 work. | [`archive/r10/`](archive/r10/), [`reviews/2026-08-27-roadmap-r10-closeout.md`](reviews/2026-08-27-roadmap-r10-closeout.md) |
| R11 | Verified foundation | Sprites, animation, objects, triggers, and related editor work. | [`archive/r11/`](archive/r11/), [`reviews/2026-09-04-roadmap-r11-closeout.md`](reviews/2026-09-04-roadmap-r11-closeout.md) |
| R12 | Verified foundation | Authored game-flow and responsive UI/menu-authoring foundation. | [`archive/r12/`](archive/r12/), [`reviews/2026-09-11-roadmap-r12-closeout.md`](reviews/2026-09-11-roadmap-r12-closeout.md) |

## Current cleanup sequence

| Pass | Status | Scope |
|---|---|---|
| Pass 1 | Done | Added docs index/current status and fixed stale references. |
| Pass 2A | Done | Rewrote architecture documentation as current-state authority. |
| Pass 2B | Done | Created `docs/archive/` structure and moved obvious historical clusters. |
| Pass 2C | Done | Archived the long roadmap and replaced it with this concise authority. |
| Pass 2D | Done | Archived the long handoff and replaced it with a current-status pointer. |

## Recommended next roadmap direction

1. Keep top-level `docs/` limited to current authority, active inventories, stable
   standards, and small compatibility pointers.
2. Use `CURRENT_STATUS.md` as the concise handoff/current-state source.
3. Use this file for sequenced roadmap summaries only.
4. Use `TODO.md` for unordered future/deferred work without implying priority.
5. Preserve new historical plans and implementation records under
   `docs/archive/` once they stop being current authority.

## Not currently sequenced here

The following are known but intentionally not prioritized in this roadmap summary:

- Native-scene Start Game migration from the deprecated legacy map path.
- Further editor/game UX improvements beyond the verified R12 foundation.
- TODO inventory triage and grouping beyond preserving it as non-roadmap work.
