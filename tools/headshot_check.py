#!/usr/bin/env python3
"""Headshots check (src/game/vcs_headshots.cpp, docs/game-internals.md 12c "Headshots"): Game > Combat >
Headshots always kill, checked in the game.

    python tools/headshot_check.py [OUT] [--port 47677]

Loads a rig save copy (hidden), turns free aim on and gives Vic the pistol. For each shot: a man is created on the
camera's line through the circle 4 m past Vic, the aim is tilted until that line passes through his head (or his chest),
one shot is fired, and the man's health and the `headshot` command's counters (the player's hits by body piece) are
read. Checked:
- the weapon info's fire types (instant hit = 1 for the pistol and the shotgun; 0 for fists);
- on: a pistol shot at the head hits piece 6 and kills the man;
- on: a pistol shot at the chest hits another piece and takes the same health as with the setting off (other hits
  unchanged);
- off: a pistol shot at the head hits piece 6 and the damage is the original game's (no raise).
Results: OUT/summary.txt; a screenshot after each shot. Default OUT: build/headshot/<date-time>.
"""
import argparse
import math
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rig  # noqa: E402
from controls_check import Check as ControlsCheck  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEAPON_INFO = 0x712770  # *(*this + 4) + type * 0x70; +4 the fire type
PISTOL, SHOTGUN = 19, 21


class Check(ControlsCheck):
    def counters(self):
        r = self.g.cmd("headshot")
        d = dict(re.findall(r"(\w+)=([\d,]+)", r))
        return [int(x) for x in d.get("hits", "0").split(",")], int(d.get("raised", 0)), r

    def fire_types(self):
        table = self.g.u32(self.g.u32(WEAPON_INFO) + 4)
        return [self.g.u32(table + t * 0x70 + 4) for t in range(44)]  # the table's rows (vcs_headshots.cpp)

    def shoot_at(self, k, height, name):
        """A man 4 m out on the camera's line, the aim through him `height` metres above his position (+0x30, about
        the hips), one pistol shot: (health before, after, pieces hit, raised)."""
        g = self.g
        cam = self.CAM
        g.teleport(575.0 - 12.0 * k, 135.0, -100.0, 0.0)  # land from x 575 to ~400 (west of ~300: water)
        time.sleep(2.0)
        self.give(PISTOL, 100)
        g.cmd("key mouseright down")
        time.sleep(1.0)
        ped = g.player()[0]
        (sx, sy, _), (dx, dy, _) = g.peekf(cam + 0x20, 3), g.peekf(g.u32(ped + self.PED_TARGET) + 0x30, 3)
        (px, py, _) = g.peekf(ped + 0x30, 3)
        h = math.hypot(dx - sx, dy - sy)
        t = math.hypot(px - sx, py - sy) + 4.0
        man = self.ped_at(sx + (dx - sx) / h * t, sy + (dy - sy) / h * t)
        time.sleep(0.8)
        for _ in range(4):  # the camera's line through his head / chest
            (sx, sy, sz), (dx, dy, dz) = g.peekf(cam + 0x20, 3), g.peekf(g.u32(ped + self.PED_TARGET) + 0x30, 3)
            (qx, qy, qz) = g.peekf(man + 0x30, 3)
            along = math.hypot(qx - sx, qy - sy)
            ray = sz + (dz - sz) * along / math.hypot(dx - sx, dy - sy)
            g.cmd("mouse 0 %d" % round(-math.atan2(qz + height - ray, along) * 400))
            g.wait_frames(10)
        before = g.peekf(man + self.PED_HEALTH, 1)[0]
        hits0, raised0, _ = self.counters()
        g.cmd("key mouseleft down")
        time.sleep(0.15)
        g.cmd("key mouseleft up")
        time.sleep(1.0)
        after = g.peekf(man + self.PED_HEALTH, 1)[0]
        hits1, raised1, _ = self.counters()
        self.shot(name)
        g.cmd("key mouseright up")
        time.sleep(1.0)
        pieces = {i: hits1[i] - hits0[i] for i in range(len(hits1)) if hits1[i] != hits0[i]}
        return before, after, pieces, raised1 - raised0

    def run(self):
        g = self.g = rig.Game.start(save=rig.default_save(), name="headshot_check", port=self.port,
                                    data_dir=os.path.join(ROOT, "build", "headshot", "data"), env={"VCS_FPS": "30"})
        try:
            g.wait_ingame()
            self.ok("start from the defaults", self.setting("advanced.reset_all") == "pressed")
            g.prime()
            hooked = self.counters()[2]
            self.ok("the hook is in, on by default", "hooked=1" in hooked and " on=1" in hooked, hooked)
            types = self.fire_types()
            self.ok("fire types: fists 0, pistol / shotgun instant hit (1)",
                    types[0] == 0 and types[PISTOL] == 1 and types[SHOTGUN] == 1,
                    "types 0..43: %s" % " ".join(str(t) for t in types))
            self.setting("mouse.free_aim", 1)

            def head_shot(k, on, name):
                """The head at a few heights until piece 6 is hit (the head's height over +0x30 differs by pose)."""
                self.setting("enhance.headshots_kill", on)
                last = None
                for j, height in enumerate((0.62, 0.70, 0.55, 0.78)):
                    last = (height,) + self.shoot_at(k * 4 + j, height, "%s_%d" % (name, j))
                    if 6 in last[3]:
                        break
                return last

            # off first: the original game's damage to the head and the chest
            off_head = head_shot(0, 0, "off_head")
            def chest_shot(k, on, name):
                self.setting("enhance.headshots_kill", on)
                for j, height in enumerate((0.15, 0.25, 0.05)):  # a miss (no piece hit): again, a little higher / lower
                    r = self.shoot_at(k * 4 + j, height, "%s_%d" % (name, j))
                    if r[2]:
                        return r
                return r

            off_chest = chest_shot(1, 0, "off_chest")
            on_head = head_shot(2, 1, "on_head")
            on_chest = chest_shot(3, 1, "on_chest")

            def show(r):
                return "health %.0f -> %.0f, pieces %s, raised %d" % r[-4:] if len(r) == 5 else \
                    "health %.0f -> %.0f, pieces %s, raised %d" % r

            self.ok("off: a pistol shot at the head hits the head (piece 6), not raised",
                    6 in off_head[3] and off_head[4] == 0, "height %.2f: %s" % (off_head[0], show(off_head)))
            self.ok("on: a pistol shot at the head kills", 6 in on_head[3] and on_head[4] >= 1 and on_head[2] <= 0,
                    "height %.2f: %s" % (on_head[0], show(on_head)))
            chest_off, chest_on = off_chest[0] - off_chest[1], on_chest[0] - on_chest[1]
            self.ok("on: a chest shot does the original damage (not the head, not raised)",
                    on_chest[2] and 6 not in on_chest[2] and on_chest[3] == 0 and abs(chest_on - chest_off) < 0.5
                    and on_chest[1] > 0,
                    "off: %s; on: %s" % (show(off_chest), show(on_chest)))
        finally:
            g.quit()
        failed = [r for r in self.results if not r[1]]
        with open(os.path.join(self.out, "summary.txt"), "w", encoding="utf-8") as f:
            for name, good, detail in self.results:
                line = "%s  %s%s" % ("PASS" if good else "FAIL", name, ("  (%s)" % detail) if detail else "")
                print(line)
                f.write(line + "\n")
        print("[headshot_check] %s (%s)" % ("ok" if not failed else "%d failed" % len(failed), self.out))
        return not failed


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--port", type=int, default=47677)
    a = ap.parse_args()
    out = os.path.abspath(a.out or os.path.join(ROOT, "build", "headshot", time.strftime("%Y%m%d-%H%M%S")))
    os.makedirs(out, exist_ok=True)
    sys.exit(0 if Check(out, a.port).run() else 1)


if __name__ == "__main__":
    main()
