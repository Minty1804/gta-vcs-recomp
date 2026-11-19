// [Minty1804 fork] The port's memory above the game's 32 MB (runtime/ps2_memory.h: 128 MB of EE RAM on the host, the
// game sees the PS2's 32). Its layout, and the world sectors' packet heap moved up there for the detailed city
// (game/vcs_world_hd.cpp). After willzmkv's T10 / HD world (their fork, src/game/vcs_memory.h); their streaming heap
// move (more cars kept loaded) is not part of this.
#pragma once

#include <cstdint>

class PS2Runtime;

namespace vcs::memory
{
    // The HD world's extra cells: their pieces and the drawn lists.
    constexpr uint32_t kHdWorldBase = 0x04000000u;
    constexpr uint32_t kHdWorldSize = 0x03000000u;
    // The world sectors' cached packets (the heap 0x6F3DB8, 512 KB in the main heap on the PS2: when full, every
    // sector's packet is built again each frame into the frame's packet buffer) - 8 MB here while the detailed city is
    // on at boot.
    constexpr uint32_t kPacketHeapBase = 0x07000000u;
    constexpr uint32_t kPacketHeapSize = 0x00800000u;

    // The hook on FUN_002BB768 (the heap build) for the packet heap; `memstate` (the packet heap's size and use).
    void install(PS2Runtime &runtime);
    // Whether the packet heap was built up here (0x6F3DB8's base at kPacketHeapBase).
    bool packetHeapMoved();
}
