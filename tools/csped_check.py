#!/usr/bin/env python3
"""Cutscene characters in gameplay (tools/csped.py): Vic and story characters photographed with and without the mod.

    python tools/csped_check.py [OUT] [--save DIR] [--only mod|plain]

The mod is made first with the converter as it is now (`csped.py build` into OUT/mod: Vic's outfit models and the
characters below). Two hidden runs from a card copy (rig.default_save), one with no mod and one with it: the player
teleported to gamefiles_check's street at noon, the free camera at five places around him (front, side, back, his
face, his right hand) and a picture at each; again with a pistol in his hand; then each story character below made as a
street ped in front of him and photographed. The bar: the game alive, no guest fault in the log, every picture
written, the game's log naming plr.mdl and each character's model as taken from the mod, and each character's picture
differing from the plain run's over the character (the same spot, the same clock: a converted model that drew nothing,
or the original, would not). How it looks is the player's to judge (quickcheck's "for the player").
"""
import argparse
import math
import os
import re
import shutil
import sys
import time
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gamefiles_check  # noqa: E402
import rig  # noqa: E402
import toolchain as tc  # noqa: E402

# (name, distance from the player, height above his position, angle from his facing in degrees)
VIEWS = (("front", 2.6, 0.2, 0.0), ("side", 2.6, 0.2, 90.0), ("back", 2.6, 0.3, 180.0), ("face", 0.9, 0.55, 15.0),
         ("hand", 0.8, -0.35, -60.0))
# story characters by model index (tools/img.py list): phil, lansm (Lance), lou, marty, jerry, diaz
CHARACTERS = (("phil", 147), ("lansm", 136), ("lou", 138), ("marty", 142), ("jerry", 135), ("diaz", 128),
              ("plr13", 166))  # plr13: the one outfit whose gameplay model stands turned (16 degrees off)
CONVERT = ["plr*"] + [name for name, _ in CHARACTERS]
DELETE_CHAR = 0x003F  # as framerate_audit.py
PISTOL = 19  # the pistol's weapon type (as driver_aim_check.py; GIVE_WEAPON_TO_CHAR selects it)
FAULTS = re.compile(r"fatal|guest fault|exception|unlisted-resume|-> dormant", re.I)


def heading(g, ped):
    """The player's heading (radians) from his matrix's forward vector (ped + 0x10: forward x, y)."""
    fx, fy = g.peekf(ped + 0x10, 2)
    return math.atan2(fy, fx)


def shoot(g, out, name):
    ped, (px, py, pz), _ = g.player()
    h = heading(g, ped)
    shots = []
    for view, dist, up, angle in VIEWS:
        a = h + math.radians(angle)
        cx, cy, cz = px + math.cos(a) * dist, py + math.sin(a) * dist, pz + up
        yaw = math.degrees(math.atan2(py - cy, px - cx))
        pitch = math.degrees(math.atan2(pz + 0.3 - cz, dist))
        g.campose(cx, cy, cz, yaw, pitch)
        time.sleep(1.5)
        shot = out / ("%s_%s.png" % (name, view))
        g.cmd("screenshot " + str(shot))
        shots.append(shot)
    g.campose(None, 0, 0, 0, 0)
    return shots


def characters(g, out, name):
    """Each story character made as a street ped 3 m in front of the player and photographed from 2.5 m, front-on
    (they walk off: the picture is taken at once), then deleted."""
    S = rig.Scm
    shots = []
    for who, model in CHARACTERS:
        ped, (px, py, pz), _ = g.player()
        h = heading(g, ped)
        x, y = px + math.cos(h) * 3.0, py + math.sin(h) * 3.0
        ints, floats = g.scm_outputs(
            S.op(S.REQUEST_MODEL, S.i(model)) + S.op(S.LOAD_ALL_MODELS_NOW) +
            S.op(S.CREATE_CHAR, S.i(4), S.i(model), S.f(x), S.f(y), S.f(-100.0), S.LOCAL0) +
            S.op(S.SET_CHAR_HEADING, S.LOCAL0, S.f(math.degrees(h) + 90.0)) +  # facing the player
            S.op(S.MARK_MODEL_AS_NO_LONGER_NEEDED, S.i(model)), 1)
        time.sleep(0.5)
        cx, cy = x + math.cos(h + math.pi) * 2.5, y + math.sin(h + math.pi) * 2.5
        g.campose(cx, cy, pz + 0.4, math.degrees(h), -3.0)
        time.sleep(1.2)
        shot = out / ("%s_char_%s.png" % (name, who))
        g.cmd("screenshot " + str(shot))
        shots.append(shot)
        g.scm(S.op(DELETE_CHAR, S.i(ints[0])))
        g.campose(None, 0, 0, 0, 0)
    return shots


