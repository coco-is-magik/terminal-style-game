# V1-0 C1 Final-Newline Remediation — 2026-09-14

## Status

**Partially implemented and locally verified; required-profile acceptance blocked.**

C1 corrected every parent-repository first-party production/test C file identified by
the B0 newline inventory. Four affected SMC files are image-owned pinned dependency
inputs rather than parent-repository files and require a new reviewed SMC revision.
The required Ubuntu Clang profile could not run because the Docker daemon socket was
unavailable.

## Requirements preserved

- Strict C11 `-Wall -Wextra -Wpedantic -Werror` remains unchanged.
- No warning was suppressed and no source/test runner was omitted.
- No C behavior, API, format, ownership, or SMC algorithm was changed.
- Each delivered source change is exactly the original byte sequence followed by one LF.
- The default shipping build still selects `USE_SMC_STREAM_STATE_TRACKER=1`.
- The complete functional aggregate and focused SMC owners remain green.
- A pre-existing nested-SMC modification to `src/c/smc_state.c` was not modified.

## Pre-edit evidence

The tracked manifest is
[`2026-09-14-v1-0-c1-final-newline-pre.tsv`](2026-09-14-v1-0-c1-final-newline-pre.tsv).
It records path, original byte size, and original SHA-256 for all 192 files found by
the B0 scope:

```text
scoped C/header files: 254
missing final LF: 192
manifest SHA-256: 3a39d2cf7f39f39f08f0f8ea023d3a2e58c463954f8c92ab011b37d78cc5a9e0
```

The scope was regular `.c` and `.h` files directly below `src/`, `tests/`,
`vendor/src/smc/include/`, and `vendor/src/smc/src/c/`.

## Implementation

The files were processed in bounded batches of at most 20. Every batch rejected a
file unless its size and SHA-256 still matched the manifest and it lacked final LF.
After writing, each file had to be exactly one byte larger, end in LF, and retain the
manifest SHA-256 for all preceding bytes.

### Parent-repository result

- 188 tracked files under `src/` and `tests/` received exactly one final LF.
- All 173 scoped `src/` files now end in LF.
- All 73 scoped `tests/` files now end in LF.
- The parent repository's changed C-file set exactly matches those 188 manifest paths.

### Pinned SMC dependency result

The remaining four manifest paths are owned by the separate ignored repository at
`vendor/src/smc`:

- `include/smc.h`
- `src/c/smc_artifact.c`
- `src/c/smc_artifact.h`
- `src/c/smc_runtime_stub.c`

They were initially given the same byte-exact correction and passed verification.
Subsequent dependency-boundary inspection established that platform profiles exclude
the local `vendor/` tree and reconstruct SMC from pinned commit
`3783ae976b13bf3e1d6bfa437d2927509871ef30` and archive SHA-256
`836e9c6bd41e7f0afb5f4b22d3841c5f2129dbdf63f35005ce4ef5917a18c8cd`.
Local nested-repository edits therefore cannot remediate the required profile.

The four LF additions were safely reverted only after verifying that each file was
exactly its manifest bytes plus LF. All four now match their original manifest size
and SHA-256. The unrelated pre-existing nested change remains:

```text
M src/c/smc_state.c
```

C1 cannot be complete until an approved pinned SMC revision contains equivalent
strict-Clang-compatible source and the dependency commit/hash are updated through the
normal dependency review process. Editing generated output, patching fetched sources
during image construction, or pretending ignored local vendor files are deliverable
is rejected.

## Execution failures preserved

### Missing helper invocation

Five planned batch commands initially referenced a helper script that had not been
created. Python stopped before opening any target file, so no batch mutation occurred.
A temporary helper was then created under `/tmp`, inspected, used for the remaining
bounded batches, and removed. No temporary helper remains in the repository or process.

### Independent proof completion not observed

One combined proof command did not report observable completion through shell
integration. Its result was not accepted. It was replaced by smaller independent
proofs for byte identity, newline coverage, parent changed-set equality, and nested
repository ownership; each completed and passed.

### Pager process cleanup

One nested `git diff --numstat` inspection unintentionally opened a pager and left the
exact `git` and `less` processes alive. Those two inspection processes were terminated
by PID. A later process check found no matching process. No product process was affected.

### Required Ubuntu Clang profile blocked

`make platform-test-ubuntu-clang` generated:

```text
PROFILE_ID=ubuntu-24.04-clang
PROFILE_ROLE=required
PHASE=container-start
OUTCOME=FAIL-PRODUCT
REASON=contained-product-failure
STATUS=1
```

The complete one-line Docker log was:

```text
failed to connect to the docker API at unix:///var/run/docker.sock; check if the path is correct and if the daemon is running: dial unix /var/run/docker.sock: connect: no such file or directory
```

The container did not start and no product compiler ran. Under
[`../VERIFICATION_POLICY.md`](../VERIFICATION_POLICY.md), the evidence is therefore
**FAIL-TOOL: Docker daemon/runtime unavailable**, not `FAIL-PRODUCT`.

`tools/platform-profiles/run-one.sh` currently maps Docker status 1 to an in-container
product failure when no container-written result exists. Correcting daemon-connect
classification requires a separate tested platform-harness increment; it is not folded
into this mechanical C1 change.

## Verification

Final deliverable state:

| Check | Result |
|---|---|
| Manifest byte-prefix proof | `PASS`: 188 parent files are original bytes plus one LF; four SMC files restored exactly |
| Parent newline scan | `PASS`: `src` 173/173 and `tests` 73/73 end in LF |
| Parent changed-set proof | `PASS`: exactly 188 manifest paths |
| Parent `git diff --check` | `PASS` |
| Nested SMC `git diff --check` | `PASS` |
| Strict default GCC application build | `PASS`, default SMC stream mode |
| Strict local Clang 22 application build | `PASS`, default SMC stream mode |
| Focused `test-smc-state-tracker` | `PASS`, 1/1 |
| Focused `test-smc-indexed-state-tracker` | `PASS`, 1/1 |
| `make standards-core` | `PASS` |
| Complete 62-runner `test-build` | `PASS` through bounded staged continuation |
| Complete `make test` | `PASS` |
| Required Ubuntu Clang profile | `FAIL-TOOL`, Docker daemon unavailable; no compiler evidence |

The first clean `test-build` attempt reached a 120-second harness timeout while
compiling `test-asset-refresh`; no compiler error occurred. It was not rerun from
scratch. A narrower continuation compiled only remaining prerequisites and passed,
then the complete aggregate passed. The timeout remains recorded as `FAIL-TIMEOUT`.

## Remaining work and next safe action

1. Obtain or create a reviewed SMC revision equivalent to the pinned dependency plus
   final-LF corrections, then update its pinned commit and archive SHA-256 through the
   dependency process.
2. Correct and test platform classification so a Docker daemon-connect failure cannot
   be emitted as a contained product failure.
3. Restore an operational Docker daemon and rerun the required Ubuntu Clang profile.
4. Only after that profile passes may C1/C2 be closed.
5. Keep the independent P1 surface-render budget failure open; newline remediation
   does not change or waive it.

Do not repeat the Ubuntu profile until Docker availability is confirmed. Do not edit
the ignored local SMC clone as a substitute for a pinned dependency revision.