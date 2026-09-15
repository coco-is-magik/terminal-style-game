#!/usr/bin/env python3
"""Phase-driven native Windows display/input acceptance procedure."""

from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import json
import os
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "platform-profiles"))
from windows_dependencies import windows_to_msys
from windows_guest import GuestSurvey
from windows_product import WindowsProductSurvey
from windows_qga import QgaClient, QgaError
from windows_result import SurveyFailure


OUTCOMES = {0: "PASS", 1: "FAIL-PRODUCT", 2: "FAIL-MISSING-TOOL", 3: "FAIL-TOOL"}
BUILD_TASK_PREFIX = "AsciiFpsDisplayBuild-"
APP_TASK_PREFIX = "AsciiFpsDisplayApp-"


class ProcedureError(RuntimeError):
    def __init__(self, status: int, phase: str, reason: str) -> None:
        super().__init__(reason)
        self.status = status
        self.phase = phase
        self.reason = reason


@dataclass
class RunState:
    run_id: str
    phase: str
    vm_uri: str
    vm_name: str
    vm_initial_state: str
    vm_started: bool
    windows_user: str
    workspace: str
    source: str
    build_task: str
    app_task: str
    started_utc: str


def utc_now() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


class Procedure:
    def __init__(self, root: Path, output: Path) -> None:
        self.root = root
        self.output = output
        self.state_path = output / "state.json"
        self.result_path = output / "result.env"
        self.vm_uri = os.environ.get("PROFILE_VM_URI", "qemu:///session")
        self.vm_name = os.environ.get("PROFILE_VM_NAME", "")
        self.windows_user = os.environ.get("DISPLAY_ACCEPTANCE_WINDOWS_USER", "")
        self.virsh = os.environ.get("PLATFORM_VIRSH", "virsh")

    def record_phase(self, phase: str, detail: str) -> None:
        self.output.mkdir(parents=True, exist_ok=True)
        with (self.output / "procedure.log").open("a", encoding="utf-8") as log:
            log.write(f"{utc_now()} phase={phase} detail={detail}\n")
        print(f"{utc_now()} phase={phase} detail={detail}", flush=True)

    def write_result(self, status: int, phase: str, reason: str) -> None:
        self.output.mkdir(parents=True, exist_ok=True)
        self.result_path.write_text(
            f"OUTCOME={OUTCOMES.get(status, 'FAIL-TOOL')}\nPHASE={phase}\n"
            f"REASON={reason}\nSTATUS={status}\n",
            encoding="utf-8",
        )

    def save_state(self, state: RunState) -> None:
        temporary = self.state_path.with_suffix(".tmp")
        temporary.write_text(json.dumps(asdict(state), indent=2) + "\n", encoding="utf-8")
        temporary.replace(self.state_path)

    def load_state(self) -> RunState:
        try:
            return RunState(**json.loads(self.state_path.read_text(encoding="utf-8")))
        except (OSError, TypeError, ValueError, json.JSONDecodeError) as error:
            raise ProcedureError(3, "state", "run-state-missing-or-invalid") from error

    def client(self, state: RunState | None = None) -> QgaClient:
        return QgaClient(
            state.vm_uri if state else self.vm_uri,
            state.vm_name if state else self.vm_name,
            self.virsh,
            int(os.environ.get("PLATFORM_VM_COMMAND_TIMEOUT", "30")),
            float(os.environ.get("PLATFORM_QGA_POLL_SECONDS", "1")),
        )

    def virsh_call(self, *arguments: str,
                   state: RunState | None = None) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [self.virsh, "-c", state.vm_uri if state else self.vm_uri, *arguments],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )

    def domain_state(self, state: RunState | None = None) -> str:
        result = self.virsh_call(
            "domstate", state.vm_name if state else self.vm_name, state=state
        )
        if result.returncode != 0:
            raise ProcedureError(3, "preflight", "vm-state-unavailable")
        return result.stdout.strip()

    def preflight(self) -> None:
        self.record_phase("preflight", "checking-host-and-domain")
        required = (self.virsh, "spicy", "import")
        missing = [tool for tool in required if shutil.which(tool) is None]
        if missing:
            raise ProcedureError(2, "preflight", "missing-tool-" + missing[0])
        if not os.environ.get("DISPLAY"):
            raise ProcedureError(2, "preflight", "host-display-unset")
        if not ctypes.util.find_library("X11") or not ctypes.util.find_library("Xtst"):
            raise ProcedureError(2, "preflight", "x11-xtest-runtime-unavailable")
        if not self.vm_name:
            raise ProcedureError(2, "preflight", "windows-vm-not-configured")
        if not self.windows_user:
            raise ProcedureError(2, "preflight", "windows-console-user-not-configured")
        info = self.virsh_call("dominfo", self.vm_name)
        xml = self.virsh_call("dumpxml", self.vm_name)
        if info.returncode != 0:
            raise ProcedureError(2, "preflight", "windows-vm-not-found")
        if xml.returncode != 0:
            raise ProcedureError(3, "preflight", "windows-vm-inspection-failure")
        if "<graphics type='spice'" not in xml.stdout:
            raise ProcedureError(2, "preflight", "spice-console-not-configured")
        if "org.qemu.guest_agent.0" not in xml.stdout:
            raise ProcedureError(2, "preflight", "qga-channel-not-configured")
        self.record_phase("preflight", "pass")

    def wait_for_qga(self, state: RunState, attempts: int = 60) -> None:
        self.record_phase("vm-readiness", "waiting-for-qga")
        for attempt in range(1, attempts + 1):
            try:
                self.client(state).call("guest-ping")
                self.record_phase("vm-readiness", f"pass-attempt-{attempt}")
                return
            except QgaError:
                time.sleep(5)
        raise ProcedureError(3, "vm-readiness", "qga-readiness-timeout")

    def verify_console_user(self, state: RunState) -> None:
        self.record_phase("console-session", "checking-active-user")
        script = (
            "$u=(Get-CimInstance Win32_ComputerSystem).UserName;"
            "if(-not $u){exit 41};$u"
        )
        identity = self.powershell(state, script).decode("utf-8-sig", "replace").strip()
        actual_user = identity.rsplit("\\", 1)[-1]
        if actual_user.casefold() != state.windows_user.casefold():
            raise ProcedureError(2, "console-session", "configured-console-user-not-active")
        self.record_phase("console-session", "pass")

    def prepare(self) -> None:
        if self.state_path.exists():
            raise ProcedureError(3, "prepare", "active-run-state-already-exists")
        self.preflight()
        initial = self.domain_state()
        if initial not in ("running", "shut off"):
            raise ProcedureError(3, "prepare", "unsupported-initial-vm-state")
        survey = GuestSurvey(self.root, self.output, self.client())
        state = RunState(
            survey.run_id, "prepare", self.vm_uri, self.vm_name, initial, False,
            self.windows_user, survey.workspace, survey.source,
            BUILD_TASK_PREFIX + survey.run_id, APP_TASK_PREFIX + survey.run_id, utc_now(),
        )
        self.save_state(state)
        if initial == "shut off":
            self.record_phase("vm-start", "requesting-start")
            started = self.virsh_call("start", self.vm_name)
            if started.returncode != 0:
                raise ProcedureError(3, "vm-start", "vm-start-failure")
            state.vm_started = True
            self.save_state(state)
        self.wait_for_qga(state)
        self.verify_console_user(state)
        self.record_phase("prepare", "checking-pinned-dependencies")
        survey.client = self.client(state)
        survey.check_dependencies()
        self.record_phase("prepare", "transferring-manifest-identified-source")
        survey.transfer_source(256 * 1024 * 1024)
        state.phase = "prepared"
        self.save_state(state)
        self.record_phase("prepare", "complete")

    def powershell(self, state: RunState, script: str, timeout: int = 120) -> bytes:
        code, stdout, stderr = self.client(state).exec(
            r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe",
            ["-NoLogo", "-NoProfile", "-NonInteractive", "-Command", script], timeout,
        )
        if code != 0:
            raise ProcedureError(3, state.phase, "guest-powershell-failure")
        return stdout + stderr

    def build_start(self) -> None:
        state = self.load_state()
        if state.phase != "prepared":
            raise ProcedureError(3, "build-start", "run-is-not-prepared")
        state.phase = "dependency-layout"
        self.save_state(state)
        self.record_phase("dependency-layout", "arranging-pinned-dependencies")
        product = WindowsProductSurvey(self.client(state), self.root, self.output, state.source)
        product.arrange_dependencies()
        source = windows_to_msys(state.source)
        dependencies = windows_to_msys(product.dependencies)
        build_cmd = self.output / "build.cmd"
        build_cmd.write_text(
            "@echo off\r\n"
            f"set PATH={product.dependencies}\\bin;C:\\msys64\\ucrt64\\bin;C:\\msys64\\usr\\bin;%PATH%\r\n"
            f"C:\\msys64\\usr\\bin\\bash.exe -lc \"export MSYSTEM=UCRT64; "
            f"export PATH='{dependencies}/bin:/ucrt64/bin:/usr/bin'; cd '{source}'; "
            f"make CC=gcc all >'{windows_to_msys(state.workspace)}/native-build.log' 2>&1\"\r\n"
            f"echo %ERRORLEVEL%> {state.workspace}\\build.status\r\n",
            encoding="ascii",
        )
        self.client(state).put_file(build_cmd, state.workspace + r"\build.cmd")
        state.phase = "building"
        self.save_state(state)
        self.record_phase("build-start", "registering-background-guest-build-task")
        script = (
            "$a=New-ScheduledTaskAction -Execute 'C:\\Windows\\System32\\cmd.exe' "
            f"-Argument '/d /c \"{state.workspace}\\build.cmd\"';"
            "$p=New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount "
            "-RunLevel Highest;"
            f"Register-ScheduledTask -TaskName '{state.build_task}' -Action $a "
            "-Principal $p -Force|Out-Null;"
            f"Start-ScheduledTask -TaskName '{state.build_task}'"
        )
        self.powershell(state, script)
        self.record_phase("build-start", "started-use-status-or-wait-build")

    def read_guest_file(self, state: RunState, suffix: str, limit: int = 1024) -> bytes | None:
        try:
            return self.client(state).get_file(state.workspace + suffix, max_bytes=limit)
        except QgaError:
            return None

    def refresh_build_state(self, state: RunState) -> RunState:
        status = self.read_guest_file(state, r"\build.status", 32)
        if status is None:
            return state
        if status.strip() == b"0":
            state.phase = "built"
            self.save_state(state)
            return state
        state.phase = "build-failed"
        self.save_state(state)
        return state

    def status(self) -> None:
        state = self.load_state()
        print(json.dumps(asdict(state), indent=2))
        if state.phase in ("building", "build-failed", "built"):
            status = self.read_guest_file(state, r"\build.status", 32)
            if status is None:
                print("observed_build_phase=building")
            elif status.strip() == b"0":
                print("observed_build_phase=built")
            else:
                print("observed_build_phase=build-failed")
            print("build_status=" +
                  (status.decode("ascii", "replace").strip() if status else "running"))

    def wait_build(self) -> None:
        state = self.load_state()
        if state.phase not in ("building", "built"):
            raise ProcedureError(3, "wait-build", "build-was-not-started")
        self.record_phase("wait-build", "waiting-for-explicit-guest-status")
        wait_seconds = int(os.environ.get("DISPLAY_ACCEPTANCE_BUILD_WAIT_SECONDS", "3600"))
        if wait_seconds <= 0:
            raise ProcedureError(3, "wait-build", "invalid-build-wait-seconds")
        deadline = time.monotonic() + wait_seconds
        while time.monotonic() < deadline:
            state = self.refresh_build_state(state)
            if state.phase == "built":
                self.record_phase("wait-build", "complete")
                return
            if state.phase == "build-failed":
                self.collect_build_log(state)
                raise ProcedureError(1, "build", "native-application-build-failure")
            time.sleep(10)
        raise ProcedureError(3, "wait-build", "native-build-status-timeout")

    def collect_build_log(self, state: RunState) -> None:
        log = self.read_guest_file(state, r"\native-build.log", 16 * 1024 * 1024)
        if log is not None:
            (self.output / "native-build.log").write_bytes(log)

    def observe(self) -> None:
        state = self.refresh_build_state(self.load_state())
        if state.phase != "built":
            raise ProcedureError(3, "observe", "native-build-is-not-complete")
        self.collect_build_log(state)
        state.phase = "observing"
        self.save_state(state)
        self.record_phase("observe", "opening-spice-after-successful-build")
        display = self.virsh_call("domdisplay", state.vm_name, state=state)
        if display.returncode != 0 or not display.stdout.strip().startswith("spice://"):
            raise ProcedureError(3, "observe", "spice-display-address-unavailable")
        viewer_log = (self.output / "spice.log").open("wb")
        viewer = subprocess.Popen(
            ["spicy", "--uri", display.stdout.strip(), "--title", "ASCII FPS Windows Acceptance"],
            stdout=viewer_log, stderr=subprocess.STDOUT,
        )
        window = None
        try:
            window = SpiceWindow("ASCII FPS Windows Acceptance")
            window.wait_until_visible(30)
            app_cmd = self.output / "app.cmd"
            dependencies = WindowsProductSurvey(
                self.client(state), self.root, self.output, state.source
            ).dependencies
            app_cmd.write_text(
                "@echo off\r\n"
                f"set PATH={dependencies}\\bin;C:\\msys64\\ucrt64\\bin;%PATH%\r\n"
                f"cd /d {state.source}\r\n"
                f"build\\ascii-fps.exe --display-acceptance 30 > {state.workspace}\\application.log 2>&1\r\n"
                f"echo %ERRORLEVEL%> {state.workspace}\\application.status\r\n",
                encoding="ascii",
            )
            self.client(state).put_file(app_cmd, state.workspace + r"\app.cmd")
            script = (
                "$a=New-ScheduledTaskAction -Execute 'C:\\Windows\\System32\\cmd.exe' "
                f"-Argument '/d /c \"{state.workspace}\\app.cmd\"';"
                f"$p=New-ScheduledTaskPrincipal -UserId '{state.windows_user}' "
                "-LogonType Interactive -RunLevel Limited;"
                f"Register-ScheduledTask -TaskName '{state.app_task}' -Action $a "
                "-Principal $p -Force|Out-Null;"
                f"Start-ScheduledTask -TaskName '{state.app_task}'"
            )
            self.powershell(state, script)
            self.record_phase("observe", "waiting-for-application-ready-record")
            for _ in range(30):
                log = self.read_guest_file(state, r"\application.log", 1024 * 1024)
                if log and b'"display_acceptance":"ready"' in log:
                    break
                time.sleep(1)
            else:
                raise ProcedureError(3, "observe", "application-ready-record-timeout")
            window.drive(self.output / "presentation.png")
            self.record_phase("observe", "input-and-capture-complete")
            for _ in range(45):
                status = self.read_guest_file(state, r"\application.status", 32)
                if status is not None:
                    break
                time.sleep(1)
            else:
                raise ProcedureError(3, "observe", "application-status-timeout")
        finally:
            if window is not None:
                window.close()
            viewer.terminate()
            try:
                viewer.wait(timeout=10)
            except subprocess.TimeoutExpired:
                viewer.kill()
                viewer.wait(timeout=10)
                self.record_phase("observe", "host-viewer-required-kill-after-terminate")
            viewer_log.close()
        state.phase = "observed"
        self.save_state(state)

    def collect(self) -> None:
        state = self.load_state()
        if state.phase != "observed":
            raise ProcedureError(3, "collect", "observation-is-not-complete")
        log = self.read_guest_file(state, r"\application.log", 16 * 1024 * 1024)
        status = self.read_guest_file(state, r"\application.status", 32)
        if log is None or status is None:
            raise ProcedureError(3, "collect", "application-result-missing")
        (self.output / "application.log").write_bytes(log)
        (self.output / "host.log").write_text(
            "session_type=Windows-active-console\n"
            "session_context=virtual-SPICE-console-not-QGA-display\n"
            "display_device=libvirt-configured-video-via-SPICE\n"
            "keyboard_device=libvirt-keyboard-via-SPICE\n"
            "pointer_device=libvirt-pointer-via-SPICE\n"
            "input_method=synthetic-host-XTest-forwarded-through-SPICE-not-physical\n"
            "capture_method=ImageMagick-import-SPICE-viewer-window\n"
            "launch_method=Windows-Task-Scheduler-interactive-token\n"
            "application_duration_limit_seconds=30\n",
            encoding="utf-8",
        )
        state.phase = "collected"
        self.save_state(state)
        if status.strip() != b"0" or b'"display_acceptance":"pass"' not in log:
            raise ProcedureError(1, "collect", "display-acceptance-criteria-failed")
        self.write_result(0, "collect", "display-acceptance-passed")
        self.record_phase("collect", "pass")

    def cleanup(self) -> None:
        state = self.load_state()
        state.phase = "cleanup"
        self.save_state(state)
        self.record_phase("cleanup", "removing-run-owned-guest-resources")
        script = self.cleanup_script(state)
        self.powershell(state, script)
        if state.vm_started:
            self.record_phase("cleanup", "requesting-graceful-qga-shutdown")
            try:
                self.client(state).call("guest-shutdown", {"mode": "powerdown"})
            except QgaError:
                self.record_phase("cleanup", "qga-shutdown-request-not-acknowledged")
            for _ in range(60):
                if self.domain_state(state) == "shut off":
                    break
                time.sleep(5)
            else:
                raise ProcedureError(3, "cleanup", "graceful-vm-shutdown-timeout-left-running")
        self.state_path.unlink()
        self.record_phase("cleanup", "complete")

    @staticmethod
    def cleanup_script(state: RunState) -> str:
        return (
            f"Stop-ScheduledTask -TaskName '{state.build_task}' -ErrorAction SilentlyContinue;"
            f"Stop-ScheduledTask -TaskName '{state.app_task}' -ErrorAction SilentlyContinue;"
            f"Unregister-ScheduledTask -TaskName '{state.build_task}' -Confirm:$false "
            "-ErrorAction SilentlyContinue;"
            f"Unregister-ScheduledTask -TaskName '{state.app_task}' -Confirm:$false "
            "-ErrorAction SilentlyContinue;"
            f"$w='{state.workspace}';if(Test-Path -LiteralPath $w){{"
            "Remove-Item -LiteralPath $w -Recurse -Force -ErrorAction Stop}}"
        )


