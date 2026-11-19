#!/usr/bin/env python3
"""Texture packs with DDS images (Graphics > Texture packs; src/game/vcs_texture_pack.cpp, game-internals.md 7b).

    python tools/texpack_check.py [OUT] [--count N] [--port N] [--real [DIR]]

A pack is made here from the game's own textures, so no pack has to be downloaded: a first hidden run dumps the
textures drawn at a city spot (VCS_TEXTURE_DUMP), then N of them get a DDS replacement twice their size - a checker of
two colours whose mips are the checker box-filtered, so a wrong level or row pitch on the GPU shows - in four kinds:
  bc1        BC1, whole mip chain, opaque                       -> uploaded as stored (block-compressed)
  bc3        BC3, whole mip chain, alpha 0x80, no clear texel   -> uploaded as stored
  bc3holes   BC3, whole mip chain, half its blocks clear        -> decoded (cut-out: the renderer's own mips)
  bc1top     BC1, top level only                                -> decoded (the renderer makes the chain)
Then the spot again with the pack, as stored (the default) and with PS2X_HW_BLOCK_TEXTURES=0 (every DDS decoded, as
before), and once with no pack (VCS_TEXPACK=0). Checks:
  loads      both pack runs replaced textures and failed to read none (load times reported)
  blocks     as stored, images went up block-compressed and the others decoded; with the switch off none did
  memory     the loaded images take less memory as stored than decoded (the pack's cache)
  picture    the two pack runs show the same picture, and differ from the run without the pack (it is drawn at all)
With --real (a PCSX2 pack's folder holding replacements/, default build/textures/SLUS-21590) the same runs with that
pack, prefixed real_, plus
  real_verify  VCS_TEXPACK_VERIFY: every image's classification from its blocks' alpha bounds checked against its
               decoded texels - none accepted with a clear texel
Screenshots and logs go to OUT (default build/texpack_check); OUT/results.json holds the checks. Exit code 1 when one
fails. The streets are emptied (the scripts' density 0) at noon, so the runs see one scene.
"""
import argparse
import json
import os
import shutil
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rig  # noqa: E402
import toolchain as tc  # noqa: E402

ROOT = rig.ROOT
S = rig.Scm
SPOT = (-675.0, 1425.0, 90.0)  # world_check's city spot
KINDS = ("bc1", "bc3", "bc3holes", "bc1top")
COLOURS = [(230, 40, 200), (40, 220, 60), (240, 200, 30), (30, 120, 240), (250, 110, 20), (20, 220, 220)]


def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    if head[:8] != b"\x89PNG\r\n\x1a\n":
        return 0, 0
    return struct.unpack(">II", head[16:24])


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def colour_part(c0, c1, pick):
    """a BC1 colour block: endpoints c0 / c1, texel k (row-major) takes c1 where pick(k) - indices 0 / 1 are the
    endpoints in either mode, so the block is opaque"""
    idx = 0
    for k in range(16):
        if pick(k):
            idx |= 1 << (2 * k)
    return struct.pack("<HHI", rgb565(*c0), rgb565(*c1), idx)


def dds(width, height, kind, colour):
    """a DDS file of `kind` (legacy FourCC DXT1 / DXT5): a checker of 4x4-texel squares of the colour and a darker one,
    its mips the checker box-filtered (2x2, 1x1 squares, then the mean colour) - what the renderer's own chain would be,
    so a block-compressed upload and a decoded one draw the same picture, and a wrong level offset or row pitch on the GPU
    does not"""
    bc1 = kind in ("bc1", "bc1top")
    dark = tuple(c // 2 for c in colour)
    mean = tuple((a + b) // 2 for a, b in zip(colour, dark))
    levels = []
    lw, lh = width, height
    level = 0
    while True:
        bw, bh = (lw + 3) // 4, (lh + 3) // 4
        size = 4 >> level  # the checker's squares in this level's texels (0: one mean colour)
        blocks = []
        for by in range(bh):
            for bx in range(bw):
                if size:
                    part = colour_part(colour, dark, lambda k: ((bx * 4 + k % 4) // size + (by * 4 + k // 4) // size) % 2)
                else:
                    part = colour_part(mean, mean, lambda k: False)
                if bc1:
                    blocks.append(part)
                else:
                    hole = kind == "bc3holes" and (bx + by) % 2 == 1
                    blocks.append(bytes([0 if hole else 0x80, 0, 0, 0, 0, 0, 0, 0]) + part)
        level += 1
        levels.append(b"".join(blocks))
        if kind == "bc1top" or (lw == 1 and lh == 1):
            break
        lw, lh = max(1, lw // 2), max(1, lh // 2)
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000 | (0x20000 if len(levels) > 1 else 0)
    caps = 0x1000 | ((0x8 | 0x400000) if len(levels) > 1 else 0)
    head = struct.pack("<4sIIIIIII", b"DDS ", 124, flags, height, width, len(levels[0]), 0, len(levels))
    head += b"\0" * 44
    head += struct.pack("<II4sIIIII", 32, 0x4, b"DXT1" if bc1 else b"DXT5", 0, 0, 0, 0, 0)
    head += struct.pack("<IIIII", caps, 0, 0, 0, 0)
    assert len(head) == 128
    return head + b"".join(levels)


def status(g):
    return dict(kv.split("=", 1) for kv in g.cmd("texpack").split() if "=" in kv)


def diff_fraction(a, b):
    """share of sampled pixels that differ by more than 25 in a channel"""
    wa, ha, ra = tc.read_png(a)
    wb, hb, rb = tc.read_png(b)
    if (wa, ha) != (wb, hb):
        return 1.0
    bpp = len(ra[0]) // wa
    n = changed = 0
    for y in range(0, ha, 4):
        la, lb = ra[y], rb[y]
        for x in range(0, wa * bpp, 4 * bpp):
            n += 1
            if max(abs(la[x + c] - lb[x + c]) for c in range(3)) > 25:
                changed += 1
    return changed / max(n, 1)


def session(name, env, out, port, seconds=20.0):
    """the spot at noon, streets emptied: (texpack status, screenshot path, log path)"""
    g = rig.Game.start(save=rig.default_save(), name="texpack_" + name, env=env, port=port)
    try:
        g.wait_ingame()
        g.prime()
        g.invincible()
        g.scm(S.op(0x0125, S.f(0.0)) + S.op(0x0264, S.f(0.0)))  # SET_CAR / PED_DENSITY_MULTIPLIER 0
        g.set_time(12)
        g.weather(0)
        g.teleport(*SPOT[:2], -100.0, heading=SPOT[2])
        time.sleep(seconds)
        g.set_time(12)
        g.wait_frames(30)
        shot = os.path.join(out, name + ".png")
        g.cmd("screenshot " + shot)
        time.sleep(1.0)
        st = status(g)
        print("[texpack_check] %-8s %s" % (name, " ".join("%s=%s" % kv for kv in st.items())), flush=True)
        return st, shot, g.log_path
    finally:
        try:
            g.quit()
        except Exception:
            pass


def compare(prefix, tex, out, port, ok, verify, fixtures):
    """the spot with the pack as stored (the default) and decoded (PS2X_HW_BLOCK_TEXTURES=0), and the fixtures' pack also
    with none: the checks, named with `prefix`"""
    n = lambda st, k: int(st.get(k, "0"))  # noqa: E731
    env = {"VCS_TEXTURE_DIR": tex}
    if verify:
        env["VCS_TEXPACK_VERIFY"] = "1"
    stored, shot_stored, log_stored = session(prefix + "stored", env, out, port)
    decoded, shot_decoded, _ = session(prefix + "decoded", {"VCS_TEXTURE_DIR": tex, "PS2X_HW_BLOCK_TEXTURES": "0"}, out,
                                       port)
    ok(prefix + "loads", n(stored, "replaced") > 0 and n(decoded, "replaced") > 0 and n(stored, "failed") == 0 and
       n(decoded, "failed") == 0,
       "replaced %d / %d, loaded %d / %d, failed %d / %d (as stored / decoded); load %s / %s ms an image" % (
           n(stored, "replaced"), n(decoded, "replaced"), n(stored, "loaded"), n(decoded, "loaded"),
           n(stored, "failed"), n(decoded, "failed"), stored.get("load_ms_avg"), decoded.get("load_ms_avg")))
    # the fixtures: half the kinds must stay decoded; a real pack may have no image with clear texels
    blocks_ok = 0 < n(stored, "blocks") and (n(stored, "blocks") < n(stored, "loaded") if fixtures else
                                             n(stored, "blocks") <= n(stored, "loaded"))
    ok(prefix + "blocks", blocks_ok and n(decoded, "blocks") == 0,
       "block-compressed as stored %d of %d loaded; with the switch off %d" % (
           n(stored, "blocks"), n(stored, "loaded"), n(decoded, "blocks")))
    per_stored = n(stored, "cache_kb") / max(1, n(stored, "loaded"))
    per_decoded = n(decoded, "cache_kb") / max(1, n(decoded, "loaded"))
    ok(prefix + "memory", per_stored < per_decoded,
       "pack cache %d KB for %d images as stored, %d KB for %d decoded (%.0f / %.0f KB an image)" % (
           n(stored, "cache_kb"), n(stored, "loaded"), n(decoded, "cache_kb"), n(decoded, "loaded"), per_stored,
           per_decoded))
    same = diff_fraction(shot_stored, shot_decoded)
    if fixtures:
        _, shot_none, _ = session(prefix + "none", {"VCS_TEXTURE_DIR": tex, "VCS_TEXPACK": "0"}, out, port)
        drawn = diff_fraction(shot_stored, shot_none)
        ok(prefix + "picture", same < 0.02 and drawn > 0.05,
           "as stored vs decoded: %.2f%% of the picture differs; vs no pack: %.1f%%" % (same * 100, drawn * 100))
    else:
        ok(prefix + "picture", same < 0.02, "as stored vs decoded: %.2f%% of the picture differs" % (same * 100))
    if verify:
        with open(log_stored, encoding="utf-8", errors="replace") as f:
            lines = [l for l in f if "[texpack] verify " in l]
        bad = [l.strip() for l in lines if "MISMATCH" in l]
        stricter = sum(1 for l in lines if "stricter" in l)
        ok(prefix + "verify", lines and not bad,
           "%d images checked against their texels: %d accepted with clear texels, %d kept decoded that had none%s" % (
               len(lines), len(bad), stricter, (" - " + bad[0]) if bad else ""))
    return stored, decoded


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?", default=os.path.join(ROOT, "build", "texpack_check"))
    ap.add_argument("--count", type=int, default=160, help="dumped textures given a replacement")
    ap.add_argument("--port", type=int, default=47721)
    ap.add_argument("--real", nargs="?", const=os.path.join(ROOT, "build", "textures", "SLUS-21590"), default=None,
                    help="also a real pack's folder (holding replacements/; default build/textures/SLUS-21590)")
    args = ap.parse_args()
    out = os.path.abspath(args.out)
    tex = os.path.join(out, "textures")
    if os.path.isdir(tex):
        shutil.rmtree(tex)
    os.makedirs(tex)
    results = {}

    def ok(name, cond, detail=""):
        results[name] = {"ok": bool(cond), "detail": str(detail)}
        print("[texpack_check] %-8s %s  %s" % (name, "ok" if cond else "FAILED", detail), flush=True)

    # 1. the textures drawn there, dumped
    session("dump", {"VCS_TEXTURE_DIR": tex, "VCS_TEXTURE_DUMP": "1"}, out, args.port)
    dumps = os.path.join(tex, "dumps")
    names = sorted(n for n in os.listdir(dumps) if n.endswith(".png")) if os.path.isdir(dumps) else []
    sized = [(n,) + png_size(os.path.join(dumps, n)) for n in names]
    sized = [s for s in sized if s[1] >= 8 and s[2] >= 8]
    sized.sort(key=lambda s: -(s[1] * s[2]))  # the larger ones: the ones that cover the picture
    chosen = sized[:args.count]
    if not chosen:
        ok("dump", False, "no textures dumped to %s" % dumps)
        return 1
    # 2. the pack
    repl = os.path.join(tex, "replacements", "check")
    os.makedirs(repl)
    made = {k: 0 for k in KINDS}
    for i, (name, w, h) in enumerate(chosen):
        kind = KINDS[i % len(KINDS)]
        width, height = max(4, (w * 2 + 3) // 4 * 4), max(4, (h * 2 + 3) // 4 * 4)
        with open(os.path.join(repl, name[:-4] + ".dds"), "wb") as f:
            f.write(dds(width, height, kind, COLOURS[i % len(COLOURS)]))
        made[kind] += 1
    shutil.rmtree(dumps)  # (no dumping in the pack runs)
    print("[texpack_check] pack: %s" % made, flush=True)

    # 3. the runs
    stored, decoded = compare("", tex, out, args.port, ok, verify=False, fixtures=True)
    real = {}
    if args.real and os.path.isdir(os.path.join(args.real, "replacements")):
        # 4. a real pack (PCSX2's format), if there is one: every image the bounds accept checked against its texels
        real["stored"], real["decoded"] = compare("real_", os.path.abspath(args.real), out, args.port, ok, verify=True,
                                                  fixtures=False)
    elif args.real:
        print("[texpack_check] no real pack at %s (its replacements/ folder): skipped" % args.real, flush=True)
    with open(os.path.join(out, "results.json"), "w") as f:
        json.dump({"checks": results, "pack": made, "stored": stored, "decoded": decoded, "real": real}, f, indent=1)
    failed = [k for k, v in results.items() if not v["ok"]]
    print("[texpack_check] %s" % ("all ok" if not failed else "FAILED: " + ", ".join(failed)), flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
