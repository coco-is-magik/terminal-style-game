# Cppcheck Gate Recovery — 2026-09-16

## Outcome

**Real cppcheck and the complete canonical `make check` gate now pass.** The installed
tool is `/usr/bin/cppcheck`, version 2.18.2.

## Initial retry and findings

The first `make -j2 check` retry progressed beyond the former missing-tool result and
returned `FAIL-PRODUCT`. Cppcheck reported four `uninitvar` warnings in
`scene_format_serialize()` for count-bounded pointer arrays passed to `qsort`:

- decal instances;
- sprite instances;
- triggers;
- object instances.

The arrays were populated for every validated entry before sorting, so the diagnostics
were path-analysis false positives for valid candidates. The narrow correction explicitly
zero-initializes all five bounded serialization pointer arrays, including lights. This
does not change valid counts, ordering, allocation, canonical bytes, formats, or APIs; it
also leaves every unused stack slot in a defined state if validation is later changed.

After that correction, cppcheck 2.18.2 emitted only two informational analysis-scope
IDs, but its `--error-exitcode=100` option also applies to information messages:

- `normalCheckLevelMaxBranches` — normal analysis intentionally limits branch expansion;
- `toomanyconfigs` — normal analysis checks a bounded set of preprocessor configurations.

The gate now suppresses exactly those two information IDs. It does not use wildcard,
severity-wide, warning, style, performance, portability, or error suppression. It does
not add `--force` or `--check-level=exhaustive`, because either would change the accepted
analysis scope and runtime rather than make the existing gate compatible with the
installed version.

`check-static-analysis-policy` is now part of `make standards`. It requires C11,
`--error-exitcode=100`, and the two named information suppressions, and rejects broad
defect-category suppressions.

## Verification

- `cppcheck --version`: `Cppcheck 2.18.2`.
- Focused strict `build/test-scene-format`: compiled under
  `-Wall -Wextra -Wpedantic -Werror`.
- `./build/test-scene-format`: **24/24 passed**.
- `make style check-static-analysis-policy`: **passed**.
- `make standards`: **passed**, including real cppcheck and every standards-core guard.
- First serial `timeout 120s make check`: `FAIL-TIMEOUT` after the complete functional
  suite passed while cppcheck was still running. This is incomplete aggregate evidence,
  not a product failure or pass.
- Bounded `timeout 120s make -j2 check` retry: **passed** with strict application build,
  complete functional suite, real cppcheck, policy guard, and all standards-core guards.
- `git diff --check`: **passed**.

No sanitizer rerun was required: the production change only initializes previously
unused stack-array slots, and the complete sanitizer suites for the underlying serializer
and application state remain recorded. The focused serializer suite and complete
functional aggregate protect canonical serialization behavior.