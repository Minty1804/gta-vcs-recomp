// Graphics > Texture packs: high-resolution replacement textures in PCSX2's format, and dumping the game's textures for
// making them (docs/game-internals.md 7b, docs/DECISIONS.md 2026-11-18).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

class PS2Runtime;

namespace vcs::texpack
{
    // [Minty1804 fork] A folder of PCSX2-named images (anywhere under it), from the mod loader (src/mods/modloader.h):
    // `priority` 1-100 (Mod Loader's; the game folder's own pack is 50), the higher wins a texture two sources have.
    struct Source
    {
        std::filesystem::path dir;
        int priority = 50;
        std::string name;
        bool mod = true; // its dumps/ folders are skipped
    };
    // Main thread: the mods' sources from now on (the game folder's pack stays, when Texture packs is on); the renderer
    // makes every texture again at its next frame. Before install(): kept for it.
    void setModSources(std::vector<Source> sources);
    // Whether Graphics > Texture packs is on (the mods' textures follow it too).
    bool enabled();
    // Graphics > Texture packs: Texture packs (on / off); Advanced > Diagnostics: Dump textures.
    void registerSettings();
    // The pack folder (<game folder>/textures/SLUS-21590/replacements, as PCSX2 lays it out; VCS_TEXTURE_DIR=<dir> for
    // the folder holding replacements/ and dumps/) indexed and handed to the renderer; the `texpack` command.
    void install(PS2Runtime &runtime, const std::filesystem::path &gameFolder);
}
