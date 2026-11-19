#!/usr/bin/env python3
"""Aiming from the driver's seat and the mouse wheel's weapons in a vehicle (Keyboard > Free aim, Mouse wheel in a
vehicle; src/game/vcs_aim.cpp, src/game/vcs_input.cpp; after ChrisM-Pek's fork, docs/FORK_IMPORTS.md item 20).

    python tools/driver_aim_check.py [OUT] [--port 47693]

Loads the Fort Baxter rig save (a copy, hidden), gives Vic a pistol and an SMG and puts him in a car on the runway, then: the aim key (the right mouse button) aims from the driver's seat (aimstate
driver=1) and the game sees no R1 (no handbrake), the handbrake's key still gives R1; the mouse turns the camera and it
stays there; shots from each gun are counted down the camera ray and match the ammo used; the wheel switches between
the guns; W / D still drive and steer; letting go ends the aim; the pause menu ends it; the free camera ends it. Free aim
off: the right button is R1 again, the wheel still switches weapons; Mouse wheel in a vehicle: Radio leaves the
weapon alone. On a motorcycle the wheel switches weapons and the right button does not aim from the seat (cars only).
Results: OUT/summary.txt, screenshots OUT/*.png. Default OUT: build/driver_aim/<date-time>.
"""
import argparse
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rig  # noqa: E402
from controls_check import Check, R1  # noqa: E402
from framerate_audit import BIKE, RUNWAY_SPOT  # noqa: E402

PISTOL, SMG = 19, 25
THE_CAMERA = 0x6F44D0