class SpiceWindow:
    def __init__(self, title: str) -> None:
        x11_name = ctypes.util.find_library("X11")
        xtst_name = ctypes.util.find_library("Xtst")
        if not x11_name or not xtst_name:
            raise ProcedureError(2, "observe", "x11-xtest-runtime-unavailable")
        self.title = title
        self.x11 = ctypes.CDLL(x11_name)
        self.xtst = ctypes.CDLL(xtst_name)
        self.x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        self.x11.XOpenDisplay.restype = ctypes.c_void_p
        self.x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
        self.x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        self.x11.XDefaultRootWindow.restype = ctypes.c_ulong
        self.x11.XQueryTree.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
            ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong),
            ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)), ctypes.POINTER(ctypes.c_uint)]
        self.x11.XFetchName.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
                                        ctypes.POINTER(ctypes.c_char_p)]
        self.x11.XFree.argtypes = [ctypes.c_void_p]
        self.x11.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
                                            ctypes.c_int, ctypes.c_ulong]
        self.x11.XWarpPointer.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong,
            ctypes.c_int, ctypes.c_int, ctypes.c_uint, ctypes.c_uint, ctypes.c_int, ctypes.c_int]
        self.x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
        self.x11.XStringToKeysym.restype = ctypes.c_ulong
        self.x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        self.x11.XKeysymToKeycode.restype = ctypes.c_ubyte
        self.x11.XFlush.argtypes = [ctypes.c_void_p]
        self.xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                                ctypes.c_int, ctypes.c_ulong]
        self.xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                                   ctypes.c_int, ctypes.c_ulong]
        self.display = self.x11.XOpenDisplay(None)
        if not self.display:
            raise ProcedureError(3, "observe", "host-x11-display-open-failure")
        self.window = 0

    def close(self) -> None:
        if self.display:
            self.x11.XCloseDisplay(self.display)
            self.display = None

    def find(self, parent: int) -> int:
        name = ctypes.c_char_p()
        if self.x11.XFetchName(self.display, parent, ctypes.byref(name)) and name.value:
            try:
                if name.value.decode("utf-8", "replace") == self.title:
                    return parent
            finally:
                self.x11.XFree(name)
        root = ctypes.c_ulong()
        ancestor = ctypes.c_ulong()
        children = ctypes.POINTER(ctypes.c_ulong)()
        count = ctypes.c_uint()
        if not self.x11.XQueryTree(self.display, parent, ctypes.byref(root),
                                   ctypes.byref(ancestor), ctypes.byref(children),
                                   ctypes.byref(count)):
            return 0
        try:
            for index in range(count.value):
                found = self.find(children[index])
                if found:
                    return found
        finally:
            if children:
                self.x11.XFree(children)
        return 0

    def wait_until_visible(self, seconds: int) -> None:
        root = self.x11.XDefaultRootWindow(self.display)
        for _ in range(seconds * 5):
            self.window = self.find(root)
            if self.window:
                return
            time.sleep(0.2)
        raise ProcedureError(3, "observe", "spice-viewer-window-timeout")

    def key(self, name: str, modifier: str | None = None) -> None:
        names = [modifier, name] if modifier else [name]
        codes = [self.x11.XKeysymToKeycode(
            self.display, self.x11.XStringToKeysym(item.encode("ascii"))) for item in names]
        if any(code == 0 for code in codes):
            raise ProcedureError(3, "observe", "host-keycode-unavailable")
        for code in codes:
            self.xtst.XTestFakeKeyEvent(self.display, code, 1, 0)
        for code in reversed(codes):
            self.xtst.XTestFakeKeyEvent(self.display, code, 0, 0)
        self.x11.XFlush(self.display)
        time.sleep(0.15)

    def drive(self, screenshot: Path) -> None:
        self.x11.XSetInputFocus(self.display, self.window, 1, 0)
        self.x11.XFlush(self.display)
        self.key("space", "Alt_L")
        self.key("s")
        for _ in range(4):
            self.key("Left")
        for _ in range(3):
            self.key("Up")
        self.key("Return")
        self.x11.XWarpPointer(self.display, 0, self.window, 0, 0, 0, 0, 100, 100)
        self.x11.XFlush(self.display)
        time.sleep(0.2)
        self.x11.XWarpPointer(self.display, 0, self.window, 0, 0, 0, 0, 500, 400)
        self.x11.XFlush(self.display)
        self.xtst.XTestFakeButtonEvent(self.display, 1, 1, 0)
        self.x11.XFlush(self.display)
        time.sleep(0.2)
        self.xtst.XTestFakeButtonEvent(self.display, 1, 0, 0)
        self.x11.XFlush(self.display)
        subprocess.run(["import", "-window", hex(self.window), str(screenshot)],
                       check=True, timeout=10)
        self.key("Return")


