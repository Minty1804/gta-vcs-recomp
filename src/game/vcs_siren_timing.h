// The horn tap that switches an emergency vehicle's lights / siren, on real time above 30 fps (willzmkv, gothamor#209;
// docs/game-internals.md 4, "The horn's history").
#pragma once

class PS2Runtime;

namespace vcs
{
    // CPad 0's 8-frame horn history (CPad+0x9A..0xA2, one entry a frame on the PS2) kept on the game's unscaled ms clock,
    // so a tap shorter than the PS2's 7 frames (~233 ms) still toggles the lights at any frame rate.
    // VCS_SIREN_TIMING=0: per frame, as the game has it; VCS_SIREN_LOG=1 logs each press / hold / release.
    void installSirenTimingHooks(PS2Runtime &runtime);
}
