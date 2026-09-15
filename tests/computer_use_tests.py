"""Desktop guard/release tests. All input functions are mocked; never press keys."""
import ctypes
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'computer_use'))
import capture
import controls
import win32 as w


class ComputerUseTests(unittest.TestCase):
    def setUp(self):
        self.info = dict(hwnd=1, pid=123, exe='D2RLoader.exe', foreground=True, minimized=False, width=2560, height=1440)

    def test_input_layout(self):
        self.assertEqual(ctypes.sizeof(w.Input), 40 if ctypes.sizeof(ctypes.c_void_p) == 8 else 28)

    def test_reject_wrong_focus_or_identity(self):
        for change in [dict(foreground=False), dict(pid=124), dict(exe='another.exe'), dict(minimized=True)]:
            with patch.object(w, 'describe', return_value=self.info | change):
                with self.assertRaises(RuntimeError):
                    w.require_foreground(self.info)

    def test_duration_bounds_never_send(self):
        for seconds in [0, -1, 2.01, float('nan'), float('inf')]:
            with patch.object(controls, 'send') as send:
                with self.assertRaises(ValueError):
                    controls.hold(self.info, [1], [2], seconds)
                send.assert_not_called()

    def test_lost_focus_releases_all_keys_in_reverse_order(self):
        with patch.object(w, 'require_foreground', side_effect=[self.info, RuntimeError('focus lost')]), \
             patch.object(w, 'key_state', return_value=0), patch.object(controls, 'send') as send:
            with self.assertRaisesRegex(RuntimeError, 'focus lost'):
                controls.hold(self.info, ['ctrl down', 't down'], ['ctrl up', 't up'], 0.5)
            self.assertEqual([c.args[0] for c in send.call_args_list], ['ctrl down', 't down', 't up', 'ctrl up'])

    def test_pause_releases_key(self):
        with patch.object(w, 'require_foreground', return_value=self.info), \
             patch.object(w, 'key_state', side_effect=[0, 1]), patch.object(controls, 'send') as send:
            with self.assertRaisesRegex(RuntimeError, 'Pause'):
                controls.hold(self.info, ['w down'], ['w up'], 0.5)
            self.assertEqual([c.args[0] for c in send.call_args_list], ['w down', 'w up'])

    def test_partial_send_failure_releases_successful_keys(self):
        with patch.object(w, 'require_foreground', return_value=self.info), \
             patch.object(w, 'key_state', return_value=0), \
             patch.object(controls, 'send', side_effect=[None, RuntimeError('send failed'), None]) as send:
            with self.assertRaisesRegex(RuntimeError, 'send failed'):
                controls.hold(self.info, ['ctrl down', 't down'], ['ctrl up', 't up'], 0.5)
            self.assertEqual([c.args[0] for c in send.call_args_list], ['ctrl down', 't down', 'ctrl up'])

    def test_physical_modifier_prevents_accidental_shortcut(self):
        for modifier in controls.MODIFIER_KEYS:
            with patch.object(w, 'key_state', side_effect=lambda key: 0x8000 if key == modifier else 0), \
                 patch.object(controls, 'hold') as hold:
                with self.assertRaises(RuntimeError):
                    controls.press_keys(self.info, 'w', 0.1)
                hold.assert_not_called()

    def test_key_parser_and_extended_scan_codes(self):
        self.assertEqual(controls.keys_from_text('ctrl+t'), [0x11, 0x54])
        self.assertEqual(controls.keys_from_text('f9+f13'), [0x78, 0x7C])
        for text in ['', 'delete-all', 'ctrl+ctrl', 'w+a+s+d', 'é']:
            with self.assertRaises(ValueError):
                controls.keys_from_text(text)
        self.assertTrue(controls.key_event(controls.VK['up']).data.key.flags & controls.EXTENDED)
        self.assertTrue(controls.key_event(ord('W'), True).data.key.flags & controls.KEY_UP)

    def test_click_bounds_prevent_input(self):
        with patch.object(w, 'require_foreground', return_value=self.info), patch.object(w, 'cursor_position') as move:
            for x, y in [(-1, 5), (2560, 0), (0, 1440)]:
                with self.assertRaises(ValueError):
                    controls.click(self.info, x, y, 'left')
            move.assert_not_called()

    def test_scroll_limits_prevent_input(self):
        with patch.object(controls, 'move_mouse') as move, patch.object(controls, 'send') as send:
            for steps in [0, -9, 9]:
                with self.assertRaises(ValueError):
                    controls.scroll(self.info, 100, 100, steps)
            move.assert_not_called()
            send.assert_not_called()

    def test_negative_scroll_uses_signed_wheel_delta(self):
        with patch.object(controls, 'move_mouse'), patch.object(w, 'key_state', return_value=0), \
             patch.object(w, 'require_foreground', return_value=self.info), patch.object(controls, 'send') as send:
            controls.scroll(self.info, 100, 100, -3)
            event = send.call_args.args[0]
            self.assertEqual(event.data.mouse.flags, 0x0800)
            self.assertEqual(ctypes.c_int32(event.data.mouse.data).value, -360)

    def test_png_preserves_top_down_color_and_rows(self):
        png = capture.encode_png(1, 2, bytes([0, 0, 255, 0, 255, 0, 0, 0]))
        self.assertEqual(png[:8], b'\x89PNG\r\n\x1a\n')
        chunks, offset = {}, 8
        while offset < len(png):
            size = struct.unpack('!I', png[offset:offset+4])[0]
            kind, data = png[offset+4:offset+8], png[offset+8:offset+8+size]
            crc = struct.unpack('!I', png[offset+8+size:offset+12+size])[0]
            self.assertEqual(crc, zlib.crc32(kind+data))
            chunks[kind] = data
            offset += 12+size
        self.assertEqual(zlib.decompress(chunks[b'IDAT']), b'\0\xff\0\0\0\0\0\xff')


if __name__ == '__main__':
    unittest.main()
