#!/usr/bin/env python3
"""Mod loader check (src/mods/modloader.cpp, docs/MODS.md "The mod loader"): mods found, ignored and prioritised as
modloader.ini says, and F2 > Mods' switches applied while the game runs.

    python tools/mods_check.py [OUT] --pack DIR [--port 47666]

Builds a modloader/ under OUT with no modloader.ini: "HD" (a link to the texture pack DIR), "Tinted" (half the textures
the game dumps at a city spot, painted magenta), "_ignore" (Mod Loader's default ignore entry) and ".hidden" (not a
mod). Then one hidden run (VCS_TEST_MODS=1) at that spot. The game makes modloader.ini in the folder (both mods at 50,
_ignore ignored); the steps switch with the `setting` command as F2 does: both at 50 (HD wins the shared textures:
little magenta), Tinted's priority slider to 60 (magenta; "Tinted=60" written into modloader.ini), Tinted off (no
magenta, HD stays), Texture packs off (the game's own: it governs the mods' textures), on again, Load mods off. Each step: a screenshot, the
magenta share, the mean difference from the game's own picture. Results: OUT/result.json. Exit code 0 = pass.

[Minty1804 fork] From willzmkv's (dejot) fork, fitted to this one: the dumps are PNG in VCS_TEXTURE_DIR/dumps (an
empty texture folder of the check's own, so the player's pack is not a source), a Tinted texture is a magenta PNG
of the dump's size, and the log lines are ours ([textures] pack changed, [texpack] can't read).
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
import struct
import zlib
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bench  # noqa: E402
import rig  # noqa: E402
import toolchain as tc  # noqa: E402

SPOT = bench.SPOTS[0]


def link(target, link_path):
    """A folder link: a junction on Windows (no rights needed), a symlink elsewhere."""
    if tc.IS_WINDOWS:
        subprocess.run(['cmd', '/c', 'mklink', '/J', str(link_path), str(target)], check=True, capture_output=True)
    else:
        os.symlink(str(target), str(link_path), target_is_directory=True)


def unlink(path):
    """Removes the link alone, never what it points to: rmdir takes a junction (and fails on a real folder with files
    in it, which is then left alone); unlink takes a symlink."""
    if tc.IS_WINDOWS:
        try:
            os.rmdir(path)
        except OSError:
            pass
    elif path.is_symlink():
        os.unlink(path)


def picture_stats(path, reference=None):
    w, h, rows = tc.read_png(str(path))
    rows = rows[:h * 4 // 5]  # the HUD's bottom band aside
    # magenta as lit by the game: (243, 0, 197) in full light
    magenta = sum(1 for row in rows for x in range(0, w * 3, 3)
                  if row[x] > 150 and row[x + 1] < 90 and row[x + 2] > 120 and row[x] - row[x + 1] > 100)
    stats = {'magenta': round(magenta / (w * len(rows)), 4)}
    if reference:
        _, _, ref = tc.read_png(str(reference))
        total = count = 0
        for y in range(0, len(rows), 4):
            for x in range(0, w * 3, 12):
                total += abs(rows[y][x] - ref[y][x]) + abs(rows[y][x + 1] - ref[y][x + 1]) + abs(rows[y][x + 2] - ref[y][x + 2])
                count += 3
        stats['diff_from_original'] = round(total / count, 2)
    return stats


def magenta_png(src, dst):
    """A PNG of src's size, all magenta (opaque): an unmistakable replacement."""
    data = Path(src).read_bytes()
    w, h = struct.unpack('>II', data[16:24])
    raw = b''.join(b'\x00' + b'\xff\x00\xff\xff' * w for _ in range(h))

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff)
    Path(dst).write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
                          chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def start(name, out, env, port):
    # [Minty1804 fork] from the card copy, as this fork's checks: a new game's opening cutscene began partway through
    # the steps (the pictures then compared an office with the street)
    g = rig.Game.start(save=rig.default_save(), name=name, seconds=400, data_dir=str(out / ('data_' + name)), port=port,
                       env=dict({'VCS_ASPECT': '43', 'VCS_PERF_OVERLAY': '0'}, **env))
    g.wait_ingame()
    g.set_time(12)
    g.weather(0)
    g.invincible()
    g.teleport(SPOT[1], SPOT[2], -100, heading=SPOT[3])
    time.sleep(6)
    return g