def run(c):
    g = c.g
    s = rig.Scm
    g.wait_ingame()
    g.prime()
    g.cmd("trainer toggle god 1")
    x, y, heading = RUNWAY_SPOT
    g.teleport(x, y, -100.0, heading)
    g.wait_frames(90)
    for weapon in (PISTOL, SMG):
        g.scm(s.op(s.GIVE_WEAPON_TO_CHAR, s.PLAYER, s.i(weapon), s.i(300)))
    g.wait_frames(10)
    g.spawn_car()
    g.wait_frames(60)
    ped = g.player()[0]
    car = g.u32(ped + 0x480)
    c.ok("in a car, the driver", car and g.u32(car + 0x384) == ped, "car 0x%x" % car)

    def cam_front():
        cam = THE_CAMERA + 0x70 + 0x270 * g.peek(THE_CAMERA + 0x50, 1)[0]
        return g.u32(cam) & 0xFFFF, g.peekf(cam + 0x10, 3)

    def weapon():
        slot = g.peek(ped + 0x789, 1)[0]
        at = ped + 0x578 + slot * 0x1C
        return g.u32(at), g.u32(at + 0xC)

    def aim():
        return {k: int(v) for k, v in (w.split("=") for w in g.cmd("aimstate").split()[1:])}

    def fire():
        kind, ammo = weapon()
        before = aim()["shots"]
        g.cmd("press mouseleft 1600")
        g.wait_frames(60)
        kind2, ammo2 = weapon()
        shots = aim()["shots"] - before
        return kind, kind2, shots, ammo - ammo2

    def to_weapon(want):
        for _ in range(4):
            if weapon()[0] == want:
                return True
            g.cmd("press wheeldown 100")
            g.wait_frames(15)
        return weapon()[0] == want

    c.setting("mouse.free_aim", 1)
    c.setting("mouse.wheel_vehicle", 1)
    for name in ("keys",):
        c.ok("%s: weapons ready" % name, to_weapon(SMG), "weapon %d" % weapon()[0])
        mode, front = cam_front()
        g.cmd("key mouseright down")
        try:
            g.wait_frames(10)
            st, pad = aim(), c.pad()
            c.ok("%s: the right button aims from the seat, no R1 for the game" % name,
                 st["driver"] == 1 and not pad["buttons"] & R1, "aimstate %s buttons %04x" % (st, pad["buttons"]))
            hb = c.held("space")
            c.ok("%s: Space is still the handbrake (R1) while aiming" % name, hb["buttons"] & R1,
                 "buttons %04x" % hb["buttons"])
            g.cmd("mouse 150 20")
            g.wait_frames(10)
            mode2, front2 = cam_front()
            g.wait_frames(60)
            _, front3 = cam_front()
            c.ok("%s: the mouse turns the car camera, and it stays" % name,
                 mode == 18 and math.dist(front, front2) > 0.15 and math.dist(front2, front3) < 0.02,
                 "mode %d, moved %.3f, then %.3f" % (mode, math.dist(front, front2), math.dist(front2, front3)))
            c.shot("reticle_" + name)
            k0, k1, shots, used = fire()
            c.ok("%s: SMG shots down the camera ray = ammo used" % name, k0 == k1 == SMG and shots > 0 and shots == used,
                 "weapon %d->%d, shots %d, ammo used %d" % (k0, k1, shots, used))
            g.cmd("press wheeldown 100")
            g.wait_frames(15)
            k0, k1, shots, used = fire()
            c.ok("%s: the wheel to the pistol, its shots too" % name,
                 k0 == k1 == PISTOL and shots > 0 and shots == used,
                 "weapon %d->%d, shots %d, ammo used %d" % (k0, k1, shots, used))
            g.cmd("press wheelup 100")
            g.wait_frames(15)
            c.ok("%s: wheel up, back to the SMG" % name, weapon()[0] == SMG, "weapon %d" % weapon()[0])
            pos0, fwd0 = g.peekf(car + 0x30, 2), g.peekf(car + 0x10, 2)
            g.cmd("key w down")
            g.cmd("key d down")
            g.wait_frames(40)
            g.cmd("key d up")
            g.cmd("key w up")
            c.ok("%s: W / D drive and steer while aiming" % name,
                 math.dist(pos0, g.peekf(car + 0x30, 2)) > 1.0 and math.dist(fwd0, g.peekf(car + 0x10, 2)) > 0.1 and
                 aim()["driver"] == 1, "moved %.2f, turned %.3f" % (math.dist(pos0, g.peekf(car + 0x30, 2)),
                                                                    math.dist(fwd0, g.peekf(car + 0x10, 2))))
        finally:
            g.cmd("key mouseright up")
        g.wait_frames(10)
        c.ok("%s: letting go ends the aim" % name, aim()["driver"] == 0, g.cmd("aimstate"))
        g.cmd("key s down")  # stand still for the next round
        g.wait_frames(90)
        g.cmd("key s up")

    # the pause menu and the free camera end the aim
    g.cmd("key mouseright down")
    try:
        g.wait_frames(10)
        on = aim()["driver"]
        g.cmd("press escape 150")
        time.sleep(1.5)
        paused = aim()["driver"]
        menu = g.cmd("menustate")[:20]
        g.cmd("press escape 150")
        time.sleep(1.5)
        # a key held while the pause menu closes is the menu's until it is let go (game/vcs_frontend.h)
        g.cmd("key mouseright up")
        g.wait_frames(5)
        g.cmd("key mouseright down")
        g.wait_frames(10)
        back = aim()["driver"]
        g.cmd("trainer toggle freecam 1")
        g.wait_frames(10)
        free = aim()["driver"]
        g.cmd("trainer toggle freecam 0")
        g.wait_frames(10)
    finally:
        g.cmd("key mouseright up")
    c.ok("%s: the pause menu and the free camera end the aim" % name,
         on == 1 and paused == 0 and menu.startswith("open=1") and back == 1 and free == 0,
         "aiming %d, paused %d (%s), again %d, free camera %d" % (on, paused, menu, back, free))

    # free aim off: the right button is R1 (the PC scheme's secondary fire); the wheel still switches weapons
    c.setting("mouse.free_aim", 0)
    station = g.peek(car + 0x2BA, 1)
    w0 = weapon()[0]
    g.cmd("press wheeldown 100")
    g.wait_frames(40)
    w1 = weapon()[0]
    p = c.held("mouseright")
    c.ok(name + ": free aim off: the right button is R1, no aim; the wheel switches weapons, the radio untouched",
         p["buttons"] & R1 and aim()["driver"] == 0 and w1 != w0 and w1 in (PISTOL, SMG) and
         g.peek(car + 0x2BA, 1) == station,
         "buttons %04x, weapon %d->%d, station %s->%s" % (p["buttons"], w0, w1, station, g.peek(car + 0x2BA, 1)))
    c.setting("mouse.wheel_vehicle", 0)
    g.cmd("press wheeldown 100")
    g.wait_frames(40)
    c.ok(name + ": Mouse wheel in a vehicle: Radio leaves the weapon", weapon()[0] == w1, "weapon %d" % weapon()[0])
    c.setting("mouse.wheel_vehicle", 1)

    # a motorcycle: the wheel switches weapons; no aim from the seat (cars only)
    c.setting("mouse.free_aim", 1)
    z = g.player()[1][2]
    g.scm(s.op(0x0216, s.PLAYER, s.f(x), s.f(y), s.f(z)))  # WARP_CHAR_FROM_CAR_TO_COORD
    g.wait_frames(25)
    z = g.player()[1][2] + 0.5
    g.scm(s.op(s.REQUEST_MODEL, s.i(BIKE)) + s.op(s.LOAD_ALL_MODELS_NOW) +
          s.op(s.CREATE_CAR, s.i(BIKE), s.f(x + 4), s.f(y), s.f(z), s.LOCAL0) +
          s.op(s.WARP_CHAR_INTO_CAR, s.PLAYER, s.LOCAL0))
    g.wait_frames(40)
    bike = g.u32(ped + 0x480)
    before = weapon()[0]
    g.cmd("press wheeldown 100")
    g.wait_frames(15)
    after = weapon()[0]
    g.cmd("key mouseright down")
    g.wait_frames(10)
    driver = aim()["driver"]
    g.cmd("key mouseright up")
    c.ok(name + ": a motorcycle: the wheel switches weapons, no aim from the seat",
         bike and bike != car and after != before and after in (PISTOL, SMG) and driver == 0,
         "bike 0x%x, weapon %d->%d, driver %d" % (bike, before, after, driver))
    c.shot("bike_" + name)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--port", type=int, default=47693)
    a = ap.parse_args()
    out = os.path.abspath(a.out or os.path.join(rig.ROOT, "build", "driver_aim", time.strftime("%Y%m%d-%H%M%S")))
    os.makedirs(out, exist_ok=True)
    c = Check(out, a.port)
    c.g = rig.Game.start(save=rig.default_save(), name="driver_aim", data_dir=os.path.join(out, "data"), port=a.port)
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
