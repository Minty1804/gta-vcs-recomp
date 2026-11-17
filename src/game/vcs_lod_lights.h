// Distant lights (Enhancements > World, game-internals.md 7c): at night, the glow of street lamps, windows, signs and
// traffic lights far beyond the ~100 m the game draws its own coronas at - Project2DFX's LOD lights (ThirteenAG's
// WidescreenFixesPack, MIT: its PCSX2 plugin for this game's PS2 version). The light table is that plugin's lodl.c,
// downloaded by the build next to vcs.exe (project2dfx/lodl.c; never committed: tools/deps.py, CMakeLists.txt).
//
// Drawn by the host renderer, not the game: the camera is the frame's own (VU1 memory at the chain's first world
// packet), the glows go into the game's 3D picture after its coronas and before its bloom (gsHwSetOverlay), depth-tested
// against the world. Off by default; VCS_DISTANT_LIGHTS=0/1 for one run.
#pragma once

#include <cstdint>
#include <filesystem>

class PS2Runtime;

namespace vcs::lodlights
{
    // Enhancements > World: Distant lights.
    void registerSettings();
    // Before runtime.run(): the chain observer; the table is read from gameFolder/project2dfx/lodl.c when first needed.
    void install(PS2Runtime &runtime, const std::filesystem::path &gameFolder);
    // Game thread, once a frame: the clock and the timer the lights follow.
    void onGameFrame(uint8_t *rdram);
    // The thread that processes the VIF1 chains, at each VU1 program start (game/vcs_hle_world.cpp's MSCAL hook).
    void onMscal(uint32_t startPc, const uint8_t *vuData);
}
