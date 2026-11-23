// [Minty1804 fork] Game > Combat: a gunshot to the head kills (docs/game-internals.md 12c "Headshots").
#pragma once

class PS2Runtime;

namespace vcs::headshots
{
    // Game > Combat: "Headshots always kill" (on: the user's wish, 2026-11-20).
    void registerSettings();
    // The hook on CPed::InflictDamage; the `headshot` command. VCS_HEADSHOT_HOOKS=0: none (the original game).
    void install(PS2Runtime &runtime);
}
