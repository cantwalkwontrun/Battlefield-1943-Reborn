"""Build a local BF1943 Reborn sky-only BFHD pack. Language: en-GB.

The original archives are read only. Replacement artwork stays external; the
original skydome alpha is retained so the game's horizon blend still applies.
HDR lighting cubemaps, clouds and other textures are deliberately not replaced.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image

import xxhash

SKIES = {
    'coral_sea': 'Objects/Beach_Objects/skydome/textures/sky_CS.res',
    'guadal_canal': 'Objects/Beach_Objects/skydome/textures/sky_GC.res',
    'iwo_jima_s': 'Objects/Beach_Objects/skydome/textures/sky_IJ.res',
    'wake_island_s': 'Objects/Beach_Objects/skydome/textures/sky_1.res',
    'wake_island_tutorial': 'Objects/Beach_Objects/skydome/textures/sky_1.res',
}
INDEX = struct.Struct('<QIIIIIIIIQQ')


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            value.update(chunk)
    return value.hexdigest()


def tiled_offset(x: int, y: int, width: int, size: int) -> int:
    """XGAddress2DTiledOffset, matching the game's local texture decoder."""
    aligned = (width + 31) & ~31
    log = (size >> 2) + ((size >> 1) >> (size >> 2))
    macro = ((x >> 5) + (y >> 5) * (aligned >> 5)) << (log + 7)
    micro = ((x & 7) + ((y & 6) << 2)) << log
    off = macro + ((micro & ~15) << 1) + (micro & 15) + ((y & 8) << (3 + log)) + ((y & 1) << 4)
    return (((off & ~511) << 3) + ((off & 448) << 2) + (off & 63) + ((y & 16) << 7)
            + (((((y & 8) >> 2) + (x >> 3)) & 3) << 6)) >> log


def dds_image(blocks: bytes, width: int, height: int, bc: int) -> Image.Image:
    # DDS_HEADER with the single top level, then the independently decoded data.
    words = [124, 0x81007, height, width, len(blocks), 0, 1] + [0] * 11
    words += [32, 4, int.from_bytes(b'DXT1' if bc == 1 else b'DXT5', 'little'), 0, 0, 0, 0, 0]
    words += [0x1000, 0, 0, 0, 0]
    return Image.open(io.BytesIO(b'DDS ' + struct.pack('<31I', *words) + blocks)).convert('RGBA')


