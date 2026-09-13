#!/usr/bin/env python3
"""Deterministic tests for the QGA transport boundary."""

from __future__ import annotations

import base64
import json
import os
import stat
import tempfile
import unittest
from pathlib import Path

from windows_qga import QgaClient, QgaError


FAKE_VIRSH = r'''#!/usr/bin/env python3
import base64
import json
import os
import sys
from pathlib import Path

request = json.loads(sys.argv[-1])
execute = request["execute"]
arguments = request.get("arguments", {})
state = Path(os.environ["FAKE_QGA_STATE"])
log = Path(os.environ["FAKE_QGA_LOG"])
with log.open("a", encoding="utf-8") as output:
    output.write(execute + "\n")
files = state / "files"
files.mkdir(exist_ok=True)
if os.environ.get("FAKE_QGA_SCENARIO") == "invalid-json":
    print("not json")
    raise SystemExit(0)
if execute == "guest-exec":
    response = {"pid": 7}
elif execute == "guest-exec-status":
    response = {
        "exited": True,
        "exitcode": 0,
        "out-data": base64.b64encode(b"ok").decode("ascii"),
    }
elif execute == "guest-file-open":
    path = files / "content.bin"
    if arguments["mode"] == "wb":
        path.write_bytes(b"")
    response = 9
elif execute == "guest-file-write":
    chunk = base64.b64decode(arguments["buf-b64"])
    with (files / "content.bin").open("ab") as output:
        output.write(chunk)
    response = {"count": len(chunk), "eof": False}
elif execute == "guest-file-read":
    path = files / "content.bin"
    offset_path = state / "offset"
    offset = int(offset_path.read_text()) if offset_path.exists() else 0
    chunk = path.read_bytes()[offset:offset + arguments["count"]]
    offset_path.write_text(str(offset + len(chunk)))
    response = {
        "count": len(chunk),
        "buf-b64": base64.b64encode(chunk).decode("ascii"),
        "eof": offset + len(chunk) >= path.stat().st_size,
    }
elif execute in ("guest-file-flush", "guest-file-close"):
    response = {}
else:
    response = {}
print(json.dumps({"return": response}, separators=(",", ":")))
'''


class QgaClientTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.virsh = self.root / "virsh"
        self.virsh.write_text(FAKE_VIRSH, encoding="utf-8")
        self.virsh.chmod(self.virsh.stat().st_mode | stat.S_IXUSR)
        os.environ["FAKE_QGA_STATE"] = str(self.root)
        os.environ["FAKE_QGA_LOG"] = str(self.root / "calls.log")
        os.environ["FAKE_QGA_SCENARIO"] = "normal"
        self.client = QgaClient("qemu:///fixture", "fixture", str(self.virsh), 5, 0)

    def tearDown(self) -> None:
        for name in ("FAKE_QGA_STATE", "FAKE_QGA_LOG", "FAKE_QGA_SCENARIO"):
            os.environ.pop(name, None)
        self.temp.cleanup()

    def test_exec_decodes_captured_output(self) -> None:
        code, stdout, stderr = self.client.exec("cmd.exe", ["/c", "ver"], 5)
        self.assertEqual((code, stdout, stderr), (0, b"ok", b""))

    def test_chunked_file_round_trip(self) -> None:
        source = self.root / "source.bin"
        source.write_bytes(bytes(range(256)) * 500)
        self.client.put_file(source, r"C:\fixture.bin", 4096)
        self.assertEqual(self.client.get_file(r"C:\fixture.bin", 3000), source.read_bytes())

    def test_invalid_json_is_rejected(self) -> None:
        os.environ["FAKE_QGA_SCENARIO"] = "invalid-json"
        with self.assertRaises(QgaError):
            self.client.call("guest-ping")

    def test_download_limit_is_enforced(self) -> None:
        content = self.root / "files" / "content.bin"
        content.parent.mkdir(exist_ok=True)
        content.write_bytes(b"too large")
        with self.assertRaises(QgaError):
            self.client.get_file(r"C:\fixture.bin", max_bytes=3)


if __name__ == "__main__":
    unittest.main(verbosity=2)