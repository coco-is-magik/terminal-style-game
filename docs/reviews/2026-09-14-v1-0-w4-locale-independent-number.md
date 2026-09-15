# V1-0 W4 Locale-Independent Number Conversion — 2026-09-14

## Outcome

**W4 is complete. `platform_number` now owns locale-independent ASCII `double` parsing and
canonical formatting on POSIX and UCRT, and `scene_format` no longer contains platform locale
types or calls. Linux and native Windows produce the same accepted `%.17g` corpus bytes.**

This increment changes no scene grammar, public scene result, diagnostic identifier,
serialization order, or ownership boundary.

## Requirements and preserved behavior

- Parse only the existing complete ASCII decimal grammar under C numeric rules.
- Reject malformed/trailing input, comma decimals, NaN/Inf text, overflow, underflow, and
  non-finite values.
- Preserve parse and format outputs on every failure.
- Format finite values with the existing canonical `%.17g` semantics.
- Normalize either signed zero to the exact byte `0`.
- Never mutate process-global locale state.
- Keep locale objects per-call with explicit creation and destruction.
- Keep `SceneFormatCandidate` and `SceneFormatBuffer` publication transactional.

## Implementation

### `platform_number`

`src/platform_number.h` exposes two stateless operations:

- `platform_number_parse_double`;
- `platform_number_format_double`.

`src/platform_number.c` validates grammar before conversion. POSIX uses
`newlocale`/`strtod_l`; canonical formatting temporarily selects a per-call C thread locale,
restores the prior thread locale before returning, and invokes no callback while selected.
Windows uses `_create_locale`, `_strtod_l`, `_snprintf_l`, and `_free_locale`. Neither path
calls process-global `setlocale` or shares a locale object.

Formatting first writes to a fixed 32-byte local buffer, sufficient for finite `%.17g`
output. Caller storage and length are updated only after successful conversion and capacity
validation. `src/platform_number_internal.h` supplies explicit per-call locale, parse, and
format fault modes without production-global failure flags.

### `scene_format` migration

`src/scene_format.c` delegates scalar and tuple floating parsing to `platform_number` and
delegates each canonical floating token to its bounded formatter. Scene diagnostics and
serialization-buffer ownership remain in `scene_format`. All former `locale_t`, `newlocale`,
`strtod_l`, `uselocale`, `freelocale`, and `LC_NUMERIC_MASK` uses were removed from that
domain module.

### Build and native harness

`SRC_PLATFORM_NUMBER` is included through `SRC_SCENE_FORMAT`, so all scene consumers link the
adapter exactly once. A new registered `test-platform-number` runner raises the inventory
from 63 to 64. The Windows preflight builds, PE/import-checks, and runs both the standalone
number corpus and `test-scene-format` before full product phases.

## Regression coverage

The five-test number owner covers:

- zero, negative zero, leading plus, omitted leading/trailing fractional digits;
- ordinary fractions and integers, 17-significant-digit values, exponent thresholds,
  minimum normal and maximum finite doubles;
- exact canonical bytes and binary round trips, with signed-zero normalization treated as
  the required exception;
- malformed, trailing, comma-decimal, NaN/Inf, overflow, and underflow input;
- non-finite formatting, short destinations, invalid arguments, and injected failures;
- unchanged parse/format outputs on failure; and
- unchanged process numeric locale after success and failure.

The existing 24-test scene owner continues to prove exact canonical scene bytes, canonical
round trips, finite/range diagnostics, negative-zero output, caller-locale preservation, and
transactional candidate/buffer behavior.

## Verification evidence

| Check | Result |
|---|---|
| Strict GCC number owner | `PASS`, 5/5 |
| Strict Clang number owner | `PASS`, 5/5 |
| Strict GCC scene owner | `PASS`, 24/24 |
| Strict Clang scene owner | `PASS`, 24/24 |
| Number ASan + LeakSanitizer + UBSan combined | `PASS`, 5/5, no report |
| Scene ASan + LeakSanitizer + UBSan combined | `PASS`, 24/24, no report |
| Number standalone UBSan | `PASS`, 5/5, no report |
| Scene standalone UBSan | `PASS`, 24/24, no report |
| Strict default GCC/SMC-stream application | `PASS` |
| Complete runner build | `PASS`, 64 runners |
| Complete `make test` | `PASS`, all 64 registered runners |
| UI standards aggregate | `PASS`, all nine owners |
| `make standards-core` | `PASS` |
| `make test-platform-harness` | `PASS` |
| `make smoke` | `PASS`, `{"smoke":"ok","map_width":10,"map_height":6}` |

Strict C checks used `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`. Focused sanitizer
checks used `-O1 -g`, frame pointers, and halt-on-error settings.

## Native Windows evidence

The configured run used the sole observed session domain:

```text
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

The final W4 preflight proved:

- `test-platform-number.exe` is PE x86-64 with no MSYS/Cygwin runtime import and passes 5/5;
- UCRT canonical bytes match the accepted Linux corpus, so no formatter fork or
  post-processing decision was needed;
- `test-scene-format.exe` is PE x86-64 with no MSYS/Cygwin runtime import and passes 24/24;
- the strict native application compiles successfully with zero W4 diagnostics; and
- QGA cleanup restored `win10-survey` from `shut off` to `shut off`.

The profile then stopped in `strict-test-build`: `test-deps` links the static Windows ENet
archive without `ws2_32`, producing unresolved Winsock imports. This is the next W5 build
policy/dependency-link issue, not a W4 failure. Later native test, standards, and full binary
inspection phases remained correctly unrun.

## Failures and corrections

1. The first adapter compile lacked `<stdlib.h>` for POSIX `strtod_l` and the four new files
   lacked final LF bytes. Both were corrected before execution evidence was accepted.
2. The first corpus asserted bitwise round trip for `-0`. That contradicted required
   negative-zero normalization; the assertion now requires positive zero after canonical
   reparse while retaining bitwise checks for every nonzero case.
3. Initial GCC/Clang builds targeted the same output concurrently, and an execution observed
   stale ordering. Final compiler-specific builds and runs were performed sequentially.
4. Removing `<errno.h>` from `scene_format.c` was too broad because integer growth-list
   parsing still uses `errno`; the header was restored without restoring locale coupling.
5. The first native invocation omitted the machine-local VM identity and returned
   `FAIL-MISSING-TOOL`. Read-only session enumeration found the sole documented
   `win10-survey` domain; configured bounded runs then supplied valid native evidence.

## Remaining risks and next safe action

1. Proceed to **W5 native Windows functional profile remediation**, beginning with the
   narrowly scoped Windows ENet/`ws2_32` link policy exposed by `test-deps`. Do not weaken
   tests or override flags only in the harness.
2. Local Clang application compilation remains blocked by the pre-existing first-party and
   pinned-SMC final-newline set owned by C1/C2; W4 files compile cleanly under Clang.
3. Sprite animation persistence still has its separately recorded locale-sensitive
   formatting path; W4's accepted migration target was `scene_format`, so that owner was not
   changed speculatively.
4. P1 performance and native display/input evidence remain independent V1-0 work.
