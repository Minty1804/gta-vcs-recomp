#!/usr/bin/env python3
"""Traffic at a raised Car distance: the car spawner's two gates (FORK_IMPORTS row 42, after willzmkv's bca7997;
src/game/vcs_world_detail.cpp findObjectsInRange, game-internals.md 12d).

    python tools/traffic_spawn_check.py [OUT] [--seconds S] [--spot X,Y] [--port N]

Two hidden sessions from a card copy (rig.default_save), standing in Little Havana at noon (willzmkv's place: the
gates threw 98-99 % of the cars built there away), one with the game's gates (VCS_CAR_SPAWN_GATES=game), one with
ours. In each, Car distance at 100 % then 200 % (Traffic density 100 %), S seconds each after a settle. The spawner's
funnel is counted in both (worlddetail): its calls, the spots it checked (clear10), the cars it built (clear20A), those
past clear20A (not_toward's spot check) and those it kept (carspawns). Checks:
  original   at 100 % no gate is touched in either session (the counters clear20a / towards stay)
  game       with VCS_CAR_SPAWN_GATES=game no gate is touched at 200 % either
  relaxed    with ours at 200 % the gates are relaxed (clear20a or towards moves) - skipped, with the funnel, when the
             game's spawner built next to no car there (nothing for a gate to throw away: not this change's to show)
  more       with ours at 200 % more cars on the street than with the game's gates (the mean over the S seconds) -
             skipped likewise, and when both sessions sit within 10 % of each other at the same count: the game's own
             limit on cars in use reached either way (what this PC showed in Little Havana: 18-20 both ways)
  waste      with ours at 200 % fewer of the built cars thrown away than with the game's gates (built - kept) - skipped
             when the game's spawner built next to no car
The ramp (cars each second from the teleport) is printed and kept in results.json: with the limit reached either way,
the change shows as the spawner wasting less of its work (the cars arrived sooner in one run, not in another).
OUT (default build/traffic_spawn_check) gets results.json and a screenshot per session. Exit code 1 when one fails.
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rig  # noqa: E402

LITTLE_HAVANA = (-1028.0, -348.5)  # hdworld_missions' k11_havana: a street in Little Havana
COUNTERS = ("spawncalls", "spots", "built", "survived", "carspawns", "clear20a", "towards")
MIN_BUILT = 20  # fewer cars built in S seconds: the spawner idle there, the gates have nothing to decide


def state(g):
    return dict(kv.split("=", 1) for kv in g.cmd("worlddetail").split() if "=" in kv)


def session(label, env, seconds, spot, port, out):
    g = rig.Game.start(save=rig.default_save(), name="traffic_spawn_" + label, port=port, env=env)
    row = {}
    try:
        g.wait_ingame()
        g.invincible()
        g.set_time(12)
        g.weather(0)
        for key in ("enhance.traffic", "enhance.car_distance", "enhance.peds", "enhance.draw_distance"):
            g.cmd("setting %s 100" % key)
        missing = [k for k in COUNTERS if k not in state(g)]
        if missing:
            raise RuntimeError("worlddetail has no %s: an older vcs.exe" % ", ".join(missing))

        def sample(name):
            g.teleport(spot[0], spot[1], -100.0)
            ramp = []
            for _ in range(8):
                time.sleep(1.0)
                ramp.append(int(state(g)["vehused"]))
            before = state(g)
            cars = []
            t0 = time.time()
            while time.time() - t0 < seconds:
                cars.append(int(state(g)["vehused"]))
                time.sleep(1.0)
            after = state(g)
            r = {k: int(after[k]) - int(before[k]) for k in COUNTERS}
            r["vehicles"] = sum(cars) / len(cars)
            r["ramp"] = ramp
            r["seconds"] = seconds
            print("[traffic_spawn_check] %s %s: vehicles %.1f; spawner calls %d, spots %d, built %d, past clear20A %d, "
                  "kept %d; relaxed clear20a %d towards %d; ramp %s" % (label, name, r["vehicles"], r["spawncalls"], r["spots"],
                                                               r["built"], r["survived"], r["carspawns"],
                                                               r["clear20a"], r["towards"], r["ramp"]), flush=True)
            return r

        row["100"] = sample("100%")
        g.cmd("setting enhance.car_distance 200")
        g.wait_frames(5)
        row["200"] = sample("200%")
        g.cmd("screenshot " + os.path.join(out, label + "_200.png"))
    finally:
        g.quit()
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", nargs="?", default=os.path.join(rig.ROOT, "build", "traffic_spawn_check"))
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--spot", default="%g,%g" % LITTLE_HAVANA, help="X,Y to stand at")
    ap.add_argument("--port", type=int, default=None)
    args = ap.parse_args()
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    spot = tuple(float(v) for v in args.spot.split(","))
    data = {"spot": spot,
            "game": session("game", {"VCS_CAR_SPAWN_GATES": "game"}, args.seconds, spot, args.port, out),
            "ours": session("ours", {}, args.seconds, spot, args.port, out)}
    checks = []

    def ok(name, cond, detail=""):
        status = "skip" if cond is None else "ok" if cond else "FAIL"
        checks.append({"name": name, "status": status, "detail": str(detail)})
        print("%-4s %-9s %s" % (status, name, detail), flush=True)

    g100, o100, g200, o200 = data["game"]["100"], data["ours"]["100"], data["game"]["200"], data["ours"]["200"]
    ok("original", o100["clear20a"] == 0 and o100["towards"] == 0 and g100["clear20a"] == 0 and g100["towards"] == 0,
       "ours %d / %d, game %d / %d" % (o100["clear20a"], o100["towards"], g100["clear20a"], g100["towards"]))
    ok("game", g200["clear20a"] == 0 and g200["towards"] == 0, "%d / %d" % (g200["clear20a"], g200["towards"]))
    funnel = "the game's spawner at 200 %%: %d built, %d kept in %.0f s" % (g200["built"], g200["carspawns"], args.seconds)
    idle = g200["built"] < MIN_BUILT
    ok("relaxed", None if idle else (o200["clear20a"] > 0 or o200["towards"] > 0),
       "%d / %d (%s)" % (o200["clear20a"], o200["towards"], funnel))
    capped = abs(o200["vehicles"] - g200["vehicles"]) <= 0.1 * max(g200["vehicles"], 1.0)
    ok("more", None if idle or capped else o200["vehicles"] > g200["vehicles"],
       "cars %.1f vs the game's gates %.1f%s; ramp %s vs %s" % (
           o200["vehicles"], g200["vehicles"], " (the same count: the limit on cars in use)" if capped else "",
           o200["ramp"], g200["ramp"]))
    waste_g, waste_o = g200["built"] - g200["carspawns"], o200["built"] - o200["carspawns"]
    ok("waste", None if idle else waste_o < waste_g,
       "built and thrown away %d vs the game's gates %d (%s)" % (waste_o, waste_g, funnel))
    data["checks"] = checks
    with open(os.path.join(out, "results.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    failed = [c for c in checks if c["status"] == "FAIL"]
    skipped = [c for c in checks if c["status"] == "skip"]
    print("%d checks, %d failed, %d skipped -> %s" % (len(checks), len(failed), len(skipped), out))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