def execute(procedure: Procedure, command: str) -> int:
    try:
        if command == "preflight": procedure.preflight()
        elif command == "prepare": procedure.prepare()
        elif command == "build-start": procedure.build_start()
        elif command == "wait-build": procedure.wait_build()
        elif command == "status": procedure.status()
        elif command == "observe": procedure.observe()
        elif command == "collect": procedure.collect()
        elif command == "cleanup": procedure.cleanup()
        elif command == "run":
            try:
                procedure.prepare()
                procedure.build_start()
                procedure.wait_build()
                procedure.observe()
                procedure.collect()
            except Exception:
                if procedure.state_path.exists():
                    procedure.cleanup()
                raise
            procedure.cleanup()
        return 0
    except ProcedureError as error:
        procedure.write_result(error.status, error.phase, error.reason)
        print(f"{OUTCOMES.get(error.status, 'FAIL-TOOL')}: "
              f"phase={error.phase} reason={error.reason}", file=sys.stderr)
        return error.status
    except SurveyFailure as error:
        procedure.write_result(error.status, error.phase, error.reason)
        print(f"{error.outcome}: phase={error.phase} reason={error.reason}", file=sys.stderr)
        return error.status
    except (OSError, QgaError, subprocess.SubprocessError, ValueError) as error:
        procedure.write_result(3, command, "unexpected-orchestration-failure")
        with (procedure.output / "procedure-error.log").open("a", encoding="utf-8") as log:
            log.write(f"{utc_now()} command={command} error={error}\n")
        print(f"FAIL-TOOL: phase={command} reason=unexpected-orchestration-failure",
              file=sys.stderr)
        return 3


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("preflight", "prepare", "build-start",
                                             "wait-build", "status", "observe",
                                             "collect", "cleanup", "run"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = Path(os.environ.get(
        "DISPLAY_ACCEPTANCE_OUTPUT", root / "build/display-acceptance/windows-spice"
    ))
    return execute(Procedure(root, output), args.command)


if __name__ == "__main__":
    raise SystemExit(main())