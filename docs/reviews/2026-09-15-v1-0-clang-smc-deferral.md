# V1-0 Clang Diagnostic Continuation and SMC Deferral — 2026-09-15

## Outcome

Ubuntu Clang is now an informational strict diagnostic profile. The four known SMC
final-newline errors remain nonzero and unsuppressed, but no longer block V1-0. Because
this project also owns SMC, the trivial source correction and dependency pin update are
scheduled for V1-20 dependency reconciliation.

## Non-masking safeguard

The canonical `make CC=clang all` command remains unchanged and authoritative. If it
fails, the profile runs a diagnostic-only continuation:

```text
make -k CC=clang \
  CFLAGS="-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -ferror-limit=0" \
  all test-build
```

The continuation never converts failure into pass. Its classifier accepts known debt
only when every compiler error is `-Werror,-Wnewline-eof` in all four exact paths:

- `vendor/src/smc/include/smc.h`;
- `vendor/src/smc/src/c/smc_artifact.c`;
- `vendor/src/smc/src/c/smc_artifact.h`;
- `vendor/src/smc/src/c/smc_runtime_stub.c`.

Any other warning category, first-party path, dependency path, missing known path, empty
log, successful sweep, or malformed/incomplete diagnostic set is marked
`additional-or-inconclusive-clang-diagnostics`.

## Preserved requirements

- `-Werror` remains enabled.
- No `-Wno-*` suppression is used.
- No source or test target is omitted from the diagnostic sweep.
- The downloaded SMC source is not patched during profile setup.
- The canonical strict result remains truthful and nonzero.
- Required Linux GCC profiles and scoped native Windows GCC verification are unchanged.

## Verification

Deterministic fixtures cover the exact known set, first-party additional diagnostics,
another warning category in a known SMC file, missing known files, empty/successful sweep,
Clang-only invocation, unchanged strict-result forwarding, no suppression, and
informational-profile behavior under `platform-check`.