#!/usr/bin/env python3
"""The keyboard's PC control scheme (Keyboard > Control scheme: PC, docs/CONTROLS.md, src/game/vcs_input.cpp): every
default key, by situation, checked in the pad report it makes.

    python tools/pc_scheme_check.py [OUT] [--port 47674]

Loads the Fort Baxter rig save (a copy, hidden) with VCS_KEYS=1 (the PC scheme; the other test tools keep the PS2
scheme), then: on foot, each default key gives its PS2 button or stick direction, and the vehicle-only keys give
nothing; unarmed with Target held the fighting keys win (Space a heavy attack, not a jump); in a vehicle the same keys
mean the vehicle actions (W accelerates, Shift honks, R the radio) and the on-foot-only keys give nothing; Escape and P
pause; a rebound on-foot key keeps its vehicle binding (keys shared by situation, as on PC), a clashing one takes it.
Results: OUT/summary.txt. Default OUT: build/pc_scheme/<date-time>.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rig  # noqa: E402
from controls_check import (Check, SELECT, L3, R3, START, L2, R2, L1, R1, TRIANGLE, CIRCLE, CROSS,  # noqa: E402
                            SQUARE)

KEY_H, KEY_C = 72, 67

FOOT = [
    ("w", ("ly", 0)), ("s", ("ly", 255)), ("a", ("lx", 0)), ("d", ("lx", 255)),
    ("up", ("ly", 0)), ("down", ("ly", 255)), ("left", ("lx", 0)), ("right", ("lx", 255)),
    ("i", ("ry", 0)), ("k", ("ry", 255)), ("j", ("rx", 0)), ("l", ("rx", 255)),
    ("shift", CROSS), ("space", SQUARE), ("c", L3), ("mouseleft", CIRCLE), ("ctrl", CIRCLE),
    ("e", R2), ("q", L2), ("tab", L1), ("mousemiddle", R3), ("f", TRIANGLE), ("return", TRIANGLE), ("v", SELECT),
]
FOOT_NOTHING = ["h", "r"]  # the horn and the radio: vehicle keys only (R reloads on foot: no button)
CAR = [
    ("w", CROSS), ("s", SQUARE), ("a", ("lx", 0)), ("d", ("lx", 255)), ("up", CROSS), ("down", SQUARE),
    ("space", R1), ("mouseleft", CIRCLE), ("mouseright", R1), ("shift", L3), ("h", L3), ("r", L1),
    ("q", L2), ("e", R2), ("mousemiddle", L2 | R2), ("v", SELECT),
]  # F (exit) last, below
CAR_NOTHING = ["c", "tab"]  # crouch, action: on foot only


def expect(p, want):
    if isinstance(want, tuple):
        axis, value = want
        rest = {k: 128 for k in ("lx", "ly", "rx", "ry") if k != axis}
        return p[axis] == value and p["buttons"] == 0 and all(p[k] == v for k, v in rest.items())
    return p["buttons"] == want and all(p[k] == 128 for k in ("lx", "ly", "rx", "ry"))


def run(c):
    g = c.g
    g.wait_ingame()
    g.prime()
    g.cmd("trainer toggle god 1")
    time.sleep(2.0)
    c.ok("PC scheme in use", c.setting("keys.scheme") == "1", "keys.scheme=%s" % c.setting("keys.scheme"))

    bad = []
    for key, want in FOOT:
        p = c.held(key)
        if not expect(p, want):
            bad.append("%s -> %04x lx %d ly %d rx %d ry %d" % (key, p["buttons"], p["lx"], p["ly"], p["rx"], p["ry"]))
    c.ok("on foot: default keys", not bad, "; ".join(bad))
    bad = ["%s -> %04x" % (k, p["buttons"]) for k in FOOT_NOTHING for p in [c.held(k)] if not expect(p, 0)]
    c.ok("on foot: vehicle-only keys do nothing", not bad, "; ".join(bad))

    # fighting: unarmed, Target held
    ped = g.player()[0]
    slot = g.peek(ped + 0x789, 1)[0]
    if slot == 0:
        g.cmd("key mouseright down")
        time.sleep(0.5)
        try:
            space, shift, f = c.held("space"), c.held("shift"), c.held("f")
        finally:
            g.cmd("key mouseright up")
        c.ok("fighting: Space heavy attack, Shift block, F grab",
             space["buttons"] == R1 | CROSS and shift["buttons"] == R1 | SQUARE and f["buttons"] == R1 | TRIANGLE,
             "space %04x shift %04x f %04x" % (space["buttons"], shift["buttons"], f["buttons"]))
    else:
        c.ok("fighting: skipped (a weapon in hand, slot %d)" % slot, True)

    # keys shared by situation: an on-foot key rebound to the horn's second key (H) crouches on foot, honks in a car
    c.setting("keys.pc_crouch", KEY_H)
    p = c.held("h")
    c.ok("rebound on-foot key: H crouches on foot", p["buttons"] == L3 and c.setting("keys.pc_horn2") == str(KEY_H),
         "h -> %04x, pc_horn2=%s" % (p["buttons"], c.setting("keys.pc_horn2")))

    # in a vehicle
    g.teleport(575.0, 175.0, -100.0, 0.0)
    time.sleep(3.0)
    g.spawn_car(ahead=0.0)
    time.sleep(3.0)
    ped = g.player()[0]
    car = g.u32(ped + 0x480)
    c.ok("in a car", car != 0 and g.u32(ped + 0x8C4) == 0x37, "car 0x%x" % car)
    bad = []
    for key, want in CAR:
        p = c.held(key)
        if not expect(p, want):
            bad.append("%s -> %04x lx %d ly %d" % (key, p["buttons"], p["lx"], p["ly"]))
    c.ok("in a vehicle: default keys", not bad, "; ".join(bad))
    bad = ["%s -> %04x" % (k, p["buttons"]) for k in CAR_NOTHING for p in [c.held(k)] if not expect(p, 0)]
    c.ok("in a vehicle: on-foot-only keys do nothing", not bad, "; ".join(bad))
    p = c.held("h")
    c.ok("in a vehicle: H honks (the crouch rebound to it on foot)", p["buttons"] == L3, "h -> %04x" % p["buttons"])
    c.setting("keys.pc_crouch", KEY_C)
    p = c.held("f")
    time.sleep(3.0)
    c.ok("in a vehicle: F is Triangle, Vic gets out", p["buttons"] == TRIANGLE and g.u32(g.player()[0] + 0x8C4) != 0x37,
         "f -> %04x" % p["buttons"])

    # pause: Escape and P open the pause menu (it has the keys from then on), Escape closes it
    opened = []
    for key in ("escape", "p"):
        c.held(key)
        time.sleep(1.5)
        opened.append(g.cmd("menustate")[:20])
        g.cmd("press escape 150")
        time.sleep(2.0)
    c.ok("Escape and P open the pause menu", all(s.startswith("open=1") for s in opened), str(opened))
    c.ok("Escape closes it", g.cmd("menustate").startswith("open=0"), g.cmd("menustate")[:20])
    c.shot("after_pause")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--port", type=int, default=47674)
    a = ap.parse_args()
    out = os.path.abspath(a.out or os.path.join(rig.ROOT, "build", "pc_scheme", time.strftime("%Y%m%d-%H%M%S")))
    os.makedirs(out, exist_ok=True)
    c = Check(out, a.port)
    env = {"VCS_KEYS": "1"}
    c.g = rig.Game.start(save=rig.default_save(), name="pc_scheme", env=env, data_dir=os.path.join(out, "data"),
                         port=a.port)
    try:
        run(c)
    finally:
        c.g.quit()
    lines = ["%s %s %s" % ("ok  " if r else "FAIL", n, d) for n, r, d in c.results]
    open(os.path.join(out, "summary.txt"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
    failed = [n for n, r, _ in c.results if not r]
    print(("FAIL: " + ", ".join(failed)) if failed else "PASS", "-", out)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
