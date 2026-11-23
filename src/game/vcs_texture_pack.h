// Graphics > Texture packs: high-resolution replacement textures in PCSX2's format, and dumping the game's textures for
// making them (docs/game-internals.md 7b, docs/DECISIONS.md 2026-11-18).
#pragma once

#include <filesystem>

class PS2Runtime;

namespace vcs::texpack
{
    // Graphics > Texture packs: Texture packs (on / off); Advanced > Diagnostics: Dump textures.
    void registerSettings();
    // The pack folder (<game folder>/textures/SLUS-21590/replacements, as PCSX2 lays it out; VCS_TEXTURE_DIR=<dir> for
    // the folder holding replacements/ and dumps/) indexed and handed to the renderer; the `texpack` command.
    void install(PS2Runtime &runtime, const std::filesystem::path &gameFolder);
}
