<div align="center">

# GTA: Vice City Stories — Native PC port

**The PS2 game (SLUS-21590, USA) recompiled to C++ and drawn natively, with optional enhancements on top.**

[![Platforms](https://img.shields.io/badge/Windows_10%2F11_%7C_Linux_%7C_macOS_(Apple_Silicon)-00d4ff?style=for-the-badge&labelColor=181425)](#getting-it)
[![Renderer](https://img.shields.io/badge/D3D12_%7C_D3D11_%7C_Vulkan_%7C_Metal-a855f7?style=for-the-badge&labelColor=181425)](docs/NATIVE_RENDERER.md)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-f59e0b?style=for-the-badge&labelColor=181425)](LICENSE)

[Getting it](#getting-it) · [What this fork adds](#what-this-fork-adds) · [Keys](#in-game-keys) ·
[Problems](#problems) · [Credits](#credits)

</div>

---

A fork of [gothamor/gta-vcs-recomp](https://github.com/gothamor/gta-vcs-recomp), the native port of **Grand Theft
Auto: Vice City Stories**: the game's PS2 code recompiled to C++ and run on the PC, the world drawn by a native
renderer. Upstream's port is kept as it is; this fork adds a further detailed city, PC-style controls and combat, a mod
loader, a GPS route and a quick save, and a faster renderer. **The original is the baseline**: every change to how the
game looks or plays is a setting in the F2 menu, with the PS2's behaviour as one of its choices.

> [!IMPORTANT]
> **No game code or game data is in this repository.** You need your own **USA (SLUS-21590)** disc as an `.iso`: the
> build reads the executable from it, and the game reads everything else from it while it runs.

## Getting it

**The easy way**: download the newest [release](https://github.com/Minty1804/gta-vcs-recomp/releases/latest)
(Windows: `VCS-Recomp-<version>-windows.zip`; Linux: the AppImage; macOS: the `.dmg`), extract it into a short local
folder, start `VCSLauncher.exe` and follow **Folder -> Disc -> Build tools -> Build -> Play**. The launcher's *Updates*
follows this fork's releases. Upstream's releases install upstream's version, without these changes.

**From git** (the newest changes, between releases):

1. Install **Python 3** and **Git**. Windows needs nothing else: the build downloads its own compiler (~190 MB, once).
   Linux and macOS: `python3 tools/build.py --check-deps` says what to install
   ([docs/LINUX.md](docs/LINUX.md), [docs/MACOS.md](docs/MACOS.md)).
2. Clone into a short local folder (not Program Files, not OneDrive):

   ```bash
   git clone -b mod-reload-walk-camera https://github.com/Minty1804/gta-vcs-recomp.git vcs-recomp
   cd vcs-recomp
   ```

3. Build and play: `python tools/build.py --launcher`, then start `VCSLauncher.exe` and follow **Folder → Disc →
   Build tools → Build → Play**. By hand: put the `.iso` in the folder, run `python tools/build.py --recompile` and
   start `build\vcs.exe`. The first build takes 15-60 minutes.

**Updating**: `git pull`, then `python tools/build.py --recompile-if-needed`. Saves are in `%LOCALAPPDATA%\VCSRecomp`
(Linux `~/.local/share/VCSRecomp`, macOS `~/Library/Application Support/VCSRecomp`) and are backed up before every
change.

## What this fork adds

The most important differences from upstream. Every change, with its upstream behaviour, setting and default:
[docs/FORK.md](docs/FORK.md).

| | |
|---|---|
| **Detailed city distance** | Full-detail buildings far out instead of the low-detail city; cars and people drawn further (up to 2×), parked cars too |
| **Graphics** | Direct3D 12; HD texture packs in PCSX2's format; fog distance, exposure, sharp trails; ~4,400 distant street lights at night; presets for your PC |
| **Mods** | A mod loader as PC GTA's (`modloader/`, switched live in F2); PC Vice City car mods and VC / SA weapon mods converted while the game loads ([docs/MODS.md](docs/MODS.md)) |
| **Cutscene characters** | Vic and the story characters in gameplay with their cutscene faces and hands, made from your disc by `tools/csped.py` |
| **Controls and combat** | The PC games' keyboard scheme; a reload key; free aim from the driver's seat; button prompts for your device; headshots that kill |
| **Map and saving** | A GPS route along the roads; quick save anywhere outside a mission ([docs/mods/](docs/mods)) |
| **Look** | Cutscenes in the PS2's framing; Oswald subtitles; the F2 and F5 menus in the launcher's look |
| **Fixes** | High-frame-rate bugs (taxi tip, sirens, radio name), HUD flicker in the rain, half the draw calls |

What the port itself does (60+ fps with correct timing, Hor+ widescreen, up to 8× resolution, keyboard / mouse /
gamepad, the F5 trainer) is upstream's work: see [docs/PROGRESS.md](docs/PROGRESS.md).

## In-game keys

| Key | What |
|---|---|
| **F2** *(Back + Start)* | Settings (saved in `vcs.ini`) |
| **F3** | Performance and game-state overlay |
| **F5** *(Back + RB)* | Trainer |
| **F6** *(Back + LB)* | Free camera |
| **F8** | Screenshot |
| **F12** | Quick save, when turned on |
| **Alt + Enter** | Full screen / window |

## Problems

<details>
<summary><b>What you see, and what to do</b></summary>

<br />

| What you see | What to do |
|---|---|
| A download fails during the build | Check the connection and build again; it resumes. |
| `unsupported executable build` / `SLUS_215.90 not found` | Only the USA disc (SLUS-21590) works. |
| A step fails at once with `0xC000001D` | Built for a newer processor: build again on this PC. |
| `build.ninja still dirty` | Files dated in the future: fix the clock and build again. |
| `vcs.exe` does nothing | Put `iso=C:\path\to\your.iso` in `vcs.ini` next to it. |
| Linux: no window / `no GPU with Vulkan 1.3` | Install your GPU's Vulkan driver. |

`python tools/doctor.py` checks everything and says the next step; `python tools/report.py` zips the logs for a bug
report (no game files, no saves).

</details>

## For developers

[AGENTS.md](AGENTS.md) is the guide to the code, its rules and every tool; [docs/](docs) has how the game works
inside ([game-internals.md](docs/game-internals.md)), the decisions and the progress.

## Credits

Everything taken from another fork keeps its author on its commit; the commits and what was done with each are in
[docs/FORK_IMPORTS.md](docs/FORK_IMPORTS.md).

- **The port**: [gothamor/gta-vcs-recomp](https://github.com/gothamor/gta-vcs-recomp) and its contributors, on
  [PS2Recomp](https://github.com/ran-j/PS2Recomp).
- **[willzmkv](https://github.com/willzmkv)** (dejot): detailed city distance and streaming memory, fog, exposure,
  parked cars and traffic at the Car distance, cutscene framing and the fade-in sky, the D3D11 state cache and joined
  draws, compressed DDS packs and their cache, the mod loader with PC car and weapon mods, Oswald subtitles and
  Kenney's prompts, the menus' look, Direct3D 12, hardware presets, curl downloads, the 2026-11-24 fixes (taxi tip,
  sirens, radio name, car aim, BMX camera, mission titles, Fort Baxter, DIA_C3, RC cars, the stick's dead zone), the
  idea for cutscene characters (their HD Hands), and this README's layout.
- **[olaftnt](https://github.com/olaftnt)**: GPS route and quick save (upstream PRs #183, #184).
- **[SkipperSkipTR](https://github.com/SkipperSkipTR)**: the PC keyboard scheme (upstream PR #78).
- **[ChrisM-Pek](https://github.com/ChrisM-Pek)**: free aim while driving, the wheel's weapons.
- **[PerikiyoXD](https://github.com/PerikiyoXD)**: the additive cull (upstream PR #178).
- **[batinium](https://github.com/batinium)**: runtime speed-ups and a GS crash fix, from
  [gta-stories-recompiled](https://github.com/batinium/gta-stories-recompiled).
- **[aap](https://github.com/aap)**: [sharptrails](https://github.com/aap/sharptrails) (MIT), re-done for VCS.
- **ThirteenAG**: Project2DFX's light table, from [WidescreenFixesPack](https://github.com/ThirteenAG/WidescreenFixesPack) (MIT).
- **Libraries and assets**: [PCSX2](https://github.com/PCSX2/pcsx2)'s texture names; [bcdec](https://github.com/iOrange/bcdec)
  (Sergii Kudlai, MIT); [Oswald](https://github.com/googlefonts/OswaldFont) (The Oswald Project Authors, SIL OFL 1.1);
  [Kenney](https://kenney.nl)'s Input Prompts (CC0); [stb](https://github.com/nothings/stb) (Sean Barrett, MIT /
  public domain).

## License

GPL-3.0 (inherited from PS2Recomp). Grand Theft Auto, Vice City Stories and all game content are trademarks and
property of Rockstar Games / Take-Two Interactive. This project contains no game files.
