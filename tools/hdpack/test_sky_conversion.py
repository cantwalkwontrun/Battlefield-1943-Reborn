"""Focused pack compatibility and image invariants. Language: en-GB."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

import numpy as np
from PIL import Image

import convert_sky_pack as sky


class ConversionTests(unittest.TestCase):
    def test_panorama_wrap_is_continuous(self):
        gradient = np.zeros((1024, 4096, 3), np.uint8)
        gradient[:, :, 0] = np.arange(4096, dtype=np.uint16) % 256
        output = np.asarray(sky.prepare_panorama(Image.fromarray(gradient), {}))
        self.assertTrue(np.array_equal(output[:, 0], output[:, -1]))
        self.assertEqual(output.shape, (1024, 4096, 3))

    def test_crop_rejects_out_of_bounds(self):
        with self.assertRaisesRegex(ValueError, 'outside'):
            sky.prepare_panorama(Image.new('RGB', (4096, 1024)), {'crop': [-1, 0, 4096, 1024]})

    def test_strip_encoding_preserves_block_order_and_alpha(self):
        encoder = sky.load_module('encoder', Path(__file__).with_name('make_pack.py'))
        rgba = np.zeros((64, 64, 4), np.uint8)
        rgba[:, :, :3] = [40, 100, 200]
        rgba[:32, :, 3] = 31
        rgba[32:, :, 3] = 223
        strips = b''.join(encoder.encode(rgba[y:y + 32], 3) for y in range(0, 64, 32))
        whole = encoder.encode(rgba, 3)
        self.assertEqual(strips, whole)
        decoded = np.asarray(sky.dds_image(strips, 64, 64, 3))
        self.assertLessEqual(np.abs(decoded[:, :, 3].astype(int) - rgba[:, :, 3]).max(), 1)
        self.assertLess(np.abs(decoded[:, :, :3].astype(int) - rgba[:, :, :3]).mean(), 4)

    def make_pack(self, path, extra=b'', truncate=0, entries=1):
        size = sum(((max(1, 4096 >> m) + 3) // 4) * ((max(1, 1024 >> m) + 3) // 4) * 16 for m in range(13))
        raw = struct.pack('<4sIII', b'BFHD', 1, entries, 0)
        raw += b''.join(sky.INDEX.pack(123, 20, 2048, 512, 4096, 1024, 13, 3, 0,
                                       16 + entries * 56 + i * size, size) for i in range(entries))
        raw += bytes(size * entries) + extra
        path.write_bytes(raw[:-truncate] if truncate else raw)

    def test_valid_pack_layout(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'pack.bfhd'
            self.make_pack(path)
            self.assertEqual(len(sky.validate_pack(path)), 1)

    def test_truncated_pack_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'pack.bfhd'
            self.make_pack(path, truncate=1)
            with self.assertRaisesRegex(ValueError, 'mip data'):
                sky.validate_pack(path)

    def test_duplicate_keys_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'pack.bfhd'
            self.make_pack(path, entries=2)
            with self.assertRaisesRegex(ValueError, 'duplicate'):
                sky.validate_pack(path)

    def test_trailing_bytes_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'pack.bfhd'
            self.make_pack(path, extra=b'bad')
            with self.assertRaisesRegex(ValueError, 'Trailing'):
                sky.validate_pack(path)


if __name__ == '__main__':
    unittest.main()