def prepare_panorama(image: Image.Image, choice: dict) -> Image.Image:
    """Keep a 4:1 horizon-anchored crop, rotate, and blend the horizontal join."""
    if choice.get('crop'):
        left, top, right, bottom = choice['crop']
        if not (0 <= left < right <= image.width and 0 <= top < bottom <= image.height):
            raise ValueError('Crop lies outside replacement artwork')
    else:
        # These resource panoramas contain a little more vertical field than the
        # native 4:1 dome. Preserve their horizon rather than stretch the clouds.
        left, right, bottom = 0, image.width, image.height
        top = max(0, bottom - image.width // 4)
    image = image.crop((left, top, right, bottom)).convert('RGB').resize((4096, 1024), Image.Resampling.LANCZOS)
    pixels = np.asarray(image, dtype=np.float32).copy()
    shift = int(round(float(choice.get('horizontal_shift', 0)) * pixels.shape[1]))
    pixels = np.roll(pixels, shift, axis=1)
    # Source photos are not guaranteed to wrap. Blend only 64 pixels at each
    # edge; force the two outer columns to agree for bilinear wrap sampling.
    edge = (pixels[:, 0, :] + pixels[:, -1, :]) * 0.5
    for x in range(64):
        weight = (1.0 - x / 64.0) ** 2
        pixels[:, x, :] = pixels[:, x, :] * (1 - weight) + edge * weight
        pixels[:, -1 - x, :] = pixels[:, -1 - x, :] * (1 - weight) + edge * weight
    return Image.fromarray(np.clip(np.rint(pixels), 0, 255).astype(np.uint8), 'RGB')


def decode_original(res: bytes) -> tuple[int, int, int, Image.Image]:
    wrapper = 0x40 if res[:3] == b'RES' else 0
    if len(res) < wrapper + 0x5C + 65536:
        raise ValueError('Texture resource is too short')
    header = res[wrapper:wrapper + 0x5C]
    _, dimension, fmt, _, width, height = struct.unpack_from('>6I', header)
    if dimension != 0 or fmt != 2 or (width, height) != (2048, 512):
        raise ValueError(f'Unexpected sky layout: dimension={dimension}, format={fmt}, {width}x{height}')
    guest_format = 20  # Xenos DXT4_5; the BFHD renderer chooses BC3.
    guest = res[wrapper + 0x5C:]
    key = (xxhash.xxh3_64_intdigest(guest[:65536]) ^ (guest_format << 56)
           ^ (width << 32) ^ (height << 16))
    block_width, block_height, size = width // 4, height // 4, 16
    linear = bytearray(block_width * block_height * size)
    for y in range(block_height):
        for x in range(block_width):
            src = tiled_offset(x, y, block_width, size) * size
            dst = (y * block_width + x) * size
            if src + size > len(guest):
                raise ValueError('Truncated tiled sky texture')
            block = guest[src:src + size]
            linear[dst:dst + size] = bytes(v for pair in zip(block[1::2], block[::2]) for v in pair)
    return key, width, height, dds_image(bytes(linear), width, height, 3)


def validate_pack(path: Path) -> list[dict]:
    """Validate bounds, uniqueness and exact mip sizes before any installation."""
    length = path.stat().st_size
    with path.open('rb') as stream:
        header = stream.read(16)
        if len(header) != 16:
            raise ValueError('Truncated BFHD header')
        magic, version, count, reserved = struct.unpack('<4sIII', header)
        if magic != b'BFHD' or version != 1 or reserved or not 1 <= count <= 1000:
            raise ValueError('Invalid BFHD header')
        previous_end = 16 + INDEX.size * count
        keys, result = set(), []
        for _ in range(count):
            raw = stream.read(INDEX.size)
            if len(raw) != INDEX.size:
                raise ValueError('Truncated BFHD index')
            key, fmt, w, h, hw, hh, mips, bc, zero, offset, size = INDEX.unpack(raw)
            if key in keys or fmt != 20 or (w, h) != (2048, 512) or (hw, hh) != (4096, 1024):
                raise ValueError('Invalid or duplicate sky entry')
            keys.add(key)
            if zero or bc != 3 or mips != 13 or offset != previous_end:
                raise ValueError('Invalid BFHD sky layout')
            expected = sum(((max(1, hw >> m) + 3) // 4) * ((max(1, hh >> m) + 3) // 4) * 16
                           for m in range(mips))
            if size != expected or offset + size > length:
                raise ValueError('Invalid BFHD mip data size')
            previous_end = offset + size
            result.append({'key': f'{key:016x}', 'offset': offset, 'size': size, 'mips': mips})
        if previous_end != length:
            raise ValueError('Trailing or missing BFHD data')
    return result


def build(args) -> None:
    source, game, work = args.source.resolve(), args.game.resolve(), args.output.resolve()
    if work == game or game in work.parents or work in game.parents:
        raise ValueError('Output must be outside the original game directory')
    work.mkdir(parents=True, exist_ok=True)
    fbrb = load_module('fbrb_reader', source / 'tools/fbrb_extract.py')
    encoder = load_module('bfhd_encoder', source / 'tools/hdpack/make_pack.py')
    mapping = json.loads(args.mapping.read_text(encoding='utf-8'))
    replacements = mapping['replacements']
    original_dir, preview_dir = work / 'original', work / 'preview'
    original_dir.mkdir(exist_ok=True)
    preview_dir.mkdir(exist_ok=True)
    records, entries, blobs, seen = [], [], [], {}
    for level, resource in SKIES.items():
        archive = game / 'dist/xenon/levels' / level / 'reduced-level-00.fbrb'
        _, resources, data = fbrb.read(archive)
        entry = next((e for e in resources if e['name'] == resource and e['type'] == 'XenonTexture'), None)
        if entry is None or entry['offset'] + entry['size'] > len(data):
            raise ValueError(f'Missing or truncated sky resource: {level}/{resource}')
        res = data[entry['offset']:entry['offset'] + entry['size']]
        key, width, height, original = decode_original(res)
        choice = replacements[level]
        image_path = Path(choice['image']).resolve()
        original.save(original_dir / f'{level}.png')
        with Image.open(image_path) as image:
            image.load()
            if image.width < 4096 or image.height < 1024:
                raise ValueError(f'Replacement must contain actual HD source detail: {image_path.name}')
            replacement = prepare_panorama(image, choice)
        replacement.putalpha(original.getchannel('A').resize(replacement.size, Image.Resampling.LANCZOS))
        seam = max(np.asarray(replacement, dtype=np.int16)[:, :1, :3].__sub__(
            np.asarray(replacement, dtype=np.int16)[:, -1:, :3]).__abs__().mean(axis=(0, 1)))
        if key in seen:
            if seen[key] != (digest(image_path), choice.get('crop'), choice.get('horizontal_shift', 0)):
                raise ValueError('One original texture is mapped to conflicting replacements')
        else:
            seen[key] = (digest(image_path), choice.get('crop'), choice.get('horizontal_shift', 0))
            rgba = np.asarray(replacement)
            levels = encoder.mips_of(rgba)
            # Encode strips to bound peak memory while the main application is building.
            blob = bytearray()
            for mip in levels:
                for row in range(0, mip.shape[0], 32):
                    blob.extend(encoder.encode(mip[row:row + 32], 3))
            entries.append((key, 20, width, height, 4096, 1024, len(levels), 3, len(blob)))
            blobs.append(blob)
        replacement.save(preview_dir / f'{level}.png')
        records.append({'level': level, 'resource': resource, 'key': f'{key:016x}',
                        'original_resource_sha256': hashlib.sha256(res).hexdigest(),
                        'source_image': str(image_path), 'source_sha256': digest(image_path),
                        'crop': choice.get('crop', 'horizon-anchored 4:1'),
                        'horizontal_shift': choice.get('horizontal_shift', 0),
                        'seam_mean_channel_difference': round(float(seam), 3),
                        'original_size': [width, height], 'replacement_size': [4096, 1024]})
        print(f'{level}: {key:016x}, original alpha preserved', flush=True)
    pack = work / 'bf1943-sky-hd.bfhd'
    temporary = pack.with_suffix('.bfhd.part')
    offset = 16 + len(entries) * INDEX.size
    with temporary.open('wb') as stream:
        stream.write(struct.pack('<4sIII', b'BFHD', 1, len(entries), 0))
        for key, fmt, w, h, hw, hh, mips, bc, size in entries:
            stream.write(INDEX.pack(key, fmt, w, h, hw, hh, mips, bc, 0, offset, size))
            offset += size
        for blob in blobs:
            stream.write(blob)
    verified = validate_pack(temporary)
    temporary.replace(pack)
    evidence = {'language': 'en-GB', 'pack': pack.name, 'pack_sha256': digest(pack),
                'scope': 'Visible sky domes only; original HDR lighting cubemaps and alpha retained.',
                'runtime_verified': False, 'entries': verified, 'mapping': records,
                'licence': mapping.get('licence'), 'attribution': mapping.get('attribution'),
                'source_url': mapping.get('source_url')}
    (work / 'conversion-evidence.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    print(f'Validated {len(entries)} unique replacement textures: {pack}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--mapping', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build(args)


if __name__ == '__main__':
    main()
