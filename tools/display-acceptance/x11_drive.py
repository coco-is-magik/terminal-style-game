#!/usr/bin/env python3
"""Drive one native X11 display-acceptance session through XTest."""

from __future__ import annotations

import ctypes
import ctypes.util
import subprocess
import sys
import time
from pathlib import Path


class XClientMessageData(ctypes.Union):
    _fields_ = [("bytes", ctypes.c_char * 20),
                ("shorts", ctypes.c_short * 10),
                ("longs", ctypes.c_long * 5)]


class XClientMessageEvent(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("serial", ctypes.c_ulong),
        ("send_event", ctypes.c_int),
        ("display", ctypes.c_void_p),
        ("window", ctypes.c_ulong),
        ("message_type", ctypes.c_ulong),
        ("format", ctypes.c_int),
        ("data", XClientMessageData),
    ]


class XEvent(ctypes.Union):
    _fields_ = [("type", ctypes.c_int),
                ("client", XClientMessageEvent),
                ("padding", ctypes.c_long * 24)]


class X11Driver:
    def __init__(self) -> None:
        x11_name = ctypes.util.find_library("X11")
        xtst_name = ctypes.util.find_library("Xtst")
        if not x11_name or not xtst_name:
            raise RuntimeError("X11 or XTest runtime library is unavailable")
        self.x11 = ctypes.CDLL(x11_name)
        self.xtst = ctypes.CDLL(xtst_name)
        self.x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        self.x11.XOpenDisplay.restype = ctypes.c_void_p
        self.x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        self.x11.XDefaultRootWindow.restype = ctypes.c_ulong
        self.x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
        self.x11.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
        self.x11.XRaiseWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        self.x11.XFlush.argtypes = [ctypes.c_void_p]
        self.x11.XWarpPointer.argtypes = [
            ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong,
            ctypes.c_int, ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
            ctypes.c_int, ctypes.c_int,
        ]
        self.x11.XQueryTree.argtypes = [
            ctypes.c_void_p,
            ctypes.c_ulong,
            ctypes.POINTER(ctypes.c_ulong),
            ctypes.POINTER(ctypes.c_ulong),
            ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)),
            ctypes.POINTER(ctypes.c_uint),
        ]
        self.x11.XFetchName.argtypes = [
            ctypes.c_void_p,
            ctypes.c_ulong,
            ctypes.POINTER(ctypes.c_char_p),
        ]
        self.x11.XFree.argtypes = [ctypes.c_void_p]
        self.x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        self.x11.XInternAtom.restype = ctypes.c_ulong
        self.x11.XGetWindowProperty.argtypes = [
            ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_long,
            ctypes.c_long, ctypes.c_int, ctypes.c_ulong,
            ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong),
            ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte)),
        ]
        self.x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                                        ctypes.c_long, ctypes.POINTER(XEvent)]
        self.x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
        self.x11.XStringToKeysym.restype = ctypes.c_ulong
        self.x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        self.x11.XKeysymToKeycode.restype = ctypes.c_ubyte
        self.xtst.XTestFakeButtonEvent.argtypes = [
            ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong
        ]
        self.xtst.XTestFakeKeyEvent.argtypes = [
            ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong
        ]
        self.display = self.x11.XOpenDisplay(None)
        if not self.display:
            raise RuntimeError("cannot open DISPLAY")
        self.wm_state = self.x11.XInternAtom(self.display, b"WM_STATE", 1)
        if not self.wm_state:
            raise RuntimeError("ICCCM WM_STATE atom is unavailable")

    def close(self) -> None:
        self.x11.XCloseDisplay(self.display)

    def find_window(self, title: str, deadline: float) -> int:
        root = self.x11.XDefaultRootWindow(self.display)
        while time.monotonic() < deadline:
            found = self._find_descendant(root, title)
            if found:
                return found
            time.sleep(0.1)
        raise RuntimeError(f"window not found: {title}")

    def _find_descendant(self, window: int, title: str) -> int:
        name = ctypes.c_char_p()
        if self.x11.XFetchName(self.display, window, ctypes.byref(name)) and name.value:
            try:
                if (name.value.decode("utf-8", "replace") == title and
                        self._is_managed_client(window)):
                    return window
            finally:
                self.x11.XFree(name)
        root = ctypes.c_ulong()
        parent = ctypes.c_ulong()
        children = ctypes.POINTER(ctypes.c_ulong)()
        count = ctypes.c_uint()
        if not self.x11.XQueryTree(
            self.display,
            window,
            ctypes.byref(root),
            ctypes.byref(parent),
            ctypes.byref(children),
            ctypes.byref(count),
        ):
            return 0
        try:
            for index in range(count.value):
                found = self._find_descendant(children[index], title)
                if found:
                    return found
        finally:
            if children:
                self.x11.XFree(children)
        return 0

    def _is_managed_client(self, window: int) -> bool:
        actual_type = ctypes.c_ulong()
        actual_format = ctypes.c_int()
        item_count = ctypes.c_ulong()
        bytes_after = ctypes.c_ulong()
        value = ctypes.POINTER(ctypes.c_ubyte)()
        status = self.x11.XGetWindowProperty(
            self.display, window, self.wm_state, 0, 0, 0, 0,
            ctypes.byref(actual_type), ctypes.byref(actual_format),
            ctypes.byref(item_count), ctypes.byref(bytes_after), ctypes.byref(value),
        )
        if value:
            self.x11.XFree(value)
        return status == 0 and actual_type.value != 0

    def resize_focus_and_input(self, window: int, screenshot: Path) -> None:
        self.request_fullscreen(window)
        self.x11.XSetInputFocus(self.display, window, 1, 0)
        self.x11.XRaiseWindow(self.display, window)
        self.x11.XFlush(self.display)
        time.sleep(1.0)
        self.x11.XWarpPointer(self.display, 0, window, 0, 0, 0, 0, 20, 20)
        self.x11.XFlush(self.display)
        time.sleep(0.2)
        self.x11.XWarpPointer(self.display, 0, window, 0, 0, 0, 0, 320, 240)
        self.x11.XFlush(self.display)
        time.sleep(0.3)
        self.xtst.XTestFakeButtonEvent(self.display, 1, 1, 0)
        self.x11.XFlush(self.display)
        time.sleep(0.2)
        self.xtst.XTestFakeButtonEvent(self.display, 1, 0, 0)
        self.x11.XFlush(self.display)
        subprocess.run(
            ["import", "-window", hex(window), str(screenshot)],
            check=True,
            timeout=10,
        )
        keycode = self.x11.XKeysymToKeycode(
            self.display, self.x11.XStringToKeysym(b"Return")
        )
        if keycode == 0:
            raise RuntimeError("Return has no X11 keycode")
        self.xtst.XTestFakeKeyEvent(self.display, keycode, 1, 0)
        self.xtst.XTestFakeKeyEvent(self.display, keycode, 0, 0)
        self.x11.XFlush(self.display)

    def request_fullscreen(self, window: int) -> None:
        message_type = self.x11.XInternAtom(self.display, b"_NET_WM_STATE", 0)
        fullscreen = self.x11.XInternAtom(self.display, b"_NET_WM_STATE_FULLSCREEN", 0)
        if not message_type or not fullscreen:
            raise RuntimeError("EWMH fullscreen atoms are unavailable")
        event = XEvent()
        event.client.type = 33
        event.client.serial = 0
        event.client.send_event = 1
        event.client.display = self.display
        event.client.window = window
        event.client.message_type = message_type
        event.client.format = 32
        event.client.data.longs[0] = 2
        event.client.data.longs[1] = fullscreen
        event.client.data.longs[2] = 0
        event.client.data.longs[3] = 1
        event.client.data.longs[4] = 0
        root = self.x11.XDefaultRootWindow(self.display)
        mask = (1 << 20) | (1 << 19)
        if not self.x11.XSendEvent(self.display, root, 0, mask, ctypes.byref(event)):
            raise RuntimeError("window manager rejected EWMH fullscreen request")
        self.x11.XFlush(self.display)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: x11_drive.py SCREENSHOT", file=sys.stderr)
        return 2
    driver = X11Driver()
    try:
        window = driver.find_window("ASCII FPS Prototype", time.monotonic() + 10.0)
        print(f"x11_window_id={hex(window)}", flush=True)
        driver.resize_focus_and_input(window, Path(sys.argv[1]))
    finally:
        driver.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())