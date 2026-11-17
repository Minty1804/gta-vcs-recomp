// The on-foot camera and the mouse (game-internals.md 12b, "A camera on a string"; DECISIONS.md 2026-11-16).
//
// The game's follow camera on foot turns by itself while Vic moves: it is measured each frame from where it stood
// to where he is now (a string: moving across the view it pivots after him), and it swings behind a walking player.
// With the mouse that turn fought the hand - holding D the camera went round one way while the mouse pulled it the
// other (the user's report). For a while after the mouse turns it (Keyboard > On-foot camera hold), and only on the
// keyboard and mouse, a hook before the camera's update (FUN_0029DAB0) takes both away: the points the camera stands
// at move with him, the swing's cap is 0, its speeds start from rest. Then the game's own camera carries on.
// VCS_FOOTCAM=0: not installed (the original).
#pragma once

#include <cstdint>

class PS2Runtime;

namespace vcs::footcam
{
    void install(PS2Runtime &runtime);

    // Game thread, at the pad read: the mouse turned the on-foot camera this frame / the right stick (the gamepad's or
    // the camera keys) is in use, which hands it back to the game at once.
    void mouseTurned();
    void stickMoved();

    // Keyboard > On-foot camera hold: milliseconds of game time after the mouse's last turn; 0 off, -1 always (while
    // the keyboard and mouse are in use).
    void setHold(int ms);
}
