#!/usr/bin/env python3
"""HUD "Screen edges" check (src/game/vcs_hud.cpp, game-internals.md 6f "Radar and text"): the radar stays at the left
edge and centred text lines in the middle, whatever else is on screen.

    python tools/hud_check.py [OUT] [--port 47630] [--control]

First a 15 s walk in the rain with PS2X_HW_HUD_WATCH=1 (2026-11-19, the user's report: the top-right HUD doubled or
with artefacts for single frames): 2D the game draws after the HUD in the same frame - rain drops, radar blips, fades
- must go over the HUD layer, never make it stale in its own frame (no such [hud-watch] line from the rain on).
--control runs with PS2X_HW_HUD_OVER=0, the layer as before, and passes when the rain does break it.

From the Fort Baxter card at 16:9 with the HUD at the screen edges (VCS_ASPECT=169, VCS_HUD=2; hidden): a car at the
airport thrown into the air three times, so the game shows its insane stunt texts ("Distance: ... Height: ...", "INSANE
STUNT BONUS") under the help texts the card still has. The anchors come from VCS_TRACE_HUDANCHOR. Passes when every
radar primitive went to the left edge and every centred line to the middle (its box centred on x 320 of 640, the stunt
lines' right-shifted rect moved there too), in enough frames with a centred line. What broke (reported by the user,
2026-11-01): the radar's map tiles reach far past its circle, so bottom text near it was grouped with the radar and
the pair flipped between the left edge and the middle as the map turned. Results: OUT/result.json and screenshots
(default OUT: build/hud_check/<date-time>). Exit code 0 = pass.
"""
import argparse
import json
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rig  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AIRPORT = (-1400.0, -1150.0)  # the apron at Escobar International: room to land a jump
CAR_MOVE_SPEED = 0x140        # the vehicle's move speed (game units per frame; 1.0 = 50 m/s)
ELEMENT = re.compile(r"\[hud-anchor\]\s+x\s+(-?\d+)\.\.\s*(-?\d+) y\s+(-?\d+)\.\.\s*(-?\d+) prims\s+\d+ part (\d)( menu)? -> (\w+)")


def read_anchors(log):
    """[(frame index, part, x0, x1, anchor)] from the VCS_TRACE_HUDANCHOR lines"""
    out, frame = [], -1
    with open(log, errors="replace") as f:
        for line in f:
            if line.startswith("[hud-anchor] frame"):
                frame += 1
                continue
            m = ELEMENT.match(line)
            if m and not m.group(6):
                out.append((frame, int(m.group(5)), int(m.group(1)), int(m.group(2)), m.group(7)))
    return out


def count_watch(log, start, end=None):
    """PS2X_HW_HUD_WATCH lines between two byte offsets of the log: (layers made stale in their own frame, layers
    restarted in it)"""
    stale = restarts = 0
    with open(log, "rb") as f:
        f.seek(start)
        text = f.read(-1 if end is None else end - start).decode(errors="replace")
        for line in text.splitlines():
            if line.startswith("[hud-watch]"):
                stale += "made stale in its own frame" in line
                restarts += "restarts within its frame" in line
    return stale, restarts


def check(out, port=None, control=False):
    os.makedirs(out, exist_ok=True)
    env = {"VCS_ASPECT": "169", "VCS_HUD": "2", "VCS_TRACE_HUDANCHOR": "100000", "VCS_TRACE_IM2D_DELAY": "0",
           "PS2X_HW_HUD_WATCH": "1"}
    if control:
        env["PS2X_HW_HUD_OVER"] = "0"
    g = rig.Game.start(save=rig.default_save(), name="hud_check", port=port,
                       env=env)
    try:
        g.wait_ingame()
        g.invincible(True)
        g.prime()
        # The HUD in the rain (fixed 2026-11-19): rain drops, radar blips and fades are 2D drawn after the HUD in the
        # same frame; each made the native HUD layer stale, and the frame showed the HUD missing or half drawn.
        g.weather(2)
        time.sleep(3)
        rain_from = os.path.getsize(g.log_path)
        g.cmd("press w 15000")
        for s in range(6):
            time.sleep(2.5)
            g.cmd("screenshot " + os.path.join(out, "rain_%02d.png" % s))
        rain_to = os.path.getsize(g.log_path)
        g.weather(0)
        g.teleport(AIRPORT[0], AIRPORT[1], -100.0)
        time.sleep(3)
        g.spawn_car(211)
        time.sleep(3)
        for k in range(3):
            car = g.u32(g.player()[0] + 0x450)
            _, (x, y, z), _ = g.player()
            g.cmd("pokef 0x%x 1 0 0 0 0 1 0 0 0 0 1 0 %f %f %f" % (car, x, y, z + 1.0))
            g.cmd("pokef 0x%x 0 0.6 0.3 0 0 0 0 0" % (car + CAR_MOVE_SPEED))
            for s in range(10):
                time.sleep(0.6)
                g.cmd("screenshot " + os.path.join(out, "jump%d_%02d.png" % (k, s)))
            time.sleep(1)
    finally:
        g.quit()
    rain_stale, rain_restarts = count_watch(g.log_path, rain_from, rain_to)
    stale, restarts = count_watch(g.log_path, rain_from)
    anchors = read_anchors(g.log_path)
    radar = [a for a in anchors if a[1] == 1]
    centred = [a for a in anchors if a[1] == 2]
    off_centre = [a for a in centred if abs((a[2] + a[3]) / 2.0 - 320.0) > 4.0]
    res = {
        "frames": len({a[0] for a in anchors}),
        "radar_prims": len(radar),
        "radar_not_left": len([a for a in radar if a[4] != "left"]),
        "centred_frames": len({a[0] for a in centred}),
        "centred_not_middle": len([a for a in centred if a[4] != "centre"]),
        "centred_off_centre": len(off_centre),
        "off_centre_examples": off_centre[:5],
        "rain_layer_stale": rain_stale,
        "rain_layer_restarts": rain_restarts,
        "layer_stale_after_rain_start": stale,
        "layer_restarts_after_rain_start": restarts,
        "control": control,
    }
    if control:  # PS2X_HW_HUD_OVER=0: the check must see the broken frames the fix removed
        res["pass"] = rain_stale > 0
    else:
        res["pass"] = (res["radar_prims"] > 0 and res["radar_not_left"] == 0 and res["centred_frames"] >= 30 and
                       res["centred_not_middle"] == 0 and res["centred_off_centre"] == 0 and stale == 0 and
                       restarts == 0)
    json.dump(res, open(os.path.join(out, "result.json"), "w"), indent=1)
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--port", type=int)
    ap.add_argument("--control", action="store_true",
                    help="PS2X_HW_HUD_OVER=0 (the HUD layer as before 2026-11-19): passes when the rain breaks it")
    a = ap.parse_args()
    out = os.path.abspath(a.out or os.path.join(ROOT, "build", "hud_check", time.strftime("%Y%m%d-%H%M%S")))
    res = check(out, a.port, a.control)
    print(json.dumps(res))
    print("PASS" if res["pass"] else "FAIL", "-", out)
    return 0 if res["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
