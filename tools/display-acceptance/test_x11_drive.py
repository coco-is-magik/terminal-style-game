#!/usr/bin/env python3
"""Deterministic tests for the X11 display driver message boundary."""

from __future__ import annotations

import ctypes
import unittest
from unittest.mock import Mock

from x11_drive import XClientMessageEvent, XEvent, X11Driver


class X11DriveTests(unittest.TestCase):
    def test_client_message_storage_matches_xlib_event_size(self) -> None:
        self.assertEqual(ctypes.sizeof(XEvent), ctypes.sizeof(ctypes.c_long) * 24)
        self.assertLessEqual(ctypes.sizeof(XClientMessageEvent), ctypes.sizeof(XEvent))

    def test_fullscreen_request_uses_standard_ewmh_message(self) -> None:
        driver = object.__new__(X11Driver)
        driver.display = 123
        driver.x11 = Mock()
        driver.x11.XInternAtom.side_effect = [10, 20]
        driver.x11.XDefaultRootWindow.return_value = 30
        driver.x11.XSendEvent.return_value = 1

        driver.request_fullscreen(40)

        args = driver.x11.XSendEvent.call_args.args
        event = ctypes.cast(args[4], ctypes.POINTER(XEvent)).contents.client
        self.assertEqual(args[:4], (123, 30, 0, (1 << 20) | (1 << 19)))
        self.assertEqual((event.type, event.window, event.message_type, event.format),
                         (33, 40, 10, 32))
        self.assertEqual((event.data.longs[0], event.data.longs[1], event.data.longs[3]),
                         (2, 20, 1))


if __name__ == "__main__":
    unittest.main(verbosity=2)