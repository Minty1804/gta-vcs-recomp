# Mods: the mod loader, game files, PC cars and weapons

From willzmkv's (dejot) fork ([willzmkv/gta-vcs-recomp](https://github.com/willzmkv/gta-vcs-recomp): a8a8587, cc86fcc,
9091a8e, 96edbbe, eed27a2, 69d3d21, afb87df, 6e5f8d9, d177a70 and later; FORK_IMPORTS rows 11, 13, 15, 16), taken on
2026-11-23 at their `main` and fitted to this fork. Most of this page is theirs: how VCS's files work and what their
converters do. Where a measurement was made in their fork it says so; what was checked here is under "Checked in this
fork".

PC GTA mods (`.asi` / `d3d8.dll` plugins, RenderWare `.dff` / `.txd`) cannot load as they are: vcs.exe is VCS (PS2)
recompiled, drawing through its own renderer, with RSL (Leeds' RenderWare Stories) rather than PC RenderWare. What the
mod loader does is give VCS's own files from a folder, and convert the PC files that map onto VCS's (cars, weapons)
while the game loads. A player's mods are their own: never commit anything from `modloader/` but its README.

## The mod loader

PC GTA's Mod Loader (LINK/2012) made mods folders: nothing of the game's is replaced on disc, everything is injected
at run time, each mod can be switched off in a menu, and mods can be swapped while the game runs.
`src/mods/modloader.cpp` does the same for this port:

- **Where**: `modloader/` beside vcs.exe, or in the checkout's root when vcs.exe is in `build/` (`VCS_MODLOADER_DIR`
  elsewhere). `modloader/README.md` is the only file of it in git.
- **A mod** is any folder in it (a leading `.` hides one). `modloader/modloader.ini` is read in Mod Loader's own
  format, so a player's knowledge carries over: `[Folder.Config] Profile`, and the profile's `[...Priority]
  Mod=1..100` (50 by default, 0 ignores the mod; a higher one wins a file two mods have), `[...IgnoreMods]`
  (wildcards; `_ignore` when there is no file yet, as Mod Loader ships it) and `[...Config] IgnoreAllMods`. Profiles
  beyond that (parents, `UseIfModule`) are not read. The game **makes the file** in `modloader/` when it is missing
  (Mod Loader's template, every installed mod listed at 50), so it is there to see and edit.
- **Priorities in F2**: a slider (1-100) beside each mod's switch. It writes `modloader.ini` (its line replaced, or
  added to the profile's Priority section; the rest of the file kept), so the file stays the one source: the
  priorities are not in vcs.ini (`settings::Spec::saved = false`, read back with `read`), and "Read the mods again"
  reads an edited file again. Reset page to defaults puts every mod back on, at 50.
- **F2 > Mods**: Load mods (`mods.enabled`), one switch per mod (`mods.<name>`, with its file count and priority as
  help), Read the mods again (`mods.rescan`) and Mods folder (`mods.open`, `platform::openFolder`). **Textures**: a
  mod's PCSX2-named `.dds` / `.png` files (anywhere in it, `dumps/` aside) are one more source of this fork's texture
  packs (`vcs::texpack::Source`, with the mod's priority; the game folder's own pack, `textures/SLUS-21590`, counts as
  50), so they are read as the packs are - DDS uploaded as stored, the pack's caches - and Graphics > Texture packs
  (`graphics.texture_pack`) switches them with the rest. Where willzmkv's fork has a "Replace textures" switch of the
  mod loader's own, here the packs' switch is the one.
- **Live**: a switch is applied at the next host frame (`vcs::mods::onHostFrame`): the sources are indexed again and
  `gsHwTexturePackChanged()` tells the GS stage, which at its next frame makes every texture again and looks each up
  in the new index (the kept GPU textures of pack images go too: a handle may name another file now). Game files
  follow at the next read (below). A new mod folder shows after a restart (the settings registry is built at
  start-up).
- **Test runs stay authentic**: a hidden run loads mods only with `VCS_MODS=1` (which also fixes the switch), or
  `VCS_TEST_MODS=1` (switches left free for the `setting` command), and writes nothing into the player's
  `modloader/` - only into a folder of its own given by `VCS_MODLOADER_DIR`.
- **Checked in willzmkv's fork** (`tools/mods_check.py`): a `modloader/` with no ini of "HD" (an HD pack, linked),
  "Tinted" (half of a city spot's dumped textures painted magenta), `_ignore` and `.hidden`: the game makes
  `modloader.ini` (HD and Tinted at 50, `_ignore` ignored); at 50 / 50 HD wins the shared textures; Tinted at 60 wins
  them, `Tinted=60` written to the ini; Tinted off, the packs off and on again, Load mods off - each a swap, pass.

The T numbers below are willzmkv's work list (their FORK.md), kept to find the commits in their fork.

### Game files: GTA3PS2.IMG entry by entry (T1c, 2026-11-03)

As the player suggested: no archive is rebuilt or replaced - a mod holds only the entries it changes, and the game reads
those from the mod. VCS's model archive has no directory on the disc (game-internals.md 9d): the game's own table gives
each entry its sectors, and its model infos keep each name's key, so a file named after a model finds its entry.

- **In a mod**: `<model name>.mdl` (`admiral.mdl`), `<texture dictionary>.xtx` (`admiral.xtx`: the TXD store's
  names) or `#<streaming index>.<anything>` (`#1234.bin`) for an archive entry, and a loose disc file by its own name (`AMERICAN.GXT`, `ENGLISH.GXT`, `MAIN.SCM`), anywhere in the mod's folder. The priority slider decides between two mods with the same entry. **Any size**: a smaller file is
  padded with zeros; a larger one has its record moved past the end of the disc (the streaming info's offset and
  size, LSN 0x180000 on, and the archive's size raised to reach it), where only the mod's bytes are served - the
  game allocates the mod's size and the entry touches no neighbour; the game's values come back when the mod goes.
- **How**: the mod loader hands the list to the stream layer (`vcs::setStreamFiles`); at the next read the stream
  thread resolves each name against the game's tables (the disc's file table for GTA3PS2.IMG's LSN, the model infos'
  keys, the streaming info's offset / size) and lays the mod's bytes over every disc read that covers the entry. An
  entry already in memory changes when the game streams it again (cars, peds, weapons: when they come back into the
  scene). **Vic's model** the game never drops (the player's report, 2026-11-13: a mod switched off stayed on, even
  after loading the save): an entry whose served bytes changed is "dirty", and where the game dresses the player (an
  outfit change, a save's load) the request first has the game remove the dirty model - its dictionary goes with it
  when nothing else holds it - so both are read anew; a switch of a mod holding his model or dictionary dresses him
  again at once in his outfit (UNDRESS_CHAR, on foot; in a vehicle at the next dressing). Checked with HD Hands
  switched off / on / off in play: the hands follow each switch. `VCS_MOD_FILES=0`: none.
- **`tools/img.py`**: `table` (the directory from a hidden run into `build/img/streaming.json`), `list`, `extract`
  (from the player's disc image into `build/img/GTA3PS2`, named `admiral.mdl` / `#1234.bin`): an extracted folder is a
  mod as it is. The entries are the game's data - never in git.
- **Checked in willzmkv's fork** (`tools/gamefiles_check.py`): an ADMIRAL spawned in the street with no mod, with its own entry from a mod
  (the same picture: 0.32) and with the CHEETAH's entry as `admiral.mdl` (a Cheetah drawn as "Admiral": 1.82); and the
  text files from a mod with "Admiral" written "ADMIRAL" - the game's text for that key reads "ADMIRAL" - pass.
  And the ADMIRAL's entry (37 sectors) as `cheetah.mdl` (30): a CHEETAH spawned draws the Admiral's body, read from
  past the disc. A loose file larger than the disc's moves the same way (its file-table record).

### `.dff` / `.txd`: what PC mods replace, and what VCS has

PC GTA III / VC / SA mods are RenderWare 3 files: `.dff` (a clump: frames, geometry, materials, skin) and `.txd` (a
texture dictionary), loaded by the PC executable's RenderWare and streamed from `gta3.img`. **VCS on the PS2 has
neither**: its models are Leeds' RenderWare Stories (RSL) structures with PS2-native geometry (VU1 packets, game-
internals.md 6d), in `GTA3PS2.IMG` and the world's `BEACH / MAINLA / MALL.IMG` + `.LVZ`; its textures are `.XTX`
(GS formats, section 7). A PC `.dff` / `.txd` dropped into the game would not be read. Two different things can be
built:

1. **VCS's own files from a mod** (T1c) - what Mod Loader's `std.stream` does on PC, for VCS's formats: a file in a
   mod named as an archive entry (or a loose file: `MAIN.SCM`, the `.GXT` texts) is read in place of the disc's. The
   disc reads are native already (`src/game/vcs_stream.cpp`), so the bytes can come from the mod's file; what has to
   be found first is each archive's directory (entry names, offsets, sizes - where VCS keeps them: `GAME.DTZ`?) and
   what the game assumes about an entry's size (buffers sized from the directory). Mods made with VCS / LCS
   modding tools for the PS2 / PSP versions would then work.
2. **PC `.dff` / `.txd` themselves** (T9, research): either converted to VCS's formats ahead of time (RW 3 clump ->
   RSL structures with PS2 strips and VU1 data, TXD -> XTX), or loaded on the host and drawn by the native renderer in
   place of a model the game submits (needs draws at the RSL level: docs/RSL_SUBMISSION.md, milestone 7). Either is a
   project of its own; the texture half is already possible today as a texture pack (textures are matched by content,
   not by file).

### T9 findings: PC VC car mods in VCS (2026-11-03)

- **The tutorial** the player pointed to (MixMods, vehicles in GTA SA PS2:
  https://forum.mixmods.com.br/f250-tutoriais/t1411-gta-ps2-veiculos) sits behind a Cloudflare JavaScript check; the
  player pasted the pages into a text file (kept locally, not in git). It holds aap's "About SkyGFX" and the
  forum's follow-up thread of tips for converting PC cars to SA PS2 (the t1411 steps themselves are not in it). What
  carries over to VCS:
  - **Keep the original car's collision**: converted cars with their own collision float or sink; the fix is the
    replaced car's collision put back (RW Analyze). In VCS the mod loader replaces only the `.mdl`: the collision,
    handling and model info stay the VCS car's - the same choice, by construction.
  - **Textures as PS2 paletted rasters (PAL4 / PAL8, Magic.TXD)**: unpaletted ones weigh far more in the game and
    flicker. VCS's own are mostly PSMT4 + CLUT (game-internals.md 6e). For the converter: either quantise a `.txd`'s
    DXT textures to PAL4 / PAL8 rasters in the vehicle's TXD (authentic, lossy), or hand them to the renderer by name
    through the texture packs (no quantisation, no PS2 memory limits) - the second first.
  - The tutorial itself (DanielSantos, pasted later; a local copy, not in git):
    PC car -> SA PS2 is the `.txd` to PS2 format (Magic.TXD), the `.dff` through aap's RenderWare File Converter
    (`convdff`, the original mesh or - better - tristrips), the collision rebuilt as COL1 and put back into the `.dff`
    (COL Editor, HxD, RWAnalyze), and the files put into `gta3.img` / `gta3_1.img`; models over ~600 KB slow the game.
    `convdff` writes RenderWare's PS2 geometry - SA's format, not VCS's RSL `ldm` - so its output does not load here;
    the port's converter writes `ldm` (tristrips too), and the mod loader spares the collision and IMG steps.
  - aap: RSL is "at the high level a simplified copy & paste of RW", "at the lower levels completely different" - the
    node / element / geometry structure maps from a `.dff`, the PS2 mesh data has to be built anew. VCS's vehicle
    reflections: an environment map rendered from the camera with coronas, alpha blended (LCS adds it).
- **`tools/rwinfo.py`** reads PC RenderWare files on the host (stdlib only; `--self-test` writes and reads a small
  clump and texture dictionary): a `.dff`'s frames with their names and parents, geometries (vertices, triangles,
  materials and texture names; PS2-native flagged), atomics; a `.txd`'s textures (name, size, D3D8 DXT / palette /
  D3D9 FourCC, levels). Checked on their player's VC mods (`Fixed_Xbox_Vehs_1.3`: `comet.dff`, 33 frames and 6
  geometries; `comet.txd`, 256x256 DXT1 with 9 levels and 8-bit palettes) - read only.
- **The match**: a VCS vehicle's `.mdl` (game-internals.md 9d) carries the very frame names of a VC car's `.dff`
  (`chassis_dummy`, `chassis_hi` / `_vlo`, `wheel_*_dummy`, `door_*_dummy`, `bonnet` / `boot` / `bump_*` /
  `windscreen` / `wing_*`), so a VC car's parts line up with what VCS's vehicle code looks for. Its textures have names
  too, in the same style (`admiral868bit_sl` vs VC's `comet868bit128`). VC car mods also come in Mod Loader's folder
  layout (`gta3.img/comet.dff`).
- **The routes**:
  - (c) a `.txd`'s textures by name over a VCS vehicle's: only for skins made for VCS's own models - VCS remodelled
    its cars (other UV layouts), so a VC car's textures on a VCS model would map wrong. Not the way to reuse VC cars.
  - (a) **convert a `.dff` (+ its `.txd`) into a VCS `ldm` vehicle resource** offline (a `tools/dff2mdl.py`): the
    result goes through T1c's game files as `<car>.mdl`, and the game's own paths draw it (the CHEETAH-as-ADMIRAL swap
    shows the game takes another `ldm`). **Chosen**: self-contained, the game's own paths, and T1c already carries it.
  - (b) load the `.dff` on the host and draw it in place of the model's atomics: no PS2 limits, but it needs draws at
    the RSL level (docs/RSL_SUBMISSION.md) and the vehicle code's frame logic mirrored. Kept as the alternative.
- **Route (a) done** for cars: below.

### PC VC cars in VCS: `tools/dff2mdl.py` and `tools/txd2xtx.py` (T9, 2026-11-03)

A VC car's `.dff` and `.txd` become a VCS vehicle's two archive entries, `<vcs car>.mdl` and `<vcs car>.xtx`. **The
mod loader does it itself**: a VC car mod put into `modloader/` as it comes (`comet.dff` + `comet.txd`, in any
subfolder - Mod Loader's `gta3.img/` layout too) replaces VCS's car of that name, converted while the game loads
(`src/mods/rwconv.cpp`: the Comet in 21 + 55 ms; the 80 cars of `Fixed_Xbox_Vehs_1.3` in 4.1 s at boot). A `.txd` is
taken only beside a `.dff` of its name. The same conversion as tools, to look at a result or make VCS files by hand:

```
python tools/img.py extract                  # once: the disc's vehicles into build/img/GTA3PS2 (the templates)
python tools/dff2mdl.py convert comet.dff build/img/GTA3PS2/admiral.mdl "modloader/VC Comet/admiral.mdl" --txd comet.txd
python tools/txd2xtx.py convert comet.txd "modloader/VC Comet/admiral.xtx"
python tools/carmod_check.py                 # a VC car mod's Comet as the ADMIRAL, in the game (the mod: --help)
```

- **The model** (`dff2mdl convert DFF TEMPLATE OUT [--txd TXD]`): the VCS vehicle replaced is the template - its
  handling, collision, wheels, sounds and model info stay (the tutorial's advice, by construction).
  - **Nodes**: each VCS node with a VC frame of its name is put where that frame is relative to the root, whatever the
    trees in between (`wheel_*_dummy`, `door_*_dummy`, `brakelights` ...); the root itself is the vehicle's place.
  - **Parts** (each element gets VC parts): the part of its node's name (`chassis_hi`, a bike's `forks_front` /
    `wheel_front` / `handlebars`, a boat's `boat_hi` and flaps, `extra1`); an `X_dummy`'s intact element (object flag
    4) `X_hi_ok`, its damaged one `X_hi_dam`; a helicopter's `static_rotor(2)` VC's `toprotor` / `rearrotor` (VCS's
    `moving_rotor` blur discs stay theirs: the game fades them in with the speed). VC parts left over: damaged ones,
    extras and LODs go; the rest - the wings VCS builds into its chassis, small parts - join `chassis_hi` (or a boat's
    hull) where they are. Elements no part reaches are emptied (one degenerate triangle): VCS-only pieces (`car_parts`,
    `part1`, `glass_hi`, a van door's second piece) would show VCS geometry with textures the new dictionary lacks.
  - **Meshes**: the triangles from the bin mesh (exporters leave the triangles' own material numbers at 0), strips
    started at the triangle with the fewest free neighbours and grown both ways, one PS2 mesh per material (and per
    UV repeat when a material spans more than two: the 8-bit UVs keep the game's precision), encoded as the game's own
    (`--self-test`: 3466 / 3466 byte for byte), with each mesh's and the geometry's int16 bounding box (game-internals
    9d). NaN in a `.dff` reads as 0.
- **Materials**: one per VC material, with VC's texture name (found in the `.xtx` below) and colour; VC's paint colours
  (`3CFF00` first, `FF00AF` second) put the material into the vehicle's paint lists - the game colours those with the
  car's colours - and lend it the template's paint effects (the reflection). A texture the `.txd` lacks maps to VCS's
  shared one where there is one (`vehiclegeneric256` -> `xv_vehiclegeneric_sl`, ...) or goes (the material's colour
  alone).
- **The textures** (`txd2xtx convert TXD OUT [--max 128] [--keep VCS.xtx]`): D3D8 textures (DXT1 / 3 / 5, 8-bit
  palettes, 8888 / 888 / 1555 / 565 / 4444 / LUM8) decoded, halved down to 128 pixels (VCS's own vehicle textures are
  128 at most) and quantised (median cut) to 4 bit when nothing is lost, else 8 bit - the format of every texture on
  the disc. The VCS dictionary's own textures the `.txd` has no name for follow, as they are (a helicopter's `rotor`
  blur). `info FILE.xtx --png DIR` shows a dictionary's textures (the disc's too); `tools/img.py extract
  build/img/GTA3PS2 "*"` writes the vehicles' models and dictionaries (`admiral.mdl`, `admiral.xtx`).
- **The mod loader** takes `<texture dictionary>.xtx` by the TXD's name (the game's TXD store keeps the names; a
  vehicle's is the vehicle's name), as `<model>.mdl` by the model's. VC names VCS shortened map to VCS's: `boxville`,
  `enforcer`, `fbiranch`, `police`, `sabretur`, `washing` -> `boxvil`, `enforce`, `fbiran`, `policem`, `sabretb`,
  `washin`. A VC vehicle VCS does not have (Infernus, Landstalker ...) is named in the log: rename its files as the
  vehicle it should replace. VC's `wheels.dff` / `.txd` are not used yet: VCS's wheel models are resident (T10).
- **Checked in willzmkv's fork** (`tools/carmod_check.py`): the ADMIRAL with no mod, then the Comet in its place (both entries larger than
  the disc's: 292 KB model, 165 KB textures) - spawned in the street, the player in it, from behind: the VC Comet with
  its own textures, the game's paint, no crash.
- **In the game, as they come** (`carmod_check.py --direct`, `--folder`): the Comet's `.dff` / `.txd` named
  `admiral.*` - converted, resolved, the same picture; the whole `Fixed Xbox Vehicles/gta3.img` folder as one mod
  (160 files converted, none failed): the VC Xbox Comet, Cheetah, Taxi, Voodoo, Stinger, Mesa and PCJ-600 spawned,
  each VC's model with its textures. The C++ and Python converters agree byte for byte on all 236 `.dff` / `.txd` of
  the player's mods (`tools/rwconv_check.py`).
- **Checked triangle by triangle** (`tools/carmod_verify.py FOLDER [--native]`): both cars put together as drawn -
  the VC one from its frames, the converted one from its node tree and decoded PS2 meshes - intact and damaged; every
  VC triangle must be at its place (5 mm) with its texture and UVs, and nothing else may be there. 67 / 67 vehicles of
  `Fixed_Xbox_Vehs_1.3` (both converters), and the in-game pictures from four sides (`carmod_check.py --views`, the
  rig's `campose` free-camera pose) for cars, a bike, boats and helicopters.
- **Not yet**: VC's wheels (`wheels.dff`, resident in VCS); VC extras where VCS has no extra element of that name;
  memory - a VC car with its textures is 4-5x a VCS car, and VCS streams into a fixed 5.4 MB heap (game-internals.md
  9e): loading many different VC cars at once can exhaust it. This fork's Streaming memory (up to 30 MB, FORK_IMPORTS
  row 14) gives it more room.

### PC VC weapons in VCS (T11, 2026-11-03)

A VC weapon mod's `.dff` + `.txd` (`uzi.dff`, `uzi.txd`) in `modloader/` replace VCS's weapon of that name, converted
while the game loads like a car. VC's names VCS has under its own map to VCS's: `colt45`, `m4`, `tec9`, `mp5lng`,
`m60`, `ruger` -> `beretta`, `m16`, `skorpion`, `mplng`, `m249`, `ak47` (SA's `micro_uzi`, `sawnoff` -> `uzi`,
`buddyshot`). The mod loader takes a `.dff` whose VCS model
is a vehicle (model type 6) or a weapon (type 4); other models' (objects') files are named in the log and left out.

- **The model**: a weapon's resource is one element (object type 1 at 0x20, its node unnamed: game-internals.md 9g),
  so every VC atomic goes into it - no paint lists, no extras - **moved from VC's place in the hand to VCS's**: VCS
  holds each weapon a few centimetres further along the barrel and turned (melee weapons up to 30 degrees), so a VC
  weapon as it comes sat in Vic's palm. Each weapon's transform (`dff2mdl.WEAPON_PLACES`, `kWeaponPlaces` in
  rwconv.cpp) was fitted from VC's original models onto VCS's (`tools/weaponfit.py --vc <VC>/models/gta3.img`);
  a VC mod sits where VC's original does, so it lands where VCS's does.
- **San Andreas weapon mods** (2026-11-03, the player's `Improved Default Armoury`): SA's `.txd`s are D3D9 (platform 9,
  the DXT format named by its D3DFORMAT) - read too, before which every texture was missing and the model drawn white.
  SA's weapons carry their muzzle flash as a part (`gunflash`, shown by SA only while firing): left out, VCS draws its
  own. SA's minigun holds its spinning barrels as a part (`minigun2`) of `minigun.dff` and `minigun2.dff` alike, where
  VC and VCS keep them as a model of their own (VCS's 366, turned while firing): converted onto `minigun` the barrels
  stay out, onto `minigun2` only they go in, in their own frame - before, both models got the whole gun and it showed
  twice. SA holds its weapons where VC does (the outlines over VCS's models with the VC table: pistol 0.46 -> 0.80,
  chainsaw 0.52 -> 0.93, flame-thrower 0.33 -> 0.67, M4 0.37 -> 0.50), so the same table places them. SA's
  `micro_uzi` and `sawnoff` map to VCS's `uzi` and `buddyshot`; SA-only weapons (desert eagle, silenced pistol, country
  rifle, ...) have no VCS model and are named in the log. Checked: `rwconv_check` 104 / 104, `weaponmod_check` PASS
  (16 weapons in the hand, textured, VCS's icons).
- **The HUD icon stays VCS's**: a weapon's dictionary holds its HUD icon, the texture named like the weapon, and VC's
  `.txd` has its own (VC's style) under that name. `txd2xtx --preserve NAME` writes VC's texture of that name as
  `NAME_pc`, `dff2mdl --weapon NAME` points the model's materials naming it there, and `--keep` adds VCS's icon.
- **Textures** whose sides are not powers of two (a mod's 1048 x 1024) are resampled to the powers of two below
  (nearest) before halving: VCS's rasters hold log2 of each side.
- By hand, as for cars (`python tools/img.py extract build/img/GTA3PS2 uzi` names weapons by their dictionary):

```
python tools/dff2mdl.py convert uzi.dff build/img/GTA3PS2/uzi.mdl "modloader/HD Uzi/uzi.mdl" --txd uzi.txd --weapon uzi
python tools/txd2xtx.py convert uzi.txd "modloader/HD Uzi/uzi.xtx" --keep build/img/GTA3PS2/uzi.xtx --preserve uzi
python tools/weaponmod_check.py              # modloader/HD Weapons in the game: each weapon in Vic's hand, the HUD
```

- **Checked in willzmkv's fork**: `tools/rwconv_check.py` - C++ and Python the same on the 22 files of the player's `HD Weapons` and the
  239 of their car mods (with the placement table; the cars' output unchanged by the weapon path: 119 / 119 byte for
  byte against the T9 converter); `tools/weaponmod_check.py` - the 10 weapons of `HD Weapons` converted, each drawn in
  Vic's hand where VCS holds its own (the chainsaw's handles in his hands, the pistol's grip in his palm), each
  covering VCS's model better than at VC's place (silhouette overlap, e.g. the pistol 0.50 -> 0.78, the molotov
  0.14 -> 0.65), with VCS's HUD icon, no crash (`build/weaponmod_check/placed`).
- **Cost** (measured in willzmkv's fork): an HD weapon in view costs 1-2 ms a frame: faceted HD meshes make one
  draw per ~70 strip vertices. The conversion at boot runs on every core (194 files in 1.7 s).

## Checked in this fork (2026-11-23)

- `tools/rwinfo.py --self-test`, `txd2xtx.py --self-test`, `gxt.py --self-test`: pass; `dff2mdl.py --self-test`:
  4926 / 4926 of the disc's vehicle meshes encoded byte for byte.
- `mods_check --pack build/textures/SLUS-21590`: pass - modloader.ini written, a mod's texture over the pack at a
  higher priority and back below it, switched while the game runs. Fitted to this fork: it starts from the card copy
  (a new game's opening cutscene began partway through its steps), and its magenta is the game's lit (243, 0, 197).
- `gamefiles_check`: pass - the Admiral's model entry replaced by the same file and by the Cheetah's, the Cheetah's by
  the larger Admiral (moved past the disc's entry), `ADMIRAL` read from a mod's AMERICAN.GXT. Fitted: the GXTs are
  found through the disc's file table (the sectors written in before were another dump's), and the swap is told by the
  car's outline instead of the whole picture's difference (from above the car is small, and the same file moved it a
  few pixels or added exhaust smoke): the outlines centred on each other overlap 0.84 - 0.93 for the same file, 0.34
  for the Cheetah.
- `carmod_check`, `carmod_verify`, `weaponmod_check`, `rwconv_check`: not run - they need PC car / weapon mods, and
  none are here.
