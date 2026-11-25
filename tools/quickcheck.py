#!/usr/bin/env python3
"""The checks a change needs, from the files it touched - with what they cost before anything runs (DECISIONS.md
2026-11-24: hours of checks for one batch of imports were not worth it).

    python tools/quickcheck.py                       # the plan for the uncommitted changes (git), nothing run
    python tools/quickcheck.py --base main            # ... for everything since main
    python tools/quickcheck.py --files src/game/vcs_prompts.cpp tools/menu_check.py
    python tools/quickcheck.py --since 3              # a folder without git: files modified in the last 3 hours
    python tools/quickcheck.py --run [--budget 20]    # run them, in turn, the cheapest first, within 20 minutes
    python tools/quickcheck.py --full                 # the whole suite instead (once, at the end of a batch)

Every check plays the game in real time, one after another (two at once slow each other down and their numbers stop
meaning anything), so the plan says how long it takes. What the player can see for themselves in a minute (a menu's
look, a prompt, how a control feels) is listed apart, under "for the player": ask them instead of spending a run on it.

Build first (tools/build.py); the checks take the vcs.exe of the build folder (tools/build_dir.py, VCS_BUILD_DIR) -
a test build in another folder does not see build/'s texture pack or vcs.ini, so never compare its numbers with
build/'s. Results: OUT (default build/quickcheck/<date-time>)/summary.json and a log per check. Exit code 1 when a
check failed. A FAIL is first checked against the check itself (does it fit this fork?) before it is believed.
"""
import argparse
import fnmatch
import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import toolchain as tc  # noqa: E402

# name: (command after `python`, seconds it took here (2026-11-23, RX 6950 XT, hidden runs), what it shows)
CHECKS = {
    "prompts": (["tools/prompts_check.py"], 45, "button prompt glyphs per device, help text, Original unchanged"),
    "mods": (["tools/mods_check.py", "--pack", "build/textures/SLUS-21590"], 80, "mod loader: modloader.ini, priorities, live switch"),
    "menu": (["tools/menu_check.py"], 110, "pause menu: keys, mouse, sliders, map drag"),
    "pcscheme": (["tools/pc_scheme_check.py"], 115, "keyboard PC scheme by situation (6 known failures: PROGRESS Phase 5)"),
    "gamefiles": (["tools/gamefiles_check.py"], 125, "game files from a mod: model entries, a larger one, a GXT"),
    "trainer": (["tools/trainer_check.py"], 140, "every trainer feature in the game's memory (free camera: known failure)"),
    "cutscene": (["tools/cutscene_check.py"], 160, "cutscene framing and sky"),
    "texpack": (["tools/texpack_check.py"], 180, "texture packs: DDS as stored vs decoded"),
    "hud": (["tools/hud_check.py", "--money"], 210, "HUD at the screen edges, in the rain, money digits"),
    "driveraim": (["tools/driver_aim_check.py"], 240, "aiming from the driver's seat, the wheel in a vehicle"),
    "traffic": (["tools/traffic_spawn_check.py"], 305, "car spawner gates at Car distance 200 %"),
    "world": (["tools/world_check.py"], 400, "draw distance, LOD, packets, traffic, cars vanishing"),
    "controls": (["tools/controls_check.py"], 400, "bindings, presets, reset, vibration, volume; every F2 page's screenshot"),
    "memcheck": (["tools/memcheck.py", "--minutes", "8"], 490, "memory over a streaming route must level off"),
    "hdmissions": (["tools/hdworld_missions.py"], 720, "detailed city vs original for every mission-switched building"),
    "fog": (["tools/fog_check.py"], 10, "fog distance / start"),
    "haze": (["tools/haze_check.py"], 240, "mission 94's cutscene at 60 fps, the haze at 16:9 (estimate)"),
    "cruise": (["tools/cruise.py", "run"], 160, "stutter: the car along a fixed route, every frame timed"),
    "bench": (["tools/bench.py"], 240, "game-thread ms at fixed spots and an AI drive (estimate)"),
    "iop": (["tools/build.py", "--iop-tests"], 60, "SPU2 unit tests (estimate)"),
    "regress-quick": (["tools/regress.py", "--quick"], 1000, "the regression suite without missions, halved (estimate)"),
    "regress": (["tools/regress.py"], 2010, "the regression suite: scenes, tour, missions, saves, widescreen, controls"),
}
# the whole suite (--full): once at the end of a batch, or when the user asks
FULL = ["regress", "hud", "prompts", "menu", "pcscheme", "trainer", "texpack", "fog", "cutscene", "world",
        "hdmissions", "mods", "gamefiles", "traffic", "memcheck", "cruise"]
SMOKE = ["hud"]  # a source file no rule names: the game starts, plays, draws its HUD

# (pattern, checks, what the player can try themselves in a minute or two - "" when nothing)
RULES = [
    ("*.md", [], ""),
    ("docs/*", [], ""),
    ("tools/quickcheck.py", [], ""),
    ("src/game/vcs_prompts.cpp", ["prompts"], "the button prompts: a help box on foot and in a car, with the pad and the keyboard"),
    ("src/game/vcs_glyph_art.*", ["prompts"], "the button prompts and the subtitles' letters"),
    ("src/game/vcs_world_detail.cpp", ["world", "traffic"], "the traffic and pedestrians with your World settings, a short drive"),
    ("src/game/vcs_world_hd.cpp", ["hdmissions"], "the detailed city on a drive across town (buildings drawn twice, gaps)"),
    ("src/game/vcs_input.cpp", ["controls", "pcscheme"], "your controls on foot and driving"),
    ("src/game/vcs_aim.cpp", ["controls", "driveraim"], "aiming with the mouse on foot and from a car"),
    ("src/game/vcs_frontend.cpp", ["menu"], "the pause menu with keyboard, mouse and pad"),
    ("src/game/vcs_carcam.cpp", ["controls"], "the camera behind a car, a motorbike and the BMX, turned with the mouse"),
    ("src/game/vcs_hud.cpp", ["hud", "cutscene"],"the HUD, and a mission's title through its fade into the first cutscene"),
    ("src/game/vcs_trainer.cpp", ["trainer"], "the trainer (F5): the feature changed"),
    ("src/game/vcs_texture_pack.*", ["texpack"], "your texture pack in a few places"),
    ("src/game/vcs_stream.*", ["mods", "gamefiles"], "a mod of yours in modloader/"),
    ("src/game/vcs_presets.*", ["controls"], "Display > Presets: each button, then a restart"),
    ("src/game/vcs_script_*", ["haze"], "the mission the fix is for"),
    ("src/game/vcs_volume.cpp", ["controls"], "music and effects volume in Audio"),
    ("src/game/vcs_driveby.cpp", ["driveraim"], "a drive-by"),
    ("src/game/vcs_framerate.cpp", ["hud", "cruise"], "a drive at your frame rate"),
    ("src/game/vcs_*_timing.cpp", ["hud"], "the effect changed, at your frame rate"),
    ("src/game/vcs_sound.cpp", ["iop"], "sound in a cutscene and a drive"),
    ("src/sound/*", ["iop"], "sound in a cutscene and a drive"),
    ("src/mods/*", ["mods", "gamefiles"], "a mod of yours in modloader/, F2 > Mods"),
    ("src/ui/*", ["menu", "controls"], "F2 and F5: the pages changed"),
    ("src/settings/*", ["controls"], "F2: the settings changed, then a restart (they stay)"),
    ("src/render/*", ["hud", "cruise"], "a drive and a cutscene (the picture)"),
    ("src/rhi/*", ["hud", "texpack", "cruise"], "Graphics > Graphics API: each one, a drive"),
    ("src/platform/*", ["controls"], "window, full screen, pad and keyboard"),
    ("src/debug/*", [], ""),
    ("src/main.cpp", ["controls"], "F2: the pages and their order"),
    ("third_party/stb/*", ["prompts"], "the subtitles' letters"),
    ("third_party/PS2Recomp/*gs_hw*", ["hud", "texpack", "cruise"], "a drive and a cutscene (the picture)"),
    ("third_party/PS2Recomp/*", ["regress-quick"], ""),
    ("game/vcs/*", ["regress-quick"], ""),
    ("launcher/*", [], "the launcher: the screen changed (VCSLauncher --self-test is CI's check)"),
    ("CMakeLists.txt", SMOKE, ""),
    ("tools/build.py", SMOKE, ""),
    ("tools/toolchain.py", SMOKE, ""),
]
# a check's own script changed: run that check
SCRIPT_CHECK = {cmd[0]: name for name, (cmd, _, _) in CHECKS.items()}


def changed_files(args):
    if args.files:
        return [f.replace("\\", "/") for f in args.files]
    if args.since is not None:
        cutoff = time.time() - args.since * 3600
        out = []
        for top in ("src", "tools", "third_party/PS2Recomp", "game", "launcher", "docs"):
            for dirpath, dirnames, filenames in os.walk(os.path.join(tc.ROOT, top)):
                dirnames[:] = [d for d in dirnames if d not in ("build", "__pycache__", ".git")]
                for f in filenames:
                    p = os.path.join(dirpath, f)
                    if os.path.getmtime(p) >= cutoff:
                        out.append(os.path.relpath(p, tc.ROOT).replace("\\", "/"))
        for f in ("CMakeLists.txt",):
            if os.path.exists(os.path.join(tc.ROOT, f)) and os.path.getmtime(os.path.join(tc.ROOT, f)) >= cutoff:
                out.append(f)
        return out

    def git(*a):
        r = subprocess.run(["git", "-C", tc.ROOT] + list(a), capture_output=True, text=True)
        if r.returncode:
            raise SystemExit("quickcheck: git %s failed (%s)" % (" ".join(a), (r.stderr.strip().splitlines() or ["?"])[0]))
        return [l for l in r.stdout.splitlines() if l]
    if subprocess.run(["git", "-C", tc.ROOT, "rev-parse", "--git-dir"], capture_output=True).returncode:
        raise SystemExit("quickcheck: %s is not a git checkout: use --files F... or --since HOURS" % tc.ROOT)
    files = git("diff", "--name-only", args.base) if args.base else git("diff", "--name-only", "HEAD")
    files += git("ls-files", "--others", "--exclude-standard")
    return sorted(set(files))


def plan_for(files):
    checks, player, unmatched = [], [], []

    def add(names):
        for n in names:
            if n not in checks:
                checks.append(n)
    for f in files:
        if f in SCRIPT_CHECK:
            add([SCRIPT_CHECK[f]])
            continue
        for pattern, names, try_it in RULES:
            if fnmatch.fnmatch(f, pattern):
                add(names)
                if try_it and try_it not in player:
                    player.append(try_it)
                break
        else:
            if f.startswith(("src/", "third_party/")):
                unmatched.append(f)
                add(SMOKE)
    return checks, player, unmatched


def minutes(seconds):
    return "%d min" % round(seconds / 60.0) if seconds >= 90 else "%d s" % seconds


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--files", nargs="+", help="these files instead of git's changes")
    ap.add_argument("--base", help="git: changes since this ref (default: the uncommitted ones)")
    ap.add_argument("--since", type=float, help="no git: files modified in the last HOURS")
    ap.add_argument("--full", action="store_true", help="the whole suite (once, at the end of a batch)")
    ap.add_argument("--run", action="store_true", help="run the plan (default: only show it)")
    ap.add_argument("--budget", type=float, help="minutes: leave out the checks past it (the costliest last)")
    ap.add_argument("--out", help="default build/quickcheck/<date-time>")
    args = ap.parse_args()

    if args.full:
        files, checks, player, unmatched = [], list(FULL), [], []
    else:
        files = changed_files(args)
        checks, player, unmatched = plan_for(files)
    checks.sort(key=lambda n: CHECKS[n][1])  # the cheapest first: a quick failure before the long ones
    left_out = []
    if args.budget is not None:
        total, kept = 0, []
        for n in checks:
            if total + CHECKS[n][1] <= args.budget * 60:
                kept.append(n)
                total += CHECKS[n][1]
            else:
                left_out.append(n)
        checks = kept

    if not args.full:
        print("%d changed file(s)%s" % (len(files), ": " + ", ".join(files[:12]) + (" ..." if len(files) > 12 else "") if files else ""))
    total = sum(CHECKS[n][1] for n in checks)
    print("\nchecks (%s in all, one after another):" % minutes(total) if checks else "\nno check needed")
    for n in checks:
        print("  %-14s %7s  %s" % (n, minutes(CHECKS[n][1]), CHECKS[n][2]))
    for n in left_out:
        print("  %-14s %7s  left out (past --budget)" % (n, minutes(CHECKS[n][1])))
    if unmatched:
        print("\nno rule names %s: the smoke check (%s) for them - add a rule to tools/quickcheck.py" % (
            ", ".join(unmatched), ", ".join(SMOKE)))
    if player:
        print("\nfor the player (a minute each in the game - ask them rather than spend a run on it):")
        for p in player:
            print("  - " + p)
    tc.event(event="plan", checks=checks, seconds=total, player=player, left_out=left_out)
    if not args.run or not checks:
        if checks and not args.run:
            print("\n(nothing run: add --run)")
        return 0

    out = os.path.abspath(args.out or os.path.join(tc.ROOT, "build", "quickcheck", time.strftime("%Y%m%d-%H%M%S")))
    os.makedirs(out, exist_ok=True)
    results = []
    for n in checks:
        cmd, _, _ = CHECKS[n]
        argv = [sys.executable] + cmd
        if n == "memcheck":
            argv += ["--name", "quickcheck"]
        elif n != "iop":
            argv.append(os.path.join(out, n))
        t = time.time()
        print("[quickcheck] %s ..." % n, flush=True)
        with open(os.path.join(out, n + ".log"), "w", encoding="utf-8") as log:
            rc = subprocess.call(argv, cwd=tc.ROOT, stdout=log, stderr=subprocess.STDOUT)
        results.append({"check": n, "rc": rc, "seconds": round(time.time() - t)})
        print("[quickcheck] %-14s %s  %s" % (n, "ok" if rc == 0 else "FAIL (rc %d)" % rc, minutes(time.time() - t)),
              flush=True)
        tc.event(event="check", name=n, rc=rc, seconds=results[-1]["seconds"])
    with open(os.path.join(out, "summary.json"), "w", encoding="utf-8") as f:
        json.dump({"files": files, "results": results, "player": player, "left_out": left_out}, f, indent=1)
    failed = [r["check"] for r in results if r["rc"]]
    print("\n%d check(s), %d failed%s -> %s" % (len(results), len(failed), ": " + ", ".join(failed) if failed else "", out))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
