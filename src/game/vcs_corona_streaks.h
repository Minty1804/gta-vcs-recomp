// The headlights' streaks hidden behind walls and peds (docs/game-internals.md 6g, "Headlight streaks over the player").
#pragma once

class PS2Runtime;

namespace vcs
{
    // CCoronas::Render (0x0027C418) draws a corona's streak (its last 6 screen points, as lines over everything) only
    // while a line-of-sight test from the corona to the camera passed, a test the game repeats every 2 s and with peds
    // left out: a car's streaks crossed walls for up to 2 s, and Vic always. Here the test runs every PS2 frame (33 ms)
    // and counts peds. VCS_STREAK_LOS=0: the game's (every 2 s, no peds); VCS_STREAK_LOG=1: how many are in sight, every 5 s.
    void installCoronaStreakHooks(PS2Runtime &runtime);
}
