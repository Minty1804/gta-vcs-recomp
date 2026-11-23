# What other forks have, and what came from them

Survey of every fork of gothamor/gta-vcs-recomp with work of its own (40 forks, every branch compared with upstream
`main`, 2026-11-18), and of batinium's Liberty City Stories port on the same runtime (2026-11-23). willzmkv's fork looked at again on
2026-11-23: everything after b333b16 (the head of the first survey) up to their `main` 3e71e73 of 2026-11-20 (rows 41-45). Our branch is
upstream `main` + our commits, so an upstream PR that is 0 behind merges; a fork far behind is re-done on our code.
Every item taken keeps its author on the commit and a line in the README's Credits. Items keep their numbers (commits
and docs refer to them). "Size": S hours, M a day or so, L several days, XL a project of its own.

## Done

| # | Area | What | From | How it was done, where it is written down |
|---|---|---|---|---|
| 1 | Map and saving | GPS route on the radar and the pause map (mission target and the map's own target) | olaftnt, upstream PR #183 | 2026-11-18 (game-internals 12e) |
| 2 | Map and saving | Quick save key: the game's Save menu outside missions (vehicle height, wanted level kept) | olaftnt, PR #184 | 2026-11-18, three load fixes of ours (game-internals 12f) |
| 3 | Graphics | Fog distance / fog start as settings (the game's RSL fog-range setter FUN_002F6008 scaled) | willzmkv 0d63f27 | 2026-11-19: their file as it is (game-internals 6r), tools/fog_check.py; ours on top: above 100 % a fog curve |
| 4 | Graphics | Exposure on the shown picture | willzmkv 02a6d4c, b5edbaf | 2026-11-19, re-done before the HUD with a highlight roll-off (game-internals 6s) |
| 5 | Renderer | GS: additive pixels that add nothing skipped (the hovercraft's glow) | PerikiyoXD, PR #178 | 2026-11-19: merged by hand, two guards of ours (packs, HUD layer); little gain here (game-internals 6j) |
| 6 | Renderer | D3D11: state reaches the driver only when it changes (we set the whole pipeline every draw) | willzmkv 4fc7828 | 2026-11-19: re-done with viewport / scissor / blend constant cached too; city view 1.2 -> 0.7 ms on the main thread (PERFORMANCE.md 34) |
| 7 | Renderer | Native draws joined: draws of one mesh with the same state as one (half the draw calls) | willzmkv 0bc5067 | 2026-11-19: re-done. Groups that take two passes (cut-out split, AFAIL) or read a render target are drawn as their parts, strip parity kept, the mesh cache bounded by bytes. Draws 2,055 -> 1,060 in the city; pictures identical on vs off (PERFORMANCE.md 35) |
| 10 | Texture packs | Pack DDS uploaded compressed (BC1-BC7, ~1/4 of the memory, no decode) - except textures that need our alpha-coverage mips (the palms: the pack's stored mips lose them, measured) | willzmkv 37c9194, 5f1b1c2 | 2026-11-19: block-compressed images with no clear texel go up as stored (88 % of a real pack's images in a city run; cache 238 vs 713 KB an image), cut-outs decoded as before. texpack_check (fixtures + --real, every classification verified) passes on D3D11 and on Vulkan (Windows); Metal written to the same contract, not built or run here |
| 14 | Memory | Streaming memory past the PS2's 32 MB (5.4 MB heap -> up to 30 MB) | willzmkv 58a854f | 2026-11-20 on branch hd-world: the 128 MB of EE RAM came with row 21; their streaming heap and cap hook brought over (src/game/vcs_memory.cpp, game-internals.md 9d), World > Memory > **Streaming memory**, PS2 by default (theirs: 30 MB). No car-mod loader here yet: it gives more car models in traffic and room for row 13 / 15 |
| 19 | Controls | Keyboard: PC control scheme as in VC / SA on PC, keys shared per situation | SkipperSkipTR PR #78 | 2026-11-19: merged, our Reload key in both schemes (on foot only in the PC one: R is the radio in a car). Reopened the same day (the user: still set by gamepad button, nothing split by situation, no aircraft): the gamepad by action too, an Aircraft section (measured in a Maverick and the Biplane), the shared rows split per situation, the PS2 layout on a page of its own (DECISIONS.md 2026-11-19); tools/pc_scheme_check.py (29 checks). Then the PS2 layout removed, its Crouch row renamed Manual aim (L3: VCS has no crouch) and Action renamed Pick up weapon (DECISIONS.md 2026-11-19, second entry) |
| 20 | Combat | Mouse free aim while driving; the wheel cycles the car's weapons | ChrisM-Pek (European fork) 741d6a6, a90845f | 2026-11-19: their aim and wheel code merged on ours (USA addresses), the aim key fitted to both schemes (PC: the right mouse button, Space stays the handbrake), Mouse wheel in a vehicle: Weapons (default) / Radio; their 25 fps cap is PAL-only and left out. `tools/driver_aim_check.py` |
| 21 | World | Detailed city further (the detailed sectors of neighbour cells instead of the low-detail city) | willzmkv "HD world" (~40 commits) | 2026-11-20 (branch hd-world, merged): their vcs_world_hd.cpp / vcs_memory.cpp (packet heap part) and the 128 MB host EE RAM brought over (game-internals.md 6t, DECISIONS.md 2026-11-19 "Detailed city further"); our radius-3 hang (bad frees into the packet heap) fixed; the bar passed at radius 3: 3 h soak, memcheck, cruise, overlap scan, mission buildings |
| 24 | World | Parked cars at the population distance; police spawn ring kept while wanted | willzmkv 0c41004, 57d661f | 2026-11-19: the generators' 160 / 170 m follow Car distance (patched immediates, game-internals 12d); the police ring needs nothing here (Car distance never moved it). Checked (tools/world_check.py cargen, at a city spot): 9 generators in range at 100 %, 19 at 200 %, 9 again back at 100 %; the whole check passes. Not seen here: an extra parked car. A generator entering the range spawns only when a flag on the player ped allows (game-internals 12d): 0 throughout the player's save tested (on foot, driving, any hour), so no generator rolled at all, at 100 % either. What sets it is not known yet; the fix is hoped to show where it is 1. |
| 26 | Cutscenes | Cutscenes: the sky no longer dark during the fade-in; the PS2's framing as an option | willzmkv ab1a765, 477cfd0 | 2026-11-19: both taken (the sky fix in vcs_hud.cpp, the framing in vcs_display.cpp, PS2 by default), the side planes left at the 4:3 width in PS2 framing. Checked: tools/cutscene_check.py passes (the sky at the 16th frame of the fade; VCS_HUD_SKY=0 fails at 168), regress scopemask, hud_check and pc_scheme_check pass, mission 50's .CUT framed as the 4:3 picture. Fixed 2026-11-23: the sky fix took the anchor of any HUD vertex on a sky strip's vertex, which threw a money digit to its 4:3 place for a frame (the user's HUD glitch); now only full-width HUD primitives (game-internals 6f, `hud_check.py --money`). |
| 27 | Cutscenes | Sniper scope mask bars keep their corners in widescreen | willzmkv ab6d89c | 2026-11-19 with item 26 (its fix only matters with 26's sky fix); regress scopemask passes |
| 37 | Graphics | Sharp trails: the trails / colour pass at full resolution, without its blur (not a fork: the user's request, 2026-11-20) | aap, sharptrails for GTA III / VC on PC (MIT) | 2026-11-20: Graphics > Image quality, off by default (game-internals 6g) |
| 38 | Runtime | Runtime speed: VIF1 UNPACK typed / SSE paths, the GS stage's ring in batches, the GS state lock across 32 records, ClutHash per chunk, block-list pool, flat maps; and a GIF bounds crash fix | batinium 1ec8297 (v0.3.0) | 2026-11-23 (PERFORMANCE.md 37) |

## Open or skipped

| # | What | From | Size | Status |
|---|---|---|---|---|
| 22 | Far street-lamp / traffic-light coronas to the detailed distance | willzmkv 635e6bc ... 36ee818 | M | open: 21 is done (2026-11-20), so it can be taken now; not started. Evaluated 2026-11-19: their far coronas are drawn from the HD world's sectors and object pool (635e6bc, 36ee818); without 21 our distant lights (7c) already cover the low-detail city |
| 23 | Distant traffic and crowds (proxies on the real models), crowd bands | willzmkv T2, 8c7e1be, b08463c, cecdd55... | XL | open: 21 is done (2026-11-20), so it can be taken now; not started. Evaluated 2026-11-19: the proxies stand on the detailed city's reach; experimental in their fork too |
| 39 | VIF1 chain walked on its own thread (`PS2X_CHAIN_ASYNC`) | batinium 1ec8297 | L | skipped: their own walk is synchronous whenever RSL observers are on (ours always are), and it needs game-specific waits where the model heap frees / moves memory (their lcs_dma_sync.cpp) |
| 40 | Chain images: texture pixel REFs interned by the chain walker, passed to the GS by reference | batinium 1ec8297 | L | open: needs the write watch and our RSL recordings' bounds (GitHub issue 115) re-checked; see PERFORMANCE.md 37 for whether VCS spends the time it saves |
| 41 | Detailed city: a sector with records in two buckets (opaque + translucent glass) kept whole - Downtown's construction building (-975, 475) lost its blue glass at radius 3 | willzmkv 2d22363 | S | open (2026-11-23): our `sameSphere()` (vcs_world_hd.cpp) is their logic before the fix - it takes the same id's second bucket for a duplicate. Our file differs from theirs (the radius-3 hang fix), so it is re-done, not cherry-picked: the id in `Sphere`, `isHidden()` on the 0x8000 flag, the probe's skip reasons; `VCS_HD_SAME_ID_DUPES=1` back. Bar: hd_overlap_scan (with their change: radius 0 set before the teleport, falling players skipped), hdworld_missions, cruise |
| 42 | Traffic at a raised Car distance: the car spawner's two gates relaxed (the extra 20 m clear check while you stand still, "driving away" past 160 m) - standing in Little Havana it threw 98-99 % of the cars it built away | willzmkv bca7997 | M | open: their hook is in vcs_distance.cpp, which we don't have - re-done in vcs_world_detail.cpp. First measure whether ours has the symptom (cars built vs kept at Car distance 200 %). The hook reads the stack after `CWorld::FindObjectsInRange` (0x195380, a leaf) returns, guarded by `pc`: check it against AGENTS rule 2 before taking it. Pop / density at the game's own: unchanged |
| 43 | Texture-pack cache sized by the graphics card (35 % of its memory, 512 MB - 4 GB), the pack's textures evicted by bytes, least recently drawn first: each texture read from disk once on the cruise route (was 2.2 times) | willzmkv 91b4e40 | S | open: theirs is in texture_replacements.cpp; ours (vcs_texture_pack.cpp) keeps a fixed `VCS_TEXPACK_CACHE_MB` (1024). Needs `rhi::Device::videoMemory` in the three backends. May bear on memcheck's open rise (the pack cache filling) |
| 44 | Downloads: on an SSL certificate error (an antivirus or proxy in the way, a root certificate OpenSSL doesn't see), fall back to Windows' own curl (Schannel) | willzmkv f68c195 (its tools/ part) | S | open: take the curl fallback only - not the retry without certificate checks, nor its VERSION / release note (this fork has none) |

## Later (set aside 2026-11-19, the user's choice: maybe later)

Taken out of the working order; each keeps its number and can come back to it.

| # | What | From | Size |
|---|---|---|---|
| 8 | The game from a CHD image | KenRylo-One, PR #201 | S |
| 9 | Button prompts with a translated GXT (controls by key, not English words) - our own Button prompts (2026-11-22) cover the glyphs, not translations; with it, Portuguese's tilde letters (Ã ã Õ õ) in the game's own fonts | SkipperSkipTR, PR #181; willzmkv a209f0b (the tildes, on their prompts code) | S |
| 11 | Several packs at once with priorities, switched live in F2 (mod folders, Mod Loader's modloader.ini) | willzmkv a8a8587, cc86fcc, 9091a8e | M |
| 12 | Dump as DDS (for pack makers) | willzmkv 5f1b1c2 | S |
| 13 | Game files from mods: GTA3PS2.IMG entries, GXT, MAIN.SCM, larger than the disc's | willzmkv 96edbbe, eed27a2, 69d3d21 | L |
| 15 | PC Vice City car mods (.dff + .txd converted to VCS's models) | willzmkv T9 (afb87df, 6e5f8d9, d177a70...) | XL |
| 16 | PC weapon mods (VC and SA) | willzmkv T11, spartaque12 (Immersive Weapons) | L |
| 17 | DualSense: lightbar, adaptive triggers (a feel per gun), a gun's kick, voices on its speaker | TomRous PR #96, willzmkv 946e1a9, 6d476bf, 40a9d4b, 7c196e2 (the trigger feel follows Drive / Shoot with the triggers) | M |
| 18 | Gamepad: L1 / R1 look sideways while driving, aim assist, D-pad look behind, "GTA V buttons" | willzmkv 3a13a3b, 6d476bf, aebe5fb (the trigger schemes keep the keyboard's and the wheel's bits) | M |
| 25 | Subtitles in Oswald (OFL) with size, shadow, background and colour options | willzmkv 6e14daa, d85fc1e, 27a4a9c | M |
| 28 | First person on foot and in vehicles, viewmodel while aiming, reloads in view | spartaque12 (VCS-mods) | L |
| 29 | Death ragdolls (Jolt Physics) | spartaque12 (VCS-mods) | XL |
| 30 | Vegetation wind sway | spartaque12 (VCS-mods) | M |
| 31 | Shadow maps / ambient occlusion (RenderHook-style) | spartaque12 (VCS-mods) | XL |
| 32 | DLL (.asi) mod API | spartaque12 (VCS-mods) | L |
| 33 | Head shots (PedSkills) - ours (*Headshots always kill*, 2026-11-21) is simpler: a kill, no skills | spartaque12 (VCS-mods) | M |
| 34 | A native main menu / game menu (VC Definitive Edition's layout) | rofhima13 PR #26, willzmkv T12 | L |
| 35 | Launcher: graphics presets from the PC's hardware, shader warm-up, controls on a picture of the pad | willzmkv 283bb56, 9c01e8f | L |
| 36 | Direct3D 12 backend | willzmkv ae6c99e, 28ef950, d9f73ed | XL |
| 45 | The F2 menu and F5 trainer in the launcher's look (its colours, fonts, sliders and switches: a shared ui_theme) | willzmkv 8a37ad6 (found 2026-11-23, put here: it rewrites settings_menu.cpp, which our setLayout re-layout changed) | L |

Rows 11 and 13 looked at again on 2026-11-20 (the user: mods "if needed" after the detailed city): their first commit (cc86fcc) replaces our Graphics > Texture packs page with a Mods page over their `texture_replacements.cpp`, which we do not use (ours: `vcs_texture_pack.cpp`, DDS as stored). A cherry-pick conflicts in main.cpp and every doc. The way to do it: a mod loader of our own that scans `modloader/` folders and hands each mod's textures to `vcs_texture_pack.cpp` as one more source with a priority, then row 13's `vcs_stream.cpp` part, which applies almost as it is (our stream code is theirs before 96edbbe, plus `PS2_RAM_MASK` / `hostNow`), with their `mods_check.py` and `gamefiles_check.py` as the bar. Not started, to keep the detailed city's merge apart.

## Not for us

PAL disc (ChrisM-Pek), browser port (floriandotorg), Android (batinium, KenRylo-One), Intel Macs (knox29 PR #190),
macOS path fixes (tanvir-glide PR #186, only with a space in the build path), localized disc builds and launcher
settings (MaksVolkov7863), CI files (mahoganyprogrammer), willzmkv's bundled mods (HD Hands, HD Hud, PT-BR: shipped from outside their repository,
made from the game's own files, and they need row 11 / 13's mod loader) and their branding, donations and release notes. Already ours: fire on AMD (upstream since 2026-11-04), draw
distance through the game's LOD scale, traffic / pedestrian density, mission timers above 30 fps, the pause menu with
the mouse, HD HUD through the pack, Project2DFX's distant lights.
