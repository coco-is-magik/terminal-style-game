# V1-1 Provisional UI Theme Policy Increment — 2026-09-16

> **Superseding amendment:** The no-consumer status below records this increment's
> original boundary. The first bounded production consumer was subsequently implemented
> in
> [`2026-09-16-v1-1-application-menu-palette-adapter.md`](2026-09-16-v1-1-application-menu-palette-adapter.md).
> The current static palette values and D6 motion vocabulary were subsequently manually
> approved. Motion integration remains separately owned by V1-3 under
> [`2026-09-16-v1-1-ui-motion-demo.md`](2026-09-16-v1-1-ui-motion-demo.md).

## Outcome and status

The isolated V1-1 `ui_theme` value/policy seam is implemented and automatically
verified. **At the time of this increment, every D1-D7 value remained provisional until
visible and interactive review.** The superseding amendment above records the later static
palette and D6 approval; automated agreement alone was not final product acceptance.

No production adapter consumes this module. The application, editor, authored Menu
runtime, persisted UI documents, assets, preferences, rendering, input, and visible
behavior are unchanged. V1-3 remains blocked, and no visible V1-1 component migration is
authorized by this increment.

## Implementation

Added `src/ui_theme.h` and `src/ui_theme.c` as a pure, allocation-free module with no
SDL, document, I/O, persistence, input, global-time, or mutable-global dependency.

The module contains provisional:

- immutable semantic palette values;
- editor/authored-template geometry and context-policy values;
- exact WCAG text/non-text contrast thresholds;
- opaque RGBA resolution and WCAG sRGB contrast calculation;
- overflow-safe shared scaled-edge calculation;
- deterministic dominant-state precedence;
- motion-role durations;
- explicit-time cubic ease-out progress;
- interpolation from the current resolved value for interruption/reversal;
- immediate reduced-motion resolution.

Invalid calculations preserve caller output. Unknown or translucent contrast inputs must
be resolved against a known opaque background before contrast evaluation.

Added `tests/test_ui_theme.c` and registered `test-ui-theme` in both the canonical
`make test` inventory and `make test-ui-standards`.

## Focused coverage

The seven tests protect:

1. exact provisional token values and stable immutable access;
2. known WCAG ratios, the 4.5:1/3:1 floors, and adjacent no-rounding threshold cases;
3. alpha resolution plus transactional invalid-output behavior;
4. the complete dominant-state precedence chain;
5. deterministic 100/125/150/200% edge scaling, flooring, overflow, and invalid input;
6. exact motion roles, cubic easing, endpoint behavior, invalid time, and reduced motion;
7. interruption/reversal from the current resolved value with transactional failures.

## Development failures and corrections

1. The first strict test compile failed because a compound literal containing commas was
   passed directly to cmocka's `assert_memory_equal` macro. A named const array replaced
   the macro-ambiguous expression; no product code was involved.
2. The first contrast boundary fixture incorrectly expected gray 118 on white to fail
   4.5:1. The unrounded implementation correctly showed it passes. Adjacent values now
   lock the real boundary: gray 118 passes and gray 119 fails.
3. Contract review after the first 5/5 pass found that scaled-edge and interruption
   behavior were documented but not owned by the new seam. Transactional helpers and two
   tests were added, producing the final 7/7 suite.

## Verification

- Strict focused compile under `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`: pass.
- `./build/test-ui-theme`: **7/7 passed**.
- `make -j2 test-ui-standards`: pass across all ten focused UI owners.
- `make standards`: pass with real cppcheck 2.18.2 and all repository guards.
- Focused ASan/LeakSanitizer: **7/7 passed**, no diagnostics.
- Focused UBSan: **7/7 passed**, no diagnostics.
- `make -j2 test`: complete functional aggregate pass with the new runner registered.
- `make -j2 check`: pass; strict application, complete functional suite, real cppcheck,
  static-analysis policy, and standards-core all passed.
- `git diff --check`: pass.

No benchmark was required because the module is not linked into or called from production
or any render loop. The focused sanitizer results do not silently upgrade the earlier
complete-aggregate sanitizer evidence.

## Mandatory manual approval gate

Before any adapter or visible component migration, manually evaluate representative
mockups or a deliberately bounded first consumer at 100%, 125%, 150%, and 200%, including:

- palette legibility and terminal-retrofuturist character;
- focus, selection, pressed, disabled, warning, error, success, and destructive states;
- non-color marker clarity;
- 3x3-cell pointer target feel and density;
- 260x160 editor and 40x15/60x20/80x25 authored-template layouts;
- major-context enter/exit and local feedback timing;
- interruption/reversal behavior;
- reduced motion with static continuity cues.

Manual review may approve, revise, or reject any provisional token. A passing automated
suite does not constrain that product decision.

## Next safe action

Q2 architecture review passed after bounded non-visible corrections in
[`2026-09-16-v1-1-q2-policy-architecture-review.md`](2026-09-16-v1-1-q2-policy-architecture-review.md).
The next increment is the selected application-menu focus/selection palette adapter and
manual evaluation boundary. Do not broaden that migration, finalize provisional rules,
migrate authored documents, add persistence, or release V1-3 before manual approval.