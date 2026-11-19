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
| 3 | Fog distance / fog start as settings (the game's RSL fog-range setter FUN_002F6008 scaled) | willzmkv 0d63f27 | S | re-do (vcs_fog.cpp) | done 2026-11-19: their file as it is (game-internals 6r), tools/fog_check.py |
| 4 | Exposure on the shown picture | willzmkv 02a6d4c, b5edbaf | S | re-do in the presenter | open |
| 5 | GS: additive pixels that add nothing skipped (the hovercraft's glow) | PerikiyoXD, PR #178 | S | merge; conflict in gs_hw.cpp | open |
| 6 | D3D11: state reaches the driver only when it changes (we set the whole pipeline every draw) | willzmkv 4fc7828 | S-M | re-do in rhi_d3d11.cpp | open |
| 7 | Native draws joined: draws of one mesh with the same state as one (half the draw calls) | willzmkv 0bc5067 | M | re-do in gs_hw.cpp SubmitNative | open |
| 8 | The game from a CHD image | KenRylo-One, PR #201 | S | merge; docs conflicts | open |
| 9 | Button prompts with a translated GXT (controls by key, not English words) | SkipperSkipTR, PR #181 | S | merge; only matters with a translated text | open |

## B. Texture packs and mods (each builds on the one before)

| # | What | From | Size | How | Status |
|---|---|---|---|---|---|
| 10 | Pack DDS uploaded compressed (BC1-BC7, ~1/4 of the memory, no decode) - except textures that need our alpha-coverage mips (the palms: the pack's stored mips lose them, measured) | willzmkv 37c9194, 5f1b1c2 | M-L | re-do: RHI BC formats in D3D11 / Vulkan / Metal, a hybrid in vcs_texture_pack.cpp | open |
| 11 | Several packs at once with priorities, switched live in F2 (mod folders, Mod Loader's modloader.ini) | willzmkv a8a8587, cc86fcc, 9091a8e | M | re-do on our texpack | open |
| 12 | Dump as DDS (for pack makers) | willzmkv 5f1b1c2 | S | re-do | open |
| 13 | Game files from mods: GTA3PS2.IMG entries, GXT, MAIN.SCM, larger than the disc's | willzmkv 96edbbe, eed27a2, 69d3d21 | L | re-do | open |
| 14 | Streaming memory past the PS2's 32 MB (5.4 MB heap -> up to 30 MB) | willzmkv 58a854f | L | re-do; touches guest memory size | open |
| 15 | PC Vice City car mods (.dff + .txd converted to VCS's models) | willzmkv T9 (afb87df, 6e5f8d9, d177a70...) | XL | re-do with their tools (dff2mdl, txd2xtx) | open |
| 16 | PC weapon mods (VC and SA) | willzmkv T11, spartaque12 (Immersive Weapons) | L | re-do | open |

## C. Controls

| # | What | From | Size | How | Status |
|---|---|---|---|---|---|
| 17 | DualSense: lightbar, adaptive triggers (a feel per gun), a gun's kick, voices on its speaker | TomRous PR #96, willzmkv 946e1a9, 6d476bf, 40a9d4b | M | merge #96 (conflicts in vcs_input.cpp, platform_sdl3.cpp) + re-do the rest | open |
| 18 | Gamepad: L1 / R1 look sideways while driving, aim assist, D-pad look behind, "GTA V buttons" | willzmkv 3a13a3b, 6d476bf | M | re-do | open |
| 19 | Keyboard: PC control scheme as in VC / SA on PC, keys shared per situation | SkipperSkipTR PR #78 | M | merge; conflicts with our vcs_input.cpp | open |
| 20 | Mouse free aim while driving; the wheel cycles the car's weapons | ChrisM-Pek (European fork) 741d6a6, a90845f | M | re-do (their base is the PAL disc) | open |

## D. The world further

| # | What | From | Size | How | Status |
|---|---|---|---|---|---|
| 21 | Detailed city further (the detailed sectors of neighbour cells instead of the low-detail city) | willzmkv "HD world" (~40 commits) | XL | re-do; theirs went through crashes before a 3 h soak passed | open |
| 22 | Far street-lamp / traffic-light coronas to the detailed distance | willzmkv 635e6bc ... 36ee818 | M | re-do next to our distant lights (7c) | open |
| 23 | Distant traffic and crowds (proxies on the real models), crowd bands | willzmkv T2, 8c7e1be, b08463c, cecdd55... | XL | re-do; experimental there | open |
| 24 | Parked cars at the population distance; police spawn ring kept while wanted | willzmkv 0c41004, 57d661f | S-M | re-do in vcs_world_detail.cpp | open |

## E. Presentation

| # | What | From | Size | How | Status |
|---|---|---|---|---|---|
| 25 | Subtitles in Oswald (OFL) with size, shadow, background and colour options | willzmkv 6e14daa, d85fc1e, 27a4a9c | M | re-do | open |
| 26 | Cutscenes: the sky no longer dark during the fade-in; the PS2's framing as an option | willzmkv ab1a765, 477cfd0 | S-M | check ours, re-do | open |
| 27 | Sniper scope mask bars keep their corners in widescreen | willzmkv ab6d89c | S | check ours, re-do | open |

## F. spartaque12's mods (VCS-mods repository; their fork is 160 commits behind)

| # | What | Size | Status |
|---|---|---|---|
| 28 | First person on foot and in vehicles, viewmodel while aiming, reloads in view | L | open |
| 29 | Death ragdolls (Jolt Physics) | XL | open |
| 30 | Vegetation wind sway | M | open |
| 31 | Shadow maps / ambient occlusion (RenderHook-style) | XL | open |
| 32 | DLL (.asi) mod API | L | open |
| 33 | Head shots (PedSkills) | M | open |

## G. Menus and launcher

| # | What | From | Size | Status |
|---|---|---|---|---|
| 34 | A native main menu / game menu (VC Definitive Edition's layout) | rofhima13 PR #26, willzmkv T12 | L | open |
| 35 | Launcher: graphics presets from the PC's hardware, shader warm-up, controls on a picture of the pad | willzmkv 283bb56, 9c01e8f | L | open |
| 36 | Direct3D 12 backend | willzmkv ae6c99e, 28ef950, d9f73ed | XL | open (little gain on our NVIDIA: D3D11 is past 100 fps) |

## Not for us

PAL disc (ChrisM-Pek), browser port (floriandotorg), Android (batinium, KenRylo-One), Intel Macs (knox29 PR #190),
macOS path fixes (tanvir-glide PR #186, only with a space in the build path), localized disc builds and launcher
settings (MaksVolkov7863), CI files (mahoganyprogrammer). Already ours: fire on AMD (upstream since 2026-11-04), draw
distance through the game's LOD scale, traffic / pedestrian density, mission timers above 30 fps, the pause menu with
the mouse, HD HUD through the pack, Project2DFX's distant lights.
