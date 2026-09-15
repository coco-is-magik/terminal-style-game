#!/usr/bin/env python3
"""Run strict compilation and the complete test suite in a Windows guest."""

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
        self.run_make_phase(
            "strict-app-build", "all", 900, "application-compile-failure"
        )
        self.run_make_phase(
            "complete-test-run", "test", 1800, "test-runner-failure"
        )

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
        code, stdout, stderr = self.client.exec(
            r"C:\msys64\usr\bin\bash.exe", ["-lc", script], 300
        )
        (self.output / "dependency-layout.log").write_bytes(stdout + stderr)
        if code != 0:
            raise SurveyFailure(
                "FAIL-TOOL", "dependency-check", "windows-dependency-layout-failure", 3
            )

    def run_make_phase(
        self, phase: str, target: str, seconds: int, product_reason: str
    ) -> None:
        source = windows_to_msys(self.source)
        workspace = self.source.rsplit("\\source", 1)[0]
        guest_log = windows_to_msys(workspace) + f"/{phase}.log"
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
        try:
            log = self.client.get_file(workspace + f"\\{phase}.log")
        except QgaError as error:
            raise SurveyFailure("FAIL-TOOL", phase, "guest-phase-log-missing", 3) from error
        (self.output / f"{phase}.log").write_bytes(log)
        if code == 0:
            return
        if code in (124, 137, 143):
            reason = "execution-timeout" if phase == "complete-test-run" else "build-timeout"
            raise SurveyFailure("FAIL-TIMEOUT", phase, reason, 4)
        text = (log + stdout + stderr).decode("utf-8", "replace").lower()
        if "internal compiler error" in text:
            raise SurveyFailure("FAIL-TOOL", phase, "compiler-crash", 3)
        reason = product_reason
        if phase == "strict-app-build" and self._looks_like_link_failure(text):
            reason = "application-link-failure"
        raise SurveyFailure("FAIL-PRODUCT", phase, reason, 1)

    @staticmethod
    def _looks_like_link_failure(text: str) -> bool:
        markers = (
            "collect2.exe:", "ld.exe:", "undefined reference",
            "unrecognized option", "cannot find -l",
        )
        return any(marker in text for marker in markers)