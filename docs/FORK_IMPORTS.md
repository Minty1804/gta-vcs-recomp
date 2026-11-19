# What other forks have that we could bring in (2026-11-18)

Survey of every fork of gothamor/gta-vcs-recomp with work of its own (40 forks, every branch compared with upstream
`main`). Our branch is upstream `main` + our commits, so an upstream PR that is 0 behind merges, a fork far behind is
re-done on our code. "Size" is the work here: S hours, M a day or so, L several days, XL a project of its own.

The items are in the order we go through them, one at a time. Status: open / done / skipped (with why).

## A. Small: merges of open upstream PRs and short re-implementations

| # | What | From | Size | How | Status |
|---|---|---|---|---|---|
| 1 | GPS route on the radar and the pause map (mission target and the map's own target) | olaftnt, upstream PR #183 | S | merge; conflict only in main.cpp | done 2026-11-18 (game-internals 12e) |
| 2 | Quick save key: the game's Save menu outside missions (vehicle height, wanted level kept) | olaftnt, PR #184 | S | merge; main.cpp; its F12 is free here (our F12 dump is behind an env var) | done 2026-11-18, three load fixes of ours (game-internals 12f) |
| 3 | Fog distance / fog start as settings (the game's RSL fog-range setter FUN_002F6008 scaled) | willzmkv 0d63f27 | S | re-do (vcs_fog.cpp) | done 2026-11-19: their file as it is (game-internals 6r), tools/fog_check.py; ours on top: above 100 % a fog curve |
| 4 | Exposure on the shown picture | willzmkv 02a6d4c, b5edbaf | S | re-do in the presenter | done 2026-11-19, re-done before the HUD with a highlight roll-off (game-internals 6s) |
| 5 | GS: additive pixels that add nothing skipped (the hovercraft's glow) | PerikiyoXD, PR #178 | S | merge; conflict in gs_hw.cpp | done 2026-11-19: merged by hand, two guards of ours (packs, HUD layer); little gain here (game-internals 6j) |
| 6 | D3D11: state reaches the driver only when it changes (we set the whole pipeline every draw) | willzmkv 4fc7828 | S-M | re-do in rhi_d3d11.cpp | done 2026-11-19: re-done with viewport / scissor / blend constant cached too; city view 1.2 -> 0.7 ms on the main thread (PERFORMANCE.md 34) |
| 7 | Native draws joined: draws of one mesh with the same state as one (half the draw calls) | willzmkv 0bc5067 | M | re-do in gs_hw.cpp SubmitNative | done 2026-11-19: re-done. Groups that take two passes (cut-out split, AFAIL) or read a render target are drawn as their parts, strip parity kept, the mesh cache bounded by bytes. Draws 2,055 -> 1,060 in the city; pictures identical on vs off (PERFORMANCE.md 35) |

## B. Texture packs and mods (each builds on the one before)

| # | What | From | Size | How | Status |
|---|---|---|---|---|---|
| 10 | Pack DDS uploaded compressed (BC1-BC7, ~1/4 of the memory, no decode) - except textures that need our alpha-coverage mips (the palms: the pack's stored mips lose them, measured) | willzmkv 37c9194, 5f1b1c2 | M-L | re-do: RHI BC formats in D3D11 / Vulkan / Metal, a hybrid in vcs_texture_pack.cpp | done 2026-11-19: block-compressed images with no clear texel go up as stored (88 % of a real pack's images in a city run; cache 238 vs 713 KB an image), cut-outs decoded as before. texpack_check (fixtures + --real, every classification verified) passes on D3D11 and on Vulkan (Windows); Metal written to the same contract, not built or run here |
| 14 | Streaming memory past the PS2's 32 MB (5.4 MB heap -> up to 30 MB) | willzmkv 58a854f | L | re-do; touches guest memory size | evaluated 2026-11-19, not started: their reason was VC car mods (4-5x VCS's cars), which this fork has no loader for; here it would only give more car models in traffic. It needs EE RAM past 32 MB with the game still seeing 32 MB, and 185 literal 0x01FFFFFF / 0x02000000 masks in 48 files of src/ and the runtime (plus the generated code's reads) checked one by one, a full rebuild, the regression rig and a soak. Worth doing together with a car-mod loader, on its own branch |

## C. Controls

| # | What | From | Size | How | Status |
|---|---|---|---|---|---|
| 19 | Keyboard: PC control scheme as in VC / SA on PC, keys shared per situation | SkipperSkipTR PR #78 | M | merge; conflicts with our vcs_input.cpp | done 2026-11-19: merged, our Reload key in both schemes (on foot only in the PC one: R is the radio in a car); tools/pc_scheme_check.py |
| 20 | Mouse free aim while driving; the wheel cycles the car's weapons | ChrisM-Pek (European fork) 741d6a6, a90845f | M | re-do (their base is the PAL disc) | done 2026-11-19: their aim and wheel code merged on ours (USA addresses), the aim key fitted to both schemes (PC: the right mouse button, Space stays the handbrake), Mouse wheel in a vehicle: Weapons (default) / Radio; their 25 fps cap is PAL-only and left out. `tools/driver_aim_check.py` |

## D. The world further

| # | What | From | Size | How | Status |
|---|---|---|---|---|---|
| 21 | Detailed city further (the detailed sectors of neighbour cells instead of the low-detail city) | willzmkv "HD world" (~40 commits) | XL | re-do; theirs went through crashes before a 3 h soak passed | evaluated 2026-11-19, not started: ~30 commits, +17,000 lines in their fork, a 3 h soak (soak.py) and memcheck.py are the bar before it may be merged. A project of its own, on a branch |
| 22 | Far street-lamp / traffic-light coronas to the detailed distance | willzmkv 635e6bc ... 36ee818 | M | re-do next to our distant lights (7c) | evaluated 2026-11-19, waits for 21: their far coronas are drawn from the HD world's sectors and object pool (635e6bc, 36ee818); without 21 our distant lights (7c) already cover the low-detail city |
| 23 | Distant traffic and crowds (proxies on the real models), crowd bands | willzmkv T2, 8c7e1be, b08463c, cecdd55... | XL | re-do; experimental there | evaluated 2026-11-19, waits for 21: the proxies stand on the detailed city's reach; experimental in their fork too |
| 24 | Parked cars at the population distance; police spawn ring kept while wanted | willzmkv 0c41004, 57d661f | S-M | re-do in vcs_world_detail.cpp | done 2026-11-19: the generators' 160 / 170 m follow Car distance (patched immediates, game-internals 12d); the police ring needs nothing here (Car distance never moved it). Checked (tools/world_check.py cargen, at a city spot): 9 generators in range at 100 %, 19 at 200 %, 9 again back at 100 %; the whole check passes. Not seen here: an extra parked car. A generator entering the range spawns only when a flag on the player ped allows (game-internals 12d): 0 throughout the player's save tested (on foot, driving, any hour), so no generator rolled at all, at 100 % either. What sets it is not known yet; the fix is hoped to show where it is 1. |

## E. Presentation

| # | What | From | Size | How | Status |
|---|---|---|---|---|---|
| 26 | Cutscenes: the sky no longer dark during the fade-in; the PS2's framing as an option | willzmkv ab1a765, 477cfd0 | S-M | check ours, re-do | done 2026-11-19: both taken (the sky fix in vcs_hud.cpp, the framing in vcs_display.cpp, PS2 by default), the side planes left at the 4:3 width in PS2 framing. Checked: tools/cutscene_check.py passes (the sky at the 16th frame of the fade; VCS_HUD_SKY=0 fails at 168), regress scopemask, hud_check and pc_scheme_check pass, mission 50's .CUT framed as the 4:3 picture. |
| 27 | Sniper scope mask bars keep their corners in widescreen | willzmkv ab6d89c | S | check ours, re-do | done 2026-11-19 with item 26 (its fix only matters with 26's sky fix); regress scopemask passes |

## Later (set aside 2026-11-19, the user's choice: maybe later)

Taken out of the working order; each keeps its number and can come back to it.

| # | What | From | Size |
|---|---|---|---|
| 8 | The game from a CHD image | KenRylo-One, PR #201 | S |
| 9 | Button prompts with a translated GXT (controls by key, not English words) | SkipperSkipTR, PR #181 | S |
| 11 | Several packs at once with priorities, switched live in F2 (mod folders, Mod Loader's modloader.ini) | willzmkv a8a8587, cc86fcc, 9091a8e | M |
| 12 | Dump as DDS (for pack makers) | willzmkv 5f1b1c2 | S |
| 13 | Game files from mods: GTA3PS2.IMG entries, GXT, MAIN.SCM, larger than the disc's | willzmkv 96edbbe, eed27a2, 69d3d21 | L |
| 15 | PC Vice City car mods (.dff + .txd converted to VCS's models) | willzmkv T9 (afb87df, 6e5f8d9, d177a70...) | XL |
| 16 | PC weapon mods (VC and SA) | willzmkv T11, spartaque12 (Immersive Weapons) | L |
| 17 | DualSense: lightbar, adaptive triggers (a feel per gun), a gun's kick, voices on its speaker | TomRous PR #96, willzmkv 946e1a9, 6d476bf, 40a9d4b | M |
| 18 | Gamepad: L1 / R1 look sideways while driving, aim assist, D-pad look behind, "GTA V buttons" | willzmkv 3a13a3b, 6d476bf | M |
| 25 | Subtitles in Oswald (OFL) with size, shadow, background and colour options | willzmkv 6e14daa, d85fc1e, 27a4a9c | M |
| 28 | First person on foot and in vehicles, viewmodel while aiming, reloads in view | spartaque12 (VCS-mods) | L |
| 29 | Death ragdolls (Jolt Physics) | spartaque12 (VCS-mods) | XL |
| 30 | Vegetation wind sway | spartaque12 (VCS-mods) | M |
| 31 | Shadow maps / ambient occlusion (RenderHook-style) | spartaque12 (VCS-mods) | XL |
| 32 | DLL (.asi) mod API | spartaque12 (VCS-mods) | L |
| 33 | Head shots (PedSkills) | spartaque12 (VCS-mods) | M |
| 34 | A native main menu / game menu (VC Definitive Edition's layout) | rofhima13 PR #26, willzmkv T12 | L |
| 35 | Launcher: graphics presets from the PC's hardware, shader warm-up, controls on a picture of the pad | willzmkv 283bb56, 9c01e8f | L |
| 36 | Direct3D 12 backend | willzmkv ae6c99e, 28ef950, d9f73ed | XL |

## Not for us

PAL disc (ChrisM-Pek), browser port (floriandotorg), Android (batinium, KenRylo-One), Intel Macs (knox29 PR #190),
macOS path fixes (tanvir-glide PR #186, only with a space in the build path), localized disc builds and launcher
settings (MaksVolkov7863), CI files (mahoganyprogrammer). Already ours: fire on AMD (upstream since 2026-11-04), draw
distance through the game's LOD scale, traffic / pedestrian density, mission timers above 30 fps, the pause menu with
the mouse, HD HUD through the pack, Project2DFX's distant lights.
