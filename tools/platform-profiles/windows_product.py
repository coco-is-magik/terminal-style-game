#!/usr/bin/env python3
"""Run unchanged product Make phases in a prepared Windows guest workspace."""

from __future__ import annotations

from pathlib import Path

from windows_dependencies import dependency_set_id, load_dependencies, windows_to_msys
from windows_qga import QgaClient, QgaError
from windows_result import SurveyFailure


class WindowsProductSurvey:
    def __init__(self, client: QgaClient, root: Path, output: Path, source: str) -> None:
        self.client = client
        self.root = root
        self.output = output
        self.source = source
        set_id = dependency_set_id(load_dependencies(root))
        self.dependencies = rf"C:\platform-survey\dependencies\{set_id}"

    def run(self) -> None:
        self.arrange_dependencies()
        self.verify_runner_inventory()
        self.run_make_phase(
            "strict-app-build", "all", 900, "application-compile-failure"
        )
        self.run_make_phase(
            "strict-test-build", "test-build", 1800, "test-compile-failure"
        )
        self.run_make_phase(
            "complete-test-run", "test", 1800, "test-runner-failure"
        )
        self.run_make_phase(
            "standards-core", "standards-core", 300, "standards-core-failure"
        )
        self.verify_native_binaries()

    def arrange_dependencies(self) -> None:
        source = windows_to_msys(self.source)
        dependencies = windows_to_msys(self.dependencies)
        script = f"""set -eu
source='{source}'
deps='{dependencies}'
test -f "$deps/dependency-manifest.json"
test ! -e "$source/vendor"
mkdir -p "$source/vendor/dist" "$source/vendor/src"
cp -R "$deps/include" "$deps/lib64" "$deps/bin" "$source/vendor/dist/"
cp -R "$deps/source/SDL" "$source/vendor/src/SDL"
cp -R "$deps/source/smc" "$source/vendor/src/smc"
"""
        self._run_command(
            "dependency-layout.log",
            script,
            300,
            "FAIL-TOOL",
            "dependency-check",
            "windows-dependency-layout-failure",
        )

    def verify_runner_inventory(self) -> None:
        source = windows_to_msys(self.source)
        script = f"""set -eu
cd '{source}'
runners="$(make -pn 2>/dev/null | sed -n 's/^TEST_RUNNERS := //p' | head -n 1)"
set -- $runners
printf 'runner_count=%s\n' "$#"
printf '%s\n' "$runners"
test "$#" -eq 63
"""
        self._run_command(
            "test-inventory.log",
            script,
            120,
            "FAIL-TOOL",
            "strict-test-build",
            "test-runner-inventory-mismatch",
        )

    def run_make_phase(
        self, phase: str, target: str, seconds: int, product_reason: str
    ) -> None:
        source = windows_to_msys(self.source)
        guest_log = windows_to_msys(self.source.rsplit("\\source", 1)[0]) + f"/{phase}.log"
        script = f"""set -u
export MSYSTEM=UCRT64
export PATH='{windows_to_msys(self.dependencies)}/bin:/ucrt64/bin:/usr/bin'
cd '{source}' || exit 3
timeout {seconds}s make CC=gcc {target} >'{guest_log}' 2>&1
status=$?
printf '%s\n' "$status"
exit "$status"
"""
        code, stdout, stderr = self.client.exec(
            r"C:\msys64\usr\bin\bash.exe", ["-lc", script], seconds + 60
        )
        guest_log_windows = self.source.rsplit("\\source", 1)[0] + f"\\{phase}.log"
        try:
            log = self.client.get_file(guest_log_windows)
        except QgaError as error:
            raise SurveyFailure("FAIL-TOOL", phase, "guest-phase-log-missing", 3) from error
        (self.output / f"{phase}.log").write_bytes(log)
        if code == 0:
            return
        if code in (124, 137, 143):
            raise SurveyFailure("FAIL-TIMEOUT", phase, self._timeout_reason(phase), 4)
        text = (log + stdout + stderr).decode("utf-8", "replace").lower()
        if "internal compiler error" in text:
            raise SurveyFailure("FAIL-TOOL", phase, "compiler-crash", 3)
        reason = product_reason
        if phase == "strict-app-build" and self._looks_like_link_failure(text):
            reason = "application-link-failure"
        raise SurveyFailure("FAIL-PRODUCT", phase, reason, 1)

    def verify_native_binaries(self) -> None:
        source = windows_to_msys(self.source)
        script = f"""set -eu
export PATH=/ucrt64/bin:/usr/bin
cd '{source}'
set -- build/ascii-fps.exe build/test-*.exe
test "$#" -eq 64
for binary in "$@"; do
  test -f "$binary"
  objdump -f "$binary" | grep -F 'file format pei-x86-64' >/dev/null
  imports="$(objdump -p "$binary" | sed -n 's/.*DLL Name: //p' | tr '[:upper:]' '[:lower:]')"
  printf '%s\n' "$binary" "$imports"
  case "$imports" in *msys-2.0.dll*|*cygwin1.dll*) exit 42;; esac
done
"""
        self._run_command(
            "native-binary-check.log",
            script,
            300,
            "FAIL-TOOL",
            "native-binary-check",
            "non-native-windows-binary",
        )

    def _run_command(
        self,
        log_name: str,
        script: str,
        timeout: int,
        outcome: str,
        phase: str,
        reason: str,
    ) -> None:
        code, stdout, stderr = self.client.exec(
            r"C:\msys64\usr\bin\bash.exe", ["-lc", script], timeout
        )
        (self.output / log_name).write_bytes(stdout + stderr)
        if code == 0:
            return
        if code in (124, 137, 143):
            raise SurveyFailure("FAIL-TIMEOUT", phase, self._timeout_reason(phase), 4)
        raise SurveyFailure(outcome, phase, reason, 3 if outcome == "FAIL-TOOL" else 1)

    @staticmethod
    def _looks_like_link_failure(text: str) -> bool:
        markers = (
            "collect2.exe:",
            "ld.exe:",
            "undefined reference",
            "unrecognized option",
            "cannot find -l",
        )
        return any(marker in text for marker in markers)

    @staticmethod
    def _timeout_reason(phase: str) -> str:
        return "execution-timeout" if phase in ("complete-test-run", "standards-core") else "build-timeout"
