// [Minty1804 fork] Graphics presets by what the PC can run (Display > Presets: For this PC, Low, Medium, High, Ultra), and
// the first start's pick: the preset this PC runs with room to spare. After willzmkv's (dejot) launcher presets and its
// first-install pick (283bb56, 9c01e8f), re-done in the game (docs/DECISIONS.md 2026-11-23): our launcher has no game
// settings page, and vcs.exe knows its own settings' ranges and the card it will draw with.
#pragma once

#include <string>

namespace vcs::presets
{
    // What the PC has: the graphics card the game will take (Graphics > Graphics card, else the fastest), the processor
    // and the memory.
    struct Hardware
    {
        std::string gpuName;        // empty when no card could be listed
        unsigned long long gpuMb = 0; // the card's own memory (0: unknown)
        bool integrated = false;    // the processor's chip, on the system's memory
        int threads = 0;            // logical processors
        bool avx2 = false;
        int ramMb = 0;              // physical memory
    };
    Hardware probe();

    // 0 Original (nothing changed), 1 Low, 2 Medium, 3 High: the lowest of what the card, the processor and the memory
    // each allow (willzmkv's tiers); never Ultra, which is the player's to choose. `why`: one line on what was read.
    int recommend(const Hardware &hw, std::string &why);
    const char *tierName(int tier); // "Original", "Low", "Medium", "High", "Ultra"

    // The Display > Presets rows of this module, after Authentic / Balanced (main.cpp calls it there).
    void registerSettings();
    // After settings::load, before applyAll, in the player's window only: a new install - vcs.ini holding nothing but
    // iso= (no setting of the player's, no earlier pick: presets.first_start) and no save on the memory card
    // (`cardHasSaves`: a player who has played with every default keeps them) - gets the recommended preset as its
    // start values (no restart). VCS_AUTO_PRESET=0 skips it; =1 runs it in a test run too (the check: vcs.ini is kept
    // there, nothing written).
    void firstStart(bool cardHasSaves);
}
