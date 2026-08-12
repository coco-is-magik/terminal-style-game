# R4 Manual-Review Follow-up Implementation Record — 2026-08-12

## Status

The automated implementation is complete. R4 remains **Active** until the amended
interactive checklist in `reviews/2026-08-11-roadmap-r4-targeted-review.md` passes.

## Delivered

1. Surface inspectors use Enter/Esc hierarchy, one active arrow, persistent green
   context, inline ambient entry, and reusable unavailable construction rows.
2. Floor/ceiling hover and selection use border-only `.`/`#` outlines and share
   horizontal projection with selection, preserving authored material visibility.
3. Wall removal cascades attached wall decals in the same exact undo/redo command.
4. Native v3 persists east/south growth triggers. East/south removal copies the prior
   edge outward; refill shrink is provenance-, content-, and dimension-safe.
5. The surface benchmark compares no decal with a fully wall-occluded decal and
   requires identical frame checksums.

Hover visibility remains a separate research item in `TODO.md`; this follow-up did
not reintroduce a material-obscuring filled overlay.

## Automated evidence

| Gate | Result |
|---|---:|
| Scene format | 15/15 passed |
| Scene document | 39/39 passed |
| Command system | 30/30 passed |
| Unified editor | 53/53 passed |
| Strict aggregate `make check` | passed |
| Full ASan | passed |
| Full UBSan | passed |
| Feature matrix | 8/8 passed |
| Surface benchmark | 1.649513 ms authored; 1.626518 ms occluded decal; passed |
| Surface stability | 1.623746 ms authored; 1.614445 ms occluded decal; passed |
| Obstructed/no-decal checksum | exact equality (`5602340901454607159`) |

Strict flags remain `-std=c11 -Wall -Wextra -Wpedantic -Werror`. The final normal
`make check` restored the non-sanitized build.

## Remaining gate

Run the amended real-video checklist. Record date, environment, and findings there.
Only a successful pass permits R4 to become Verified.
