"""Check a hidden mouse-flight run's observed inputs and flight. Language: en-GB."""
import argparse
from datetime import datetime, timedelta
import json
import math
from pathlib import Path
import re
import struct


def analyse(folder):
    meta = json.loads((folder / 'run.json').read_text(encoding='utf-8-sig'))
    text = (folder / 'client.log').read_text(encoding='utf-8', errors='replace')
    checks = []
    def check(name, passed):
        checks.append({'name': name, 'passed': bool(passed)})
    end = json.loads((folder / 'end.json').read_text(encoding='utf-8-sig')) if (folder / 'end.json').exists() else {}
    check('Run reached its deadline without an unexpected process exit', end.get('status') == 'Reached test deadline')
    check('Window remained hidden', 'Window kept hidden (window_start)' in text)
    check('No operating-system input requested', meta.get('os_input_sent') is False)
    check('SDK mouse emulation explicitly disabled', 'SDK mouse stick false' in text)
    check('No guest access violation', 'Unhandled guest access violation' not in text)
    entry = re.search(r'\[(.*?)\].*BF seek: in a vehicle', text)
    parts = meta.get('target_parts', 57)
    check(f'Reached the targeted {parts}-part vehicle', entry is not None and f'walking to the nearest {parts} part vehicle' in text)
    if meta.get('requires_target_confirmation'):
        check('Target tree confirmed near the seated player before scripting',
              f'BF seek: confirmed {parts} part vehicle within' in text)
    entered = datetime.fromisoformat(entry[1]) if entry else None
    motion, rates, views, pads, menus, surfaces, attitudes = [], [], [], [], [], [], []
    for line in text.splitlines():
        stamp = re.match(r'\[(.*?)\]', line)
        if not stamp:
            continue
        try:
            when = datetime.fromisoformat(stamp[1])
        except ValueError:
            continue
        if m := re.search(r'BF mouse flight: roll ([-.\d]+) pitch ([-.\d]+), encoded (-?\d+) (-?\d+)', line):
            motion.append((when, float(m[1]), float(m[2]), int(m[3]), int(m[4])))
        if m := re.search(r'BF test vehicle mouse: ([-.\d]+) ([-.\d]+) counts/s', line):
            rates.append((when, float(m[1]), float(m[2])))
        if m := re.search(r'BF view:.* A true pos ([-.\d]+) ([-.\d]+) ([-.\d]+)', line):
            views.append((when, *(float(v) for v in m.groups())))
        if m := re.search(r'BF control surface: concept (\d+) input ([-.\d]+) angle ([-.\d]+)', line):
            surfaces.append((when, int(m[1]), float(m[2]), float(m[3])))
        if m := re.search(r'attitude ([-.\d]+) ([-.\d]+) ([-.\d]+)', line):
            back_y, right_y, up_y = map(float, m.groups())
            attitudes.append((when, -math.degrees(math.asin(max(-1, min(1, back_y)))), math.degrees(math.atan2(right_y, up_y))))
        if m := re.search(r'BF pad:.* rx (-?\d+) ry (-?\d+) context (\d+) \((.*?)\)', line):
            pads.append((when, int(m[1]), int(m[2]), int(m[3]), m[4]))
        if 'BF menu command enterMenu (1)' in line and entered and when > entered + timedelta(seconds=1):
            menus.append(when)
    inversion = -1 if meta['scenario'] == 'inverted' else 1
    if meta['scenario'] != 'turret':
        for sign, axis, label in ((1, 1, 'roll right'), (-1, 1, 'roll left'), (1, 2, 'pitch up'), (-1, 2, 'pitch down')):
            check(label + ' observed from mouse input', any(sign * row[axis] > 0.02 for row in motion))
    for index, (when, x, y) in enumerate(rates):
        if x == y == 0:
            continue
        next_at = rates[index + 1][0] if index + 1 < len(rates) else when + timedelta(seconds=1)
        samples = [v for v in motion if when + timedelta(seconds=0.1) <= v[0] < next_at]
        # Handoff intentionally gives a keyboard key priority over mouse samples.
        if meta['scenario'] not in ('handoff', 'turret'):
            expected = (x / 1000, -y / 1000 * inversion)
            check(f'Mouse burst {index} reaches its calibrated axes', any(
                abs(v[1] - expected[0]) < 0.025 and abs(v[2] - expected[1]) < 0.025 for v in samples))
            concept, wanted = (6, x / 1000) if x else (5, y / 1000 * inversion)
            surface_samples = [v for v in surfaces if when + timedelta(seconds=0.12) <= v[0] < next_at and v[1] == concept]
            check(f'Mouse burst {index} survives the original vehicle input mapping', any(
                abs(v[2] - wanted) < 0.03 for v in surface_samples))
    response = []
    for when, x, y in rates:
        if max(abs(x), abs(y)) < 400 or not attitudes:
            continue
        before = min(attitudes, key=lambda v: abs((v[0] - when).total_seconds()))
        after = min(attitudes, key=lambda v: abs((v[0] - when).total_seconds() - 1))
        response.append({'x_counts_per_second': x, 'y_counts_per_second': y,
                         'pitch_change_degrees': round(after[1] - before[1], 4),
                         'bank_change_degrees': round(after[2] - before[2], 4)})
    strong_requested = sum(max(abs(float(x)), abs(float(y))) >= 400
                           for x, y in re.findall(r'mouse=([-\.\d]+):([-\.\d]+)', meta['script']))
    if meta['scenario'] != 'turret' and strong_requested:
        check('Every scripted strong sweep reaches the aircraft', len(response) == strong_requested)
        for concept, label in ((5, 'elevator'), (6, 'aileron')):
            check(f'Original {label} receives positive and negative input',
                  any(v[1] == concept and v[2] > 0.3 for v in surfaces) and
                  any(v[1] == concept and v[2] < -0.3 for v in surfaces))
            angles = [v[3] for v in surfaces if v[1] == concept]
            check(f'Original {label} physically rotates', bool(angles) and max(angles) - min(angles) > 0.01)
        if meta['scenario'] != 'handoff':
            for row in response:
                x, y = row['x_counts_per_second'], row['y_counts_per_second']
                if x:
                    check(f'Sweep {x:g} banks the aircraft in the commanded direction',
                          abs(row['bank_change_degrees']) > 5 and row['bank_change_degrees'] * x < 0)
                if y:
                    check(f'Sweep {y:g} pitches the aircraft in the commanded direction',
                          abs(row['pitch_change_degrees']) > 1 and row['pitch_change_degrees'] * y * inversion < 0)
    if meta['scenario'] == 'turret':
        target_samples = re.findall(r'BF vehicle mouse: part [0-9A-F]+ concept ([56]) angle ([-.\d]+) target ([-.\d]+).*input ([-.\d]+)', text)
        for concept, label in (('5', 'gun elevation'), ('6', 'turret yaw')):
            axis_samples = [(float(a), float(t), float(v)) for c, a, t, v in target_samples if c == concept]
            check(f'{label} retains a target-angle controller', bool(axis_samples))
            check(f'{label} moves in response to the mouse', any(abs(v) > 0.1 for _, _, v in axis_samples))
            check(f'{label} settles near its target', len(axis_samples) >= 4 and
                  all(abs(a - t) < 0.1 for a, t, _ in axis_samples[-4:]))
        check('Turret parts have no restoring force', bool(re.search(r'BF vehicle mouse data:.*reset force 0\.000', text)))
    idle_windows = []
    for index, (when, x, y) in enumerate(rates):
        if x or y or not entered or when < entered:
            continue
        next_at = rates[index + 1][0] if index + 1 < len(rates) else when + timedelta(seconds=1)
        if next_at - when < timedelta(seconds=0.4):
            continue
        samples = [v for v in motion if when + timedelta(seconds=0.2) <= v[0] < next_at]
        if samples:
            idle_windows.append(all(v[3] == v[4] == 0 for v in samples))
    if meta['scenario'] != 'turret':
        check('Stopped mouse produces exact neutral axes', bool(idle_windows) and all(idle_windows))
    if meta['scenario'] == 'handoff':
        check('Keyboard roll overrides opposing mouse input', any(p[1] == -32767 and p[3] == 0 for p in pads))
        check('Keyboard pitch overrides opposing mouse input', any(p[2] == 32767 and p[3] == 0 for p in pads))
    flight_end = min(menus) if menus else (views[-1][0] if views else entered)
    duration = (flight_end - entered).total_seconds() if entered and flight_end else 0
    track = [v for v in views if entered and entered <= v[0] < flight_end]
    altitude_gain = max((v[2] for v in track), default=0) - (track[0][2] if track else 0)
    horizontal_travel = sum(math.hypot(b[1] - a[1], b[3] - a[3]) for a, b in zip(track, track[1:])
                            if math.dist(a[1:], b[1:]) < 30)
    check('Vehicle gameplay lasts at least 30 seconds', duration >= 30)
    if meta['scenario'] != 'turret':
        check('Aircraft gains altitude above the carrier', altitude_gain > 10)
        check('Aircraft covers at least 500 metres', horizontal_travel > 500)
    sampled_seconds = (track[-1][0] - track[0][0]).total_seconds() if len(track) > 1 else 0
    # Average over the run: millisecond log timestamps quantise 120 FPS to
    # alternating 8/9 ms intervals, making the reciprocal median misleading.
    actual_fps = (len(track) - 1) / sampled_seconds if sampled_seconds > 0 else 0
    check('Observed frame sampling reaches the requested rate', actual_fps >= meta['frame_rate'] * 0.75)
    frames = []
    for raw in sorted((folder / 'frames').glob('*.bgra')):
        data = raw.read_bytes()
        width, height = struct.unpack_from('<II', data) if len(data) >= 8 else (0, 0)
        valid = 0 < width <= 8192 and 0 < height <= 8192 and len(data) == 8 + width * height * 4
        check(f'{raw.name} framebuffer complete', valid)
        if valid:
            from PIL import Image, PngImagePlugin
            metadata = PngImagePlugin.PngInfo()
            metadata.add_text('Language', 'en-GB')
            Image.frombytes('RGBA', (width, height), data[8:], 'raw', 'BGRA').save(raw.with_suffix('.png'), pnginfo=metadata)
            frames.append(raw.with_suffix('.png').name)
    check('GPU framebuffer readback captured', len(frames) == 2)
    result = {'language': 'en-GB', 'scenario': meta['scenario'], 'requested_fps': meta['frame_rate'],
              'observed_view_fps': round(actual_fps, 2), 'gameplay_seconds_after_entry': round(duration, 2),
              'altitude_gain_metres': round(altitude_gain, 2), 'horizontal_travel_metres': round(horizontal_travel, 2),
              'checks': checks, 'passed': all(v['passed'] for v in checks), 'frames': frames,
              'client_sha256': meta['client_sha256'], 'runtime_sha256': meta.get('runtime_sha256'),
              'strong_input_response': response}
    (folder / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(f"{folder.name}: {sum(v['passed'] for v in checks)}/{len(checks)} checks; {duration:.1f} s gameplay, "
          f"{altitude_gain:.1f} m altitude gain, {horizontal_travel:.1f} m travel, {actual_fps:.1f} FPS")
    for failed in (v for v in checks if not v['passed']):
        print('FAIL:', failed['name'])
    return result['passed']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    raise SystemExit(not analyse(parser.parse_args().run))
