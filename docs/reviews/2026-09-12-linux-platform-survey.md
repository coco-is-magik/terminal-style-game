# Linux Platform Survey — 2026-09-12

## Objective and constraints

This increment implemented reproducible Docker profiles and ran the unchanged
project to discover compatibility failures. It intentionally made no changes to
`src/`, `tests/`, product behavior, file formats, warning policy, or runner scope.
Expected incompatibilities remained nonzero `FAIL-PRODUCT` evidence.

## Implemented profiles

| Profile | Role | Image ID | Result |
|---|---|---|---|
| Ubuntu 24.04 GCC | required | `sha256:fbe5f00eabe79151c7323ab12ad550825ec415ef22856ab5e18541819a0beb2f` | `PASS` |
| Ubuntu 24.04 Clang | required | `sha256:dfac85d333913b891ea1152da86b6308a4c79512c1046fe8bac5d6d8d52fc14c` | `FAIL-PRODUCT` at strict app build |
| Fedora 43 GCC | required | `sha256:c8a8a0461faaa30b167cf026680c07d4521c1ea1a868ea7482de719a3877cd78` | `PASS` |
| Alpine 3.22 musl GCC | informational | `sha256:a8bad2958b5ba8ed1efb9f2910724249beed97238221a8afe2480df4d9fedcd9` | `PASS` |

All images were amd64 and executed on the Gentoo 7.2.2 host kernel. Base-image
digests and five dependency source revisions/checksums are recorded in
[`../PLATFORM_TESTING.md`](../PLATFORM_TESTING.md).

## Survey result

`make platform-survey` built/used every image, continued after the Clang product
failure, and produced:

```text
ubuntu-24.04-gcc   required       PASS
ubuntu-24.04-clang required       FAIL-PRODUCT strict-app-build application-compile-failure
fedora-gcc         required       PASS
alpine-musl-gcc    informational  PASS
```

For each passing profile, strict application compilation, the complete 62-runner
build, complete `make test`, and `standards-core` passed under
`-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`.

`make platform-check` correctly failed because required Ubuntu Clang did not pass.
The informational Alpine result did not weaken or alter required-profile semantics.

## Compatibility finding

Ubuntu Clang 18.1.3 rejected numerous first-party and pinned SMC source/header
files with:

```text
error: no newline at end of file [-Werror,-Wnewline-eof]
```

Classification:

```text
FAIL-PRODUCT
phase=strict-app-build
reason=application-compile-failure
status=2
```

This was an ordinary strict compiler diagnostic, not a compiler crash. No source
file was changed. Test compilation and execution were correctly not attempted in
that profile after the application prerequisite failed.

## Harness correction during implementation

The first Alpine image build stopped because BusyBox `sha256sum` supports `-c` but
not GNU's long `--check` spelling. This was a profile-harness portability defect,
not a project or Docker failure. The common dependency script now uses portable
`sha256sum -c`; the rebuilt Alpine image and complete profile passed.

Image-build classification was also separated into project dependency-build
failure versus Docker/image-tool failure so future survey evidence is not hidden
behind a generic tool classification.

## Verification

- `make test-platform-harness`: passed classifier, strict-check, and survey
  continuation fixtures.
- Individual image builds: all four passed.
- Individual profiles: three passed; Ubuntu Clang produced the typed product
  failure above.
- `make platform-survey`: completed all four profiles and wrote the mixed summary.
- `make platform-check`: failed as required for the required Clang incompatibility.
- Shell syntax checks: passed for all platform-profile scripts.
- `git diff -- src tests`: empty throughout and at closeout.
- `git diff --check`: passed.

The prior canonical Docker Valgrind gate remains separate and was not replaced.
No sanitizer or leak claim is made from these new profile runs.

## Deferred work

- Decide whether and when to remediate the Clang final-newline findings.
- Add native Windows, macOS, Steam Deck, X11, and Wayland providers separately.
- Add Linux arm64 only when an appropriate native or explicitly emulated provider
  exists.
- Add profile-specific sanitizer phases only after a separately scoped decision.
- Do not infer display, input, package, or release support from this headless survey.