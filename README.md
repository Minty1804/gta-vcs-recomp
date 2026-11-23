<div align="center">

# GTA: Vice City Stories — Native PC port

**The PS2 game (SLUS-21590, USA) statically recompiled to C++ and drawn natively — with a set of optional
enhancements on top.**

[![Platforms](https://img.shields.io/badge/Windows_10%2F11_%7C_Linux_%7C_macOS_(Apple_Silicon)-00d4ff?style=for-the-badge&labelColor=181425)](#getting-it)
[![Renderer](https://img.shields.io/badge/D3D11_%7C_Vulkan_%7C_Metal-a855f7?style=for-the-badge&labelColor=181425)](docs/NATIVE_RENDERER.md)
[![Branch](https://img.shields.io/badge/branch-mod--reload--walk--camera-ff4fa3?style=for-the-badge&labelColor=181425&logo=git&logoColor=white)](https://github.com/Minty1804/gta-vcs-recomp/tree/mod-reload-walk-camera)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-f59e0b?style=for-the-badge&labelColor=181425)](LICENSE)

[Getting it](#getting-it) ·
[What this fork adds](#what-this-fork-adds) ·
[Port features](#port-features) ·
[Keys](#in-game-keys) ·
[Problems](#problems) ·
[For developers](#for-developers) ·
[Credits](#credits)

</div>

---

This is a fork of [gothamor/gta-vcs-recomp](https://github.com/gothamor/gta-vcs-recomp): a native port of
**Grand Theft Auto: Vice City Stories**, made by recompiling the game's PS2 executable into C++. The game's own code runs
on the PC — not in an emulator — and the 3D world, characters, vehicles, HUD and menus are drawn by a native renderer:
Direct3D 11 on Windows, Vulkan on Linux, Metal on macOS. Sound and streaming run on the PC; the movies are decoded with
FFmpeg.

The fork keeps upstream as it is and adds the changes below: better graphics, a further detailed city, PC-style
controls and combat, a GPS route and a quick save, and a faster renderer. **The original is always the baseline**: what
changes how the game looks or plays is a setting in the menu (F2) with the PS2's behaviour as one of its choices, and
only a few plain fixes nobody would turn off are always on.

> [!IMPORTANT]
> **This repository contains no game code or game data.** You need your own copy of the game: the **USA release
> (SLUS-21590)** as an `.iso`. The build reads the executable from your disc and the finished game reads everything else
> from it while it runs.

> [!NOTE]
> In development. What works: [docs/PROGRESS.md](docs/PROGRESS.md). The newest changes are built and tested on
> Windows; Linux ([docs/LINUX.md](docs/LINUX.md)) and macOS ([docs/MACOS.md](docs/MACOS.md)) build from the same source
> but haven't been re-tested with every recent change.

---

## Getting it

This fork makes **no releases**: you build the branch from git on your own PC. Upstream's release zips and the
launcher's *Update* button install upstream's version, which doesn't have these changes.

**1. Install Python 3 and Git** (once). On Windows: [python.org](https://www.python.org/downloads/) (tick *Add
python.exe to PATH*) and [git-scm.com](https://git-scm.com/download/win). That is all: the build downloads its own
compiler ([llvm-mingw](https://github.com/mstorsjo/llvm-mingw), ~190 MB once) — no Visual Studio needed. On Linux and
macOS see *The build tools by hand* just below.

**2. Get the source** into a short, local folder (like `C:\Games\vcs-recomp`; not Program Files, not a OneDrive folder
such as Desktop or Documents — the build writes thousands of files):

```bash
git clone -b mod-reload-walk-camera https://github.com/Minty1804/gta-vcs-recomp.git vcs-recomp
cd vcs-recomp
```

**3. Build and play**, either way:

| With the launcher (easiest) | By hand |
|---|---|
| `python tools/build.py --launcher` builds `VCSLauncher.exe` in the folder (no disc needed, ~20 s). Start it: it walks you through **Folder**, **Disc** (finds or asks for your `.iso`, checks it's the USA release), **Build tools** and **Build**, then shows **Play**. It also backs up your saves and zips logs for bug reports. | Copy your `.iso` into the folder, then `python tools/build.py --recompile` (Linux / macOS: `python3`). It ends with `[build] ok: build\vcs.exe (optimised: ThinLTO)`. Start `build\vcs.exe` (`build/vcs` on Linux). |

The first build turns the game into about 200 MB of C++ and compiles it: 15 minutes on a 4-core laptop, up to an hour
on a slow PC. Saves live in `%LOCALAPPDATA%\VCSRecomp` (Windows), `~/.local/share/VCSRecomp` (Linux) or
`~/Library/Application Support/VCSRecomp` (macOS), and are zipped into `backups/` before every change.

**Updating**: `git pull`, then `python tools/build.py --recompile-if-needed` (it recompiles only when the recompiler
or its config changed). To follow upstream's newest work on top of this fork's changes:
`git fetch upstream && git rebase upstream/main` (add `upstream` once with
`git remote add upstream https://github.com/gothamor/gta-vcs-recomp.git`).

<details>
<summary><b>The build tools by hand: Windows, Linux and macOS</b></summary>

<br />

**Windows.** Python 3 and Git are all. CMake and Ninja are found wherever they are (Visual Studio's, the CMake
installer, winget, scoop, Chocolatey, pip...); when none is new enough (CMake 3.21, Ninja 1.10) the build downloads
them too (~55 MB). With Visual Studio 2022 or newer (*Desktop development with C++* and *C++ Clang tools for
Windows*), `set VCS_TOOLCHAIN=msvc` builds with its clang-cl instead: the game is the same and as fast.

**Linux.** Python 3 and Git, then in the source folder

```bash
python3 tools/build.py --check-deps
```

lists what is missing and the command that installs it (`apt`, `dnf` or `pacman`): clang 19 or newer with lld, glslang,
and the FFmpeg and SDL3 development packages. On Ubuntu 24.04 / Linux Mint 22 that is

```bash
sudo apt install clang-20 lld-20 llvm-20 cmake ninja-build pkg-config git glslang-tools \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev \
    libwayland-dev wayland-protocols libxkbcommon-dev libdecor-0-dev \
    libpipewire-0.3-dev libpulse-dev libasound2-dev libudev-dev libdbus-1-dev
```

You also need a graphics driver with Vulkan 1.3 (Mesa or NVIDIA's; `vulkaninfo --summary` shows it).

**macOS** (Apple Silicon, macOS 13 or newer). Apple's free Command Line Tools (`xcode-select --install`) and Python 3;
`python3 tools/build.py --check-deps` says what is missing. Details: [docs/MACOS.md](docs/MACOS.md).

Optional check that your disc is the right one:

```bash
python tools/vcs_disc.py info "Grand Theft Auto - Vice City Stories (USA).iso"
```

</details>

---

## What this fork adds

The settings menu (F2) is laid out by what a setting does: **Display**, **Graphics**, **World**, **Game**, **Audio**,
**Keyboard**, **Gamepad**, **Advanced**. Paths below are *Page > Section > Setting*. Who made what: [Credits](#credits).

<table>
<tr>
<td width="50%" valign="top">

### Graphics

- **HD texture packs** made for PCSX2, as they are: PNG and DDS (BC1 / BC3 / BC7, sent to the card compressed).
- **Fog distance and fog start**: the city further, the world's edge still hidden.
- **Exposure**: shadows open up, white stays white, the HUD keeps its colours.
- **Sharp trails**: the blurred colour pass at full resolution.
- A smooth sky at dusk, without colour bands.

</td>
<td width="50%" valign="top">

### World and draw distance

- **Detailed city distance**: the detailed buildings of neighbouring cells instead of the low-detail city, in memory
  beyond the PS2's 32 MB.
- **Pedestrian and car distance** up to twice as far, parked cars too; cars keep their detail further.
- **Distant lights**: ~4,400 street lights glowing over the city at night (Project2DFX's table).
- **Streaming memory** up to 30 MB: more different cars in traffic.
- Far palms solid, far fences see-through, the city in view from interiors.

</td>
</tr>
<tr>
<td width="50%" valign="top">

### Controls and combat

- **The PC games' keyboard scheme**: an action per key by situation, Vice City's default keys, a menu by action as
  in San Andreas.
- **Button prompts** of the device you use, with the key that works there.
- **R reloads**, **one press of F** gets you out of a moving car, a diagonal walk, a mouse camera that stays put.
- **Free aim from the driver's seat** with a pistol or SMG; the wheel switches guns.
- **Headshots always kill**, faster aiming, a smaller or a true-spread aiming circle.

</td>
<td width="50%" valign="top">

### Cutscenes, map, saving and speed

- **Cutscenes** in the PS2's framing in widescreen, the sky fading in with the scene.
- **GPS route**: a line along the roads to the mission's target and to your own map target.
- **Quick save**: F12 opens the game's save menu anywhere outside a mission, and loading puts you back there.
- **A faster renderer**: half the draw calls, state sent only when it changes, runtime speed-ups from the LCS port.
- The HUD no longer flickers in the rain or throws a digit for a frame.

</td>
</tr>
</table>

<details>
<summary><b>Every change in detail: upstream vs this fork, where the setting is and its default</b></summary>

<br />

### Graphics

| | Upstream | This fork |
|---|---|---|
| **Texture packs** | Not supported | HD packs made for PCSX2 work as they are (below). *Graphics > Texture packs*, on by default |
| **Fog** | The game's fog as it is | *Graphics > Fog*: **Fog distance** 10-175% (175% by default: the city further, the world's edge still hidden; 100% is the original) and **Fog start** 0-300% (100%) |
| **Exposure** | The game's brightness as it is | *Graphics > Image quality > Exposure*, 50-200% (100%, the original): shadows open up, white stays white, the HUD keeps its colours |
| **Sky at dusk** | The sky's gradient shows colour bands | Broken up by a fine dither (always on) |
| **Trails** | The blurred trails / colour pass at the PS2's resolution | *Graphics > Effects > Sharp trails*: drawn at full resolution, without the blur (off by default) |

**Texture packs.** Put the pack's `SLUS-21590` folder into a `textures` folder next to the game, so the images end up
in `build\textures\SLUS-21590\replacements\` (sub-folders are fine): PCSX2's own layout and names, so packs need no
conversion. PNG and DDS (BC1 / BC3 / BC7) work; DDS goes to the graphics card compressed where nothing is lost (a
third of the memory). Images load in the background: a texture looks original for a moment, then switches. Tested with
a 6,279-file BC7 pack: 99% of the city's textures replaced, no change in frame rate. *Advanced > Diagnostics > Dump
textures* writes every texture the game draws to `textures\SLUS-21590\dumps\`, for making a pack. Not supported yet:
PCSX2's region (`-r<w>x<h>-`) and mip-level (`-mip<n>`) file names (skipped and counted in the log).

### World and draw distance

| | Upstream | This fork |
|---|---|---|
| **Detailed city distance** | Past the nearest cells the city is its low-detail model | *World > Draw distance*: the detailed buildings of neighbouring cells instead, in memory beyond the PS2's 32 MB (Original by default) |
| **Pedestrian distance** | People appear 35-60 m away | *World > Draw distance*, up to 200% (100% by default); raise the draw distance with it |
| **Car distance** | Traffic about 110 m ahead, parked cars 160 m | *World > Draw distance*, up to twice as far, parked cars too (100% by default) |
| **Cars' detail at a distance** | Low-detail model at 70 m, gone at 100 m, whatever the draw distance | Grows with the draw distance (140 m / 200 m at 200%; always on) |
| **Far palms** | With a texture pack or anisotropic filtering, far fronds look see-through | As solid as up close (always on) |
| **Far palms fading out** | Far palms fade with distance: at long draw distances rows look like ghosts | *World > Details > Solid far palms* (off by default) |
| **Far fences** | A chain-link fence far away turns into a band of sky | Shows what is behind it (always on) |
| **Far view from interiors** | Inside, the camera sees half as far: towers through a window show only while aiming | The city stays in view (always on) |
| **Distant lights** | Street lights vanish past their short corona distance | *World > Details*: Project2DFX's far lights glow over the city at night, ~4,400 of them, hidden behind buildings (off by default; the build downloads the light table, MIT) |
| **Character shadows** | Always on | *World > Details* (on by default) |
| **Streaming memory** | The PS2's streaming buffer | *World > Memory*: 12 / 20 / 30 MB instead of the PS2's 5.4 MB: more different cars in traffic, fewer models read again (PS2 by default) |

### Controls

| | Upstream | This fork |
|---|---|---|
| **Keyboard scheme** | A key per PS2 button (Shift for Cross, Q for L1...) | The PC games' scheme: an action per key by situation (W walks on foot and accelerates in a car, Shift sprints / honks), Vice City's default keys, every key rebindable |
| **Controls menu** | Keys and buttons set by PS2 button | By action, as in San Andreas, for keyboard and gamepad: *On foot*, *Fighting*, *Vehicle*, *Aircraft* (helicopters and planes have their own: climb, turn, bank), *All situations*, *Mouse*, *Menus and pause*, *Shortcuts* |
| **Button prompts** | Always the PS2's buttons | *Game > Button prompts*: the glyphs of the device you use, and the key of the action that works there (shops: W / A / S / D, Space buys) (Original by default) |
| **Reload key** | None: the gun reloads when the clip runs out | **R** reloads with the game's animation, in a vehicle too (at once, even during a drive-by). Gamepad: no button by default |
| **Getting out of a car** | Exit while rolling only brakes; with the gas held you never get out | One press of F: the car brakes to a stop first. *Keyboard > Vehicle > Tap to exit a vehicle* (off: as on the PS2) |
| **Walk on a diagonal** | *Walk* + two direction keys left Vic standing | He walks diagonally (always on) |
| **On-foot camera** | Swings back behind Vic against your mouse as soon as he moves | *Keyboard > Mouse*: stays where the mouse put it for Off / 1-5 s / Always (2 s by default; only after a mouse turn) |

### Combat

| | Upstream | This fork |
|---|---|---|
| **Shooting while driving** | The PS2's drive-by: SMGs only, to the side you look | With free aim, hold the right mouse button to aim a pistol or SMG from the driver's seat; the wheel switches guns (or the radio: *Keyboard > Mouse > Mouse wheel in a vehicle*) |
| **Headshots** | Most guns need several shots to the head | *Game > Combat > Headshots always kill* (on by default); other hits as before |
| **Aim speed** | Vic raises the gun about 0.2 s after the button | *Game > Combat*: Fast (2/3 of the time) or Faster (half) (Original by default) |
| **Aiming circle** | The game's large circle | *Game > Combat*: Original, Small (a dot the guns hit), Spread (the shotgun's real spread) (Original by default) |
| **Free aim up close** | A man right next to you swung the gun instead of being shot; pellets left from the gun, not the circle | Every gun fires up close; pellets spread around the circle (always on) |
| **First shot of the Mac 10 / Micro SMG** | About half a second after you press fire | At once (always on) |
| **Aim after a jump or sprint** | Up to a second late | The frame the landing or sprint ends (always on) |

### Cutscenes, map and saving

| | Upstream | This fork |
|---|---|---|
| **Cutscenes in widescreen** | People the scene keeps off screen stand waiting at the edges; the sky dark grey while a scene fades in | *Display > Cutscenes > Cutscene framing*: the PS2's framing by default (PS2 / Wide); the sky fades in with the scene (always on) |
| **GPS route** | Only the blip to follow | *Game > Map and saving*: a line along the roads to the mission's target, and a red one to your own target on the pause map, on the radar and the map; avoids water, picks the right level where roads cross over (Off / In a vehicle / Always; off by default) - [docs/mods/GPS_ROUTE.md](docs/mods/GPS_ROUTE.md) |
| **Quick save** | Saving only at a safehouse | *Game > Map and saving*: **F12** (*Keyboard > Shortcuts*) opens the game's Save menu anywhere outside a mission; loading puts you back there, in your car or next to it, with your wanted level; optionally the menu comes up after each mission passed (off by default) - [docs/mods/QUICKSAVE.md](docs/mods/QUICKSAVE.md) |

### Performance and fixes (always on)

- Less work per frame in the D3D11 renderer: state sent to the driver only when it changes, half the draw calls
  (neighbouring draws joined, exact where a draw takes two passes), additive pixels that add nothing skipped (the
  helicopter's and hovercraft's dust and glow).
- Runtime speed-ups (VIF1 unpacking, the GS stage's queue and lock, palette hashing, upload bookkeeping) and a GS crash
  fix from the LCS port.
- The HUD: 2D drawn after the HUD no longer drops it (the flicker in the rain), HUD glyphs keep their corners, no money
  digit thrown to the side for a frame.
- Texture packs: no stutter when an HD image comes back into view (each keeps its GPU texture).

</details>

The changes are kept apart from upstream's code (mostly their own files under `src/game/`) so they move to new upstream
versions easily. How they work: [docs/game-internals.md](docs/game-internals.md), [docs/DECISIONS.md](docs/DECISIONS.md).
What other forks have, what was taken and what could come next: [docs/FORK_IMPORTS.md](docs/FORK_IMPORTS.md).

---

## Port features

What the port itself does (upstream's work). The defaults are the PS2 game as it was, except the frame rate: 60 fps
with frame-rate independent timing. *Display > Presets* has **Balanced** (widescreen, 60 fps, twice the original
resolution) and **Authentic (PS2)** to go back to the PS2's 30 fps.

<table>
<tr>
<td width="50%" valign="top">

### Frame rate

- 60, 100, 120, 144, 165, 240 fps, your display's refresh rate or uncapped, with game time following real time — or
  the original timing at 30 fps.
- What the PS2 game counted per frame (vehicle physics, mission meters and timers, particles, effects) put back on real
  time, so it keeps its speed at high frame rates.
- Well past 100 fps nearly everywhere at the original resolution; a locked 60 on a 2015 laptop with integrated
  graphics ([docs/PERFORMANCE.md](docs/PERFORMANCE.md)).
- Low input delay, vsync on or off, a tearing mode for G-Sync / FreeSync.

</td>
<td width="50%" valign="top">

### Picture

- Hor+ widescreen: 16:10, 16:9, 21:9, 32:9 or *Match window* for any shape — more to the sides, nothing cropped or
  stretched. The HUD at 4:3 in the middle, at the screen edges, or stretched.
- Internal resolution 1x (640x448) to 8x, or the window's own; the effects keep their original look.
- MSAA 2x / 4x / 8x, anisotropic filtering up to 16x.
- Windowed or borderless full screen (Alt+Enter).

</td>
</tr>
<tr>
<td width="50%" valign="top">

### Controls and sound

- Keyboard, mouse and gamepads (Xbox, DualShock / DualSense, Switch Pro and anything SDL3 knows), mixed freely
  ([docs/CONTROLS.md](docs/CONTROLS.md)).
- Mouse look on foot as in a PC game; every key and button rebindable, two keys per control; deadzones, camera speed,
  inverted axes; DualShock 2 rumble on your gamepad.
- Master, music, effects and movie volume; stereo, mono or swapped speakers; keep running, mute or pause in the
  background.

</td>
<td width="50%" valign="top">

### Trainer (F5) and more

- A list menu in the style of RDR2's Native Trainer ([docs/TRAINER.md](docs/TRAINER.md)): god mode, wanted level,
  money, weapons, any of the 110 vehicles, teleports, any of the 59 story missions or a side job, time, weather, game
  speed — through the game's own script commands, never saved.
- Saves written and read by the game's own save code; every setting kept in `vcs.ini` next to the game.
- Skip the intro movies (off by default).

</td>
</tr>
</table>

---

## In-game keys

| Key | What |
|---|---|
| **F2** *(or Back + Start)* | Settings menu — saved in `vcs.ini` next to the game |
| **F3** | Performance overlay and the game's state (position, vehicle, camera, clock, scripts) |
| **F4** | Record one CSV row per frame (`python tools/perf_report.py <file>` sums it up) |
| **F5** *(or Back + RB)* | Trainer |
| **F6** *(or Back + LB)* | Free camera |
| **F8** | Screenshot to the `screenshots` folder next to the game |
| **F12** | Quick save, when turned on in *Game > Map and saving* |
| **Alt + Enter** | Full screen / window |

Every shortcut can be moved in *Keyboard > Shortcuts*; the controls themselves: [docs/CONTROLS.md](docs/CONTROLS.md).

---

## Problems

<details>
<summary><b>What you see, and what to do</b></summary>

<br />

| What you see | What to do |
|---|---|
| `llvm-mingw (clang 19 or newer) not found, and downloading it failed` | Check the internet connection and build again; or unpack an llvm-mingw (`ucrt-x86_64`) yourself and set `VCS_LLVM_MINGW` to its folder. |
| `vswhere.exe not found` (with `VCS_TOOLCHAIN=msvc`) | Visual Studio isn't installed (`python tools/setup_vs.py install`), or build without it: unset `VCS_TOOLCHAIN`. |
| CMake configure fails, mentioning `clang-cl` | Open the Visual Studio Installer, *Modify*, and tick **C++ Clang tools for Windows**. |
| `no disc image` | Put the `.iso` in the repository folder, or give its path after `--recompile`. |
| `unsupported executable build` or `SLUS_215.90 not found on disc` | The disc isn't the USA release (SLUS-21590). Other regions aren't supported. |
| CMake configure fails while downloading | Check the internet connection and that `git --version` works in a new terminal. |
| `CMake 3.21 or newer not found, and downloading it failed` | Check the internet connection, or install CMake yourself ([cmake.org](https://cmake.org/download), or Visual Studio Installer, *Modify*, **C++ CMake tools for Windows**). `VCS_CMAKE` / `VCS_NINJA` set to a full path choose a particular one. |
| A step fails within seconds, its log empty or ending in `exit code 0xC000001D (illegal instruction ...)` | The build was made for a newer processor than this PC's. Build again: the build follows the PC's processor (AVX2, AVX or SSE4; `python tools/doctor.py` shows which). Processors without SSE4.2 (before 2008) can't run the game. |
| `ninja: error: manifest 'build.ninja' still dirty` | Files dated in the future (a zip from another time zone, or a wrong clock): set the clock and build again. |
| `vcs.exe` does nothing when started | It can't find the disc: put `iso=C:\path\to\your.iso` in `vcs.ini` next to `vcs.exe`. |
| Linux: `clang 19 or newer not found` / `missing build dependencies` | Install what `python3 tools/build.py --check-deps` lists. |
| Linux: `cannot find -lstdc++` | Install `libstdc++-<N>-dev` for your newest GCC (`build.py` works around it when an older one has it). |
| Linux: the game starts but no window / `no GPU with Vulkan 1.3` | Install your GPU's Vulkan driver (Mesa or NVIDIA); `vulkaninfo --summary` should list it. |

The build's full output is in `build/build.log` (`build/configure.log` for the configure step).
`python tools/doctor.py` checks everything at once (folder, compiler, disc, whether the game is current) and says what
to do next; `python tools/report.py` zips the logs for a bug report (no game files, no saves).

</details>

---

## For developers

<details>
<summary><b>Commands and the repository's layout</b></summary>

<br />

| Command | What |
|---|---|
| `python tools/build.py` | rebuild after code changes |
| `python tools/build.py --quick` | rebuild without link-time optimisation: much faster relinks, a somewhat slower game. It stays quick until `--opt lto` (switching recompiles everything, ~6-10 min) |
| `python tools/build.py --opt lto` | back to the optimised build (the default) |
| `python tools/build.py --opt pgo` | ThinLTO + profile-guided; trains a profile first (~30-50 min, from the rig's card copies in `build/rig/saves` or a new game) |
| `python tools/recompile.py [ISO]` | disc → generated C++ only (`--check`: is generated/ current?) |
| `python tools/build.py --recompile-if-needed [ISO]` | recompile only when the recompiler, its config or the function map changed, then build |
| `python tools/build.py --launcher` | build the launcher (`VCSLauncher.exe` in the repository root; needs no disc) |
| `python tools/doctor.py` | everything checked: folder, memory, compiler, disc, build state, the next step (`--json` for the launcher) |
| `python tools/build_dir.py [set PATH \| reset]` | build in another folder (another drive...) instead of `build\`; every tool follows it |
| `python tools/backup.py create\|list\|restore` | the memory card backed up / restored (order and times kept) |
| `python tools/deps.py fetch` | the libraries the build downloads, into `deps/` (offline builds) |
| `python tools/run.py 0` | run the game with its output logged to `build/run/run.log` (`run.py 60 name`: stop after 60 s, log to `name.log`) |
| `python tools/gamectl.py` | control a running game: screenshots, key presses, perf numbers, quit |
| `python tools/ghidra_analyze.py` | redo the Ghidra analysis (Ghidra 12 + JDK 21) |

All tools are Python 3 scripts using only the standard library. [AGENTS.md](AGENTS.md) is the guide to working on the
code (and the full list of test tools); the documentation is in [docs/](docs).

| Path | What |
|---|---|
| `third_party/PS2Recomp` | Vendored PS2 recompiler + runtime (GPL-3.0) |
| `game/vcs` | Recompiler config and function map for SLUS_215.90 |
| `src` | Game-specific code (overrides, renderer, settings, platform) |
| `tools` | Disc reader, build pipeline, launcher back end, test and analysis scripts |
| `launcher` | The launcher (Windows, Linux and macOS, its own renderer and UI; runs the tools) |
| `docs` | How the game works, decisions, progress |

</details>

---

## Credits

This fork stands on other people's work. Every change brought in from another fork keeps its author on its commit and
is listed here (the full list of candidates, done or not: [docs/FORK_IMPORTS.md](docs/FORK_IMPORTS.md)).

- **The port itself**: [gothamor/gta-vcs-recomp](https://github.com/gothamor/gta-vcs-recomp) and its contributors, on
  [PS2Recomp](https://github.com/ran-j/PS2Recomp).
- **GPS route**: [olaftnt](https://github.com/olaftnt) (upstream PR #183).
- **Quick save**: [olaftnt](https://github.com/olaftnt) (upstream PR #184).
- **Fog distance / start**: [willzmkv](https://github.com/willzmkv) (dejot), from their fork (commit 0d63f27).
- **Exposure**: after [willzmkv](https://github.com/willzmkv)'s (dejot) Exposure (commits 02a6d4c, b5edbaf), re-done
  on the picture before the HUD.
- **Detailed city distance** (the detailed buildings of neighbour cells instead of the low-detail city, and the memory
  above the PS2's 32 MB it lives in) and **Streaming memory** (their T10): [willzmkv](https://github.com/willzmkv) (dejot), from their fork's HD world
  (branch hd-world-merge, last commit 266838d).
- **Sharp trails**: after [aap](https://github.com/aap)'s [sharptrails](https://github.com/aap/sharptrails) (MIT) for
  GTA III / Vice City on PC, re-done for VCS's trails pass in the native renderer.
- **Additive cull** (the helicopter dust's GPU cost): [PerikiyoXD](https://github.com/PerikiyoXD) (upstream PR #178).
- **PC keyboard control scheme**: [SkipperSkipTR](https://github.com/SkipperSkipTR) (upstream PR #78), merged with our
  Reload key added to both schemes.
- **Parked cars at the Car distance**: [willzmkv](https://github.com/willzmkv) (dejot), from their fork (commit 0c41004).
- **Cutscene framing** and the sky in a cutscene's fade-in (and the sniper scope's bars that went with it):
  [willzmkv](https://github.com/willzmkv) (dejot), from their fork (commits 477cfd0, ab1a765, ab6d89c).
- **Free aim while driving** and the wheel's weapons in a vehicle: [ChrisM-Pek](https://github.com/ChrisM-Pek), from
  their fork (commits 741d6a6, a90845f), merged on ours with the USA disc's addresses.
- **D3D11 state cache** (less driver time per draw): after [willzmkv](https://github.com/willzmkv)'s (dejot) commit 4fc7828.
- **Native draws joined** (half the draw calls, fewer hitches): after [willzmkv](https://github.com/willzmkv)'s (dejot)
  commit 0bc5067, re-done so that fences' two-pass draws and render-target reads draw exactly as before.
- **Texture packs**: PCSX2's texture names, reproduced from [PCSX2](https://github.com/PCSX2/pcsx2)'s texture
  replacement code; BC1-BC7 decoding by [bcdec](https://github.com/iOrange/bcdec) (Sergii Kudlai, MIT); DDS uploaded
  compressed after [willzmkv](https://github.com/willzmkv)'s (dejot) commits 37c9194 and 5f1b1c2.
- **Distant lights**: the light table of Project2DFX's VCS version for PCSX2, from ThirteenAG's
  [WidescreenFixesPack](https://github.com/ThirteenAG/WidescreenFixesPack) (MIT).
- **Runtime speed-ups** (VIF1 unpacking, the GS stage's queue and lock, CLUT hashing, upload bookkeeping) and a GS
  crash fix: [batinium](https://github.com/batinium), from
  [gta-stories-recompiled](https://github.com/batinium/gta-stories-recompiled) (commit 1ec8297).
- **Downloads through Windows' curl** when OpenSSL can't check a certificate: [willzmkv](https://github.com/willzmkv)
  (dejot), from their fork (commit f68c195, its tools part).
- **This README's layout** (centred header, badges, feature tables side by side, collapsed sections): after
  [willzmkv](https://github.com/willzmkv)'s (dejot) README for their fork.

## License

GPL-3.0 (inherited from PS2Recomp). Grand Theft Auto, Vice City Stories and all game content are trademarks and
property of Rockstar Games / Take-Two Interactive. This project contains no game files.
