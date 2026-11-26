#!/usr/bin/env python3
"""Cutscene characters in gameplay (tools/csped.py): Vic photographed with and without the converted models.

    python tools/csped_check.py [OUT] [--mod DIR] [--save DIR] [--only mod|plain]

Two hidden runs from a card copy (rig.default_save), one with no mod and one with the mod folder (default:
build/csped/mod, made by `csped.py build --out build/csped/mod`): the player teleported to gamefiles_check's street at
noon, then the free camera at four places around him (front, side, back, a close look at the face and hands) and a
picture at each; then with a pistol raised (the arms and the grip). The bar: the game alive, no guest fault in the log,
and the player drawn in every picture (the pictures of both runs are written next to each other for a look). What
it does not judge: how it looks - that is the player's (quickcheck's "for the player").
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
         ("far", 25.0, 0.6, 20.0))
# story characters by model index (tools/img.py list): phil, lansm (Lance), lou, marty, jerry, diaz
CHARACTERS = (("phil", 147), ("lansm", 136), ("lou", 138), ("marty", 142), ("jerry", 135), ("diaz", 128))
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


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?", default=os.path.join(tc.ROOT, "build", "csped_check"))
    ap.add_argument("--mod", default=os.path.join(tc.ROOT, "build", "csped", "mod"))
    ap.add_argument("--save", default=None)
    ap.add_argument("--only", choices=("mod", "plain"))
    a = ap.parse_args()
    out = Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    save = a.save or rig.default_save()
    ok = True
    for name, mod in (("plain", None), ("mod", a.mod)):
        if a.only and a.only != name:
            continue
        shots, log, alive = run(out, name, mod, save)
        faults = [line for line in log.splitlines() if FAULTS.search(line)]
        missing = [s.name for s in shots if not s.exists()]
        files = [line for line in log.splitlines() if "[mods]" in line and ".mdl" in line][:6]
        print("%-5s alive %s, %d pictures (%d missing), log faults %d%s" % (
            name, alive, len(shots), len(missing), len(faults), "".join("\n      " + f for f in faults[:5])))
        for f in files:
            print("      " + f.strip())
        ok &= alive and not missing and not faults
    print("csped_check:", "PASS" if ok else "FAIL", "->", out)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