def check(out, pack, port):
    out.mkdir(parents=True, exist_ok=True)
    mods = out / 'modloader'
    if mods.is_dir():
        if (mods / 'HD').exists() or os.path.lexists(mods / 'HD'):
            unlink(mods / 'HD')  # the link to the player's pack goes first, on its own
        if os.path.lexists(mods / 'HD'):
            raise RuntimeError('could not remove the link %s: remove it by hand (not the folder it points to)' % (mods / 'HD'))
        shutil.rmtree(mods, ignore_errors=True)
    tex = out / 'tex'  # the run's own texture folder: no pack of the player's, the dumps in tex/dumps
    dumps = tex / 'dumps'
    if not any(dumps.glob('*.png')):
        g = start('mods_dump', out, {'VCS_TEXTURE_DUMP': '1', 'VCS_TEXTURE_DIR': str(tex)}, port)
        g.quit()
    names = sorted(p.name for p in dumps.glob('*.png'))
    if not names:
        raise RuntimeError('no textures dumped')
    (mods / 'Tinted').mkdir(parents=True)
    for name in names[::2]:
        magenta_png(dumps / name, mods / 'Tinted' / name)
    link(Path(pack).resolve(), mods / 'HD')
    (mods / '_ignore').mkdir()
    shutil.copy(dumps / names[0], mods / '_ignore' / names[0])
    (mods / '.hidden').mkdir()

    env = {'VCS_TEST_MODS': '1', 'VCS_MODLOADER_DIR': str(mods), 'VCS_TEXTURE_DIR': str(tex)}
    steps = [('tie', None), ('all_on', 'mods.tinted.priority 60'), ('tinted_off', 'mods.tinted 0'),
             ('textures_off', 'graphics.texture_pack 0'), ('textures_on', 'graphics.texture_pack 1'),
             ('mods_off', 'mods.enabled 0')]
    shots = {}
    g = start('mods_check', out, env, port)
    ini = mods / 'modloader.ini'
    ini_made = ini.read_text(encoding='utf-8', errors='replace') if ini.is_file() else ''
    try:
        for step, command in steps:
            if command:
                reply = g.cmd('setting ' + command)
                if not reply.startswith('ok') and not reply[:1].isdigit():
                    raise RuntimeError('setting %s: %s' % (command, reply))
            time.sleep(5)  # the switch at the next frame, the pack's images loaded in the background
            shots[step] = out / ('%s.png' % step)
            g.cmd('screenshot ' + str(shots[step]))
    finally:
        g.quit()
    log = Path(g.log_path).read_text(encoding='utf-8', errors='replace')
    (out / 'run.log').write_text(log, encoding='utf-8')
    result = {'dumped': len(names), 'tinted': len(names[::2])}
    for step, _ in steps:
        result[step] = picture_stats(shots[step], shots['textures_off'])
    found = re.search(r'\[mods\] (\d+) mod\(s\)', log)
    result['mods_found'] = int(found.group(1)) if found else -1
    result['ignored'] = '[mods] _ignore: ignored by modloader.ini' in log
    result['switches'] = len(re.findall(r'\[textures\] pack changed', log))
    result['tinted_in_use'] = re.findall(r'\[textures\]   Tinted \(priority (\d+)\): (\d+) in use', log)
    ini_after = ini.read_text(encoding='utf-8', errors='replace') if ini.is_file() else ''
    result['ini_made'] = all(t in ini_made for t in ('[Profiles.Default.Priority]', 'HD=50', 'Tinted=50', '_ignore'))
    result['ini_slider'] = 'Tinted=60' in ini_after and 'Tinted=50' not in ini_after and 'HD=50' in ini_after
    ok = (result['mods_found'] == 2 and result['ignored'] and result['ini_made'] and result['ini_slider'] and
          result['switches'] >= 6 and
          result['all_on']['magenta'] > max(0.005, 3 * result['tie']['magenta']) and
          result['tinted_off']['magenta'] < 0.005 and
          result['textures_on']['magenta'] < 0.005 and
          result['tinted_off']['diff_from_original'] > 2 * max(result['mods_off']['diff_from_original'], 0.5) and
          '[crash]' not in log and "[texpack] can't read" not in log)
    result['pass'] = ok
    (out / 'result.json').write_text(json.dumps(result, indent=1), encoding='utf-8')
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('out', nargs='?')
    ap.add_argument('--pack', required=True, help='a texture pack folder (PCSX2 names), linked in as the "HD" mod')
    ap.add_argument('--port', type=int, default=47666)
    a = ap.parse_args()
    out = Path(a.out or os.path.join(tc.ROOT, 'build', 'mods_check', time.strftime('%Y%m%d-%H%M%S'))).resolve()
    result = check(out, a.pack, a.port)
    print(json.dumps(result, indent=1))
    print('PASS' if result['pass'] else 'FAIL', '-', out)
    return 0 if result['pass'] else 1


if __name__ == '__main__':
    sys.exit(main())
