# What this fork changes, in detail

Every change against upstream ([gothamor/gta-vcs-recomp](https://github.com/gothamor/gta-vcs-recomp)): what upstream
does, what this fork does, where the setting is (*Page > Section > Setting* in the F2 menu) and its default. The
README has the short list; who made what is in its Credits and in [FORK_IMPORTS.md](FORK_IMPORTS.md). How each change
works: [game-internals.md](game-internals.md) and [DECISIONS.md](DECISIONS.md).

The changes are kept apart from upstream's code (mostly their own files under `src/game/`) so they move to new upstream
versions easily.
<br />

## Graphics

| | Upstream | This fork |
|---|---|---|
| **Texture packs** | Not supported | HD packs made for PCSX2 work as they are (below). *Graphics > Texture packs*, on by default |
| **Fog** | The game's fog as it is | *Graphics > Fog*: **Fog distance** 10-175% (175% by default: the city further, the world's edge still hidden; 100% is the original) and **Fog start** 0-300% (100%) |
| **Exposure** | The game's brightness as it is | *Graphics > Image quality > Exposure*, 50-200% (100%, the original): shadows open up, white stays white, the HUD keeps its colours |
| **Sky at dusk** | The sky's gradient shows colour bands | Broken up by a fine dither (always on) |
| **Trails** | The blurred trails / colour pass at the PS2's resolution | *Graphics > Effects > Sharp trails*: drawn at full resolution, without the blur (off by default) |
| **Graphics API** | Direct3D 11 (Vulkan on Linux) | *Graphics > Graphics API*: **D3D12** too, less processor time per draw; its compiled pipelines kept between runs, so a place seen before has no compile hitch (D3D11 by default) |
| **Presets** | Authentic, Balanced | *Display > Presets*: **For this PC**, **Low**, **Medium**, **High**, **Ultra**, from your card, processor and memory; a new install starts with For this PC's pick (your own settings are never changed) |

**Texture packs.** Put the pack's `SLUS-21590` folder into a `textures` folder next to the game, so the images end up
in `build\textures\SLUS-21590\replacements\` (sub-folders are fine): PCSX2's own layout and names, so packs need no
conversion. PNG and DDS (BC1 / BC3 / BC7) work; DDS goes to the graphics card compressed where nothing is lost (a
third of the memory). Images load in the background: a texture looks original for a moment, then switches. Tested with
a 6,279-file BC7 pack: 99% of the city's textures replaced, no change in frame rate. *Advanced > Diagnostics > Dump
textures* writes every texture the game draws to `textures\SLUS-21590\dumps\`, for making a pack. Not supported yet:
PCSX2's region (`-r<w>x<h>-`) and mip-level (`-mip<n>`) file names (skipped and counted in the log).

## World and draw distance

| | Upstream | This fork |
|---|---|---|
| **Detailed city distance** | Past the nearest cells the city is its low-detail model | *World > Draw distance*: the detailed buildings of neighbouring cells instead, in memory beyond the PS2's 32 MB (Original by default) |
| **Pedestrian distance** | People appear 35-60 m away | *World > Draw distance*, up to 200% (100% by default); people are drawn as far as they live: the game's 70 m draw cut for peds grows with this setting when it is above the draw distance (`VCS_PED_DRAW_FOLLOW=0`: the cut follows the draw distance only) |
| **Car distance** | Traffic about 110 m ahead, parked cars 160 m | *World > Draw distance*, up to twice as far, parked cars too (100% by default) |
| **Cars' detail at a distance** | Low-detail model at 70 m, gone at 100 m, whatever the draw distance | Grows with the draw distance (140 m / 200 m at 200%; always on) |
| **Far palms** | With a texture pack or anisotropic filtering, far fronds look see-through | As solid as up close (always on) |
| **Far palms fading out** | Far palms fade with distance: at long draw distances rows look like ghosts | *World > Details > Solid far palms* (off by default) |
| **Far fences** | A chain-link fence far away turns into a band of sky | Shows what is behind it (always on) |
| **Far view from interiors** | Inside, the camera sees half as far: towers through a window show only while aiming | The city stays in view (always on) |
| **Distant lights** | Street lights vanish past their short corona distance | *World > Details*: Project2DFX's far lights glow over the city at night, ~4,400 of them, hidden behind buildings (off by default; the build downloads the light table, MIT) |
| **Character shadows** | Always on | *World > Details* (on by default) |
| **Streaming memory** | The PS2's streaming buffer | *World > Memory*: 12 / 20 / 30 MB instead of the PS2's 5.4 MB: more different cars in traffic, fewer models read again (PS2 by default) |

## Controls

| | Upstream | This fork |
|---|---|---|
| **Keyboard scheme** | A key per PS2 button (Shift for Cross, Q for L1...) | The PC games' scheme: an action per key by situation (W walks on foot and accelerates in a car, Shift sprints / honks), Vice City's default keys, every key rebindable |
| **Controls menu** | Keys and buttons set by PS2 button | By action, as in San Andreas, for keyboard and gamepad: *On foot*, *Fighting*, *Vehicle*, *Aircraft* (helicopters and planes have their own: climb, turn, bank), *All situations*, *Mouse*, *Menus and pause*, *Shortcuts* |
| **Button prompts** | Always the PS2's buttons | *Game > Button prompts*: the glyphs of the device you use, and the key of the action that works there (shops: W / A / S / D, Space buys) (Original by default); **Icon style**: Kenney's Input Prompts or the classic drawn ones |
| **Reload key** | None: the gun reloads when the clip runs out | **R** reloads with the game's animation, in a vehicle too (at once, even during a drive-by). Gamepad: no button by default |
| **Getting out of a car** | Exit while rolling only brakes; with the gas held you never get out | One press of F: the car brakes to a stop first. *Keyboard > Vehicle > Tap to exit a vehicle* (off: as on the PS2) |
| **Walk on a diagonal** | *Walk* + two direction keys left Vic standing | He walks diagonally (always on) |
| **On-foot camera** | Swings back behind Vic against your mouse as soon as he moves | *Keyboard > Mouse*: stays where the mouse put it for Off / 1-5 s / Always (2 s by default; only after a mouse turn) |

## Combat

| | Upstream | This fork |
|---|---|---|
| **Shooting while driving** | The PS2's drive-by: SMGs only, to the side you look | With free aim, hold the right mouse button to aim a pistol or SMG from the driver's seat; the wheel switches guns (or the radio: *Keyboard > Mouse > Mouse wheel in a vehicle*) |
| **Headshots** | Most guns need several shots to the head | *Game > Combat > Headshots always kill* (on by default); other hits as before |
| **Aim speed** | Vic raises the gun about 0.2 s after the button | *Game > Combat*: Fast (2/3 of the time) or Faster (half) (Original by default) |
| **Aiming circle** | The game's large circle | *Game > Combat*: Original, Small (a dot the guns hit), Spread (the shotgun's real spread) (Original by default) |
| **Free aim up close** | A man right next to you swung the gun instead of being shot; pellets left from the gun, not the circle | Every gun fires up close; pellets spread around the circle (always on) |
| **First shot of the Mac 10 / Micro SMG** | About half a second after you press fire | At once (always on) |
| **Aim after a jump or sprint** | Up to a second late | The frame the landing or sprint ends (always on) |

## Cutscenes, map and saving

| | Upstream | This fork |
|---|---|---|
| **Cutscenes in widescreen** | People the scene keeps off screen stand waiting at the edges; the sky dark grey while a scene fades in | *Display > Cutscenes > Cutscene framing*: the PS2's framing by default (PS2 / Wide); the sky fades in with the scene (always on) |
| **GPS route** | Only the blip to follow | *Game > Map and saving*: a line along the roads to the mission's target, and a red one to your own target on the pause map, on the radar and the map; avoids water, picks the right level where roads cross over (Off / In a vehicle / Always; off by default) - [docs/mods/GPS_ROUTE.md](mods/GPS_ROUTE.md) |
| **Subtitles** | The game's letters | *Game > Subtitles*: **Oswald** for the subtitles, mission text and help boxes - sharp at any resolution, with size, outline, shadow, a box behind each line and white text as options (Original by default) |
| **Quick save** | Saving only at a safehouse | *Game > Map and saving*: **F12** (*Keyboard > Shortcuts*) opens the game's Save menu anywhere outside a mission; loading puts you back there, in your car or next to it, with your wanted level; optionally the menu comes up after each mission passed (off by default) - [docs/mods/QUICKSAVE.md](mods/QUICKSAVE.md) |

## Performance and fixes (always on)

- Less work per frame in the D3D11 renderer: state sent to the driver only when it changes, half the draw calls
  (neighbouring draws joined, exact where a draw takes two passes), additive pixels that add nothing skipped (the
  helicopter's and hovercraft's dust and glow).
- Runtime speed-ups (VIF1 unpacking, the GS stage's queue and lock, palette hashing, upload bookkeeping) and a GS crash
  fix from the LCS port.
- The HUD: 2D drawn after the HUD no longer drops it (the flicker in the rain), HUD glyphs keep their corners, no money
  digit thrown to the side for a frame.
- Texture packs: no stutter when an HD image comes back into view (each keeps its GPU texture, in a cache sized by the
  card's memory).
- The detailed city: Downtown's construction building keeps its blue glass.
- Traffic at a raised Car distance: the car spawner no longer builds cars only to throw them away (86 - 88 % of them
  in Little Havana at 200 %).

## Mods

| | Upstream | This fork |
|---|---|---|
| **Mod loader** | None | `modloader/` next to the game, one folder per mod, PC GTA's Mod Loader `modloader.ini` (priorities, ignores); *Mods* in F2: Load mods, a switch and a priority per mod, applied while the game runs (on by default; with no mods it does nothing) |
| **What a mod can hold** | - | PCSX2-named textures (as a texture pack); VCS's own files by name (`admiral.mdl`, `ENGLISH.GXT`, `MAIN.SCM`), any size; PC Vice City cars (`.dff` + `.txd`) and VC / SA weapons, converted while the game loads - [MODS.md](MODS.md) |
| **Cutscene characters** | Gameplay models: mitten hands, plain faces | `python tools/csped.py build` makes a mod from your disc: Vic's 14 outfits and 16 story characters drawn in gameplay with their cutscene meshes (faces, fingers) on the gameplay bones; switched in *Mods* like any other - [MODS.md](MODS.md) |