def run(out, name, mod, save):
    env = {"VCS_MODS": "1"}
    if mod:
        root = out / "modloader_root"
        if root.exists():
            shutil.rmtree(root)
        shutil.copytree(mod, root / "Cutscene characters")
        env["VCS_MODLOADER_DIR"] = str(root)
    else:
        env["VCS_MODLOADER_DIR"] = str(out / "empty_modloader")
        (out / "empty_modloader").mkdir(parents=True, exist_ok=True)
    g = rig.Game.start(save=save, name="csped_" + name, seconds=420, env=env)
    x, y, hd = gamefiles_check.STREET
    shots = []
    try:
        g.wait_ingame(timeout=180)
        g.set_time(12)
        g.weather(0)
        g.invincible(True)
        for _ in range(6):
            g.teleport(x, y, -100.0, heading=hd)
            there = []
            for _ in range(2):
                time.sleep(3)
                _, (px, py, _pz), _ = g.player()
                there.append(abs(px - x) + abs(py - y) < 10.0)
            if all(there):
                break
        shots += shoot(g, out, name)
        # a pistol in his hand (the grip: the finger bone the game animates)
        S = rig.Scm
        g.scm(S.op(S.GIVE_WEAPON_TO_CHAR, S.PLAYER, S.i(PISTOL), S.i(200)))
        time.sleep(2)
        shots += shoot(g, out, name + "_pistol")
        shots += characters(g, out, name)
        alive = g.alive()
    finally:
        g.quit()
    log = Path(g.log_path).read_text(encoding="utf-8", errors="replace")
    return shots, log, alive


def differs(a, b, box=(330, 120, 630, 600)):
    """Mean absolute difference (0-255) of two screenshots over a box, from the PNGs (stdlib decode)."""
    pa, pb = read_png(a), read_png(b)
    x0, y0, x1, y1 = box
    total = n = 0
    for y in range(y0, y1, 2):
        ra, rb = pa[y], pb[y]
        for x in range(x0 * 3, x1 * 3, 6):
            total += abs(ra[x] - rb[x])
            n += 1
    return total / max(1, n)


def read_png(path):
    """An 8-bit RGB / RGBA PNG's rows as RGB bytes."""
    import struct
    import zlib
    d = Path(path).read_bytes()
    i, idat, w, h, kind = 8, b"", 0, 0, 2
    while i < len(d):
        n = struct.unpack(">I", d[i:i + 4])[0]
        k, c = d[i + 4:i + 8], d[i + 8:i + 8 + n]
        if k == b"IHDR":
            w, h = struct.unpack(">II", c[:8])
            kind = c[9]
        elif k == b"IDAT":
            idat += c
        i += 12 + n
    raw, bpp = zlib.decompress(idat), 4 if kind == 6 else 3
    stride, rows, prev, p = w * bpp, [], bytearray(w * bpp), 0
    for _ in range(h):
        f, line = raw[p], bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pa_, pb_, pc_ = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa_ <= pb_ and pa_ <= pc_ else b if pb_ <= pc_ else c)) & 255
        prev = line
        rows.append(bytes(line) if bpp == 3 else bytes(v for j in range(w) for v in line[j * 4:j * 4 + 3]))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?", default=os.path.join(tc.ROOT, "build", "csped_check"))
    ap.add_argument("--save", default=None)
    ap.add_argument("--only", choices=("mod", "plain"))
    a = ap.parse_args()
    out = Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    save = a.save or rig.default_save()
    mod = out / "mod"
    if mod.exists():
        shutil.rmtree(mod)
    import csped
    if csped.build(CONVERT, str(mod), None) != 0:
        print("csped_check: FAIL (the converter made nothing)")
        return 1
    ok = True
    for name, folder in (("plain", None), ("mod", mod)):
        if a.only and a.only != name:
            continue
        shots, log, alive = run(out, name, folder, save)
        faults = [line for line in log.splitlines() if FAULTS.search(line)]
        missing = [s.name for s in shots if not s.exists()]
        print("%-5s alive %s, %d pictures (%d missing), log faults %d%s" % (
            name, alive, len(shots), len(missing), len(faults), "".join("\n      " + f for f in faults[:5])))
        ok &= alive and not missing and not faults
        if name == "mod":
            taken = set(re.findall(r"Cutscene characters[\\/](\w+)\.mdl -> GTA3PS2\.IMG", log))
            want = {"plr"} | {who for who, _ in CHARACTERS}
            print("      models taken from the mod: %d (%s)" % (
                len(taken), "missing " + " ".join(sorted(want - taken)) if want - taken else "all needed"))
            ok &= want <= taken
    if not a.only:
        for who, _ in CHARACTERS:
            d = differs(out / ("plain_char_%s.png" % who), out / ("mod_char_%s.png" % who))
            print("      %-6s picture difference over the character: %.1f / 255%s" % (
                who, d, "" if d >= 2.0 else "  <- the same picture"))
            ok &= d >= 2.0
    print("csped_check:", "PASS" if ok else "FAIL", "->", out)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
