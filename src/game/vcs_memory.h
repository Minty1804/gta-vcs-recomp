// [Minty1804 fork] The port's memory above the game's 32 MB (runtime/ps2_memory.h: 128 MB of EE RAM on the host, the
// game sees the PS2's 32; docs/game-internals.md 9d). Its layout: the streaming heap (World > Memory > Streaming memory),
// the detailed city's extra cells (game/vcs_world_hd.cpp) and the world sectors' packet heap. After willzmkv's (dejot)
// T10 and HD world (their fork, src/game/vcs_memory.h).
#pragma once

#include <cstdint>

class PS2Runtime;

namespace vcs::memory
{
    // The streaming heap (every streamed model and texture dictionary; 5.4 MB in the main heap on the PS2), built here
    // with Streaming memory above the PS2's - at most 30 MB, below the detailed city.
    constexpr uint32_t kStreamingBase = 0x02000000u;
    constexpr uint32_t kStreamingMaxSize = 0x01E00000u;
    // The detailed city's extra cells: their pieces and the drawn lists.
    constexpr uint32_t kHdWorldBase = 0x04000000u;
    constexpr uint32_t kHdWorldSize = 0x03000000u;
    // The world sectors' cached packets (the heap 0x6F3DB8, 512 KB in the main heap on the PS2: when full, every
    // sector's packet is built again each frame into the frame's packet buffer) - 8 MB here while the detailed city is
    // on at boot.
    constexpr uint32_t kPacketHeapBase = 0x07000000u;
    constexpr uint32_t kPacketHeapSize = 0x00800000u;
    // Small buffers the port hands the game's code (game/vcs_world_detail.cpp: the world scan's fan polygon).
    constexpr uint32_t kPortScratchBase = 0x07800000u;
    constexpr uint32_t kPortScratchSize = 0x00001000u;
    static_assert(kStreamingBase + kStreamingMaxSize <= kHdWorldBase, "the streaming heap runs into the detailed city");
    static_assert(kHdWorldBase + kHdWorldSize <= kPacketHeapBase, "the detailed city runs into the packet heap");
    static_assert(kPacketHeapBase + kPacketHeapSize <= kPortScratchBase, "the packet heap runs into the port's scratch");

    // World > Memory > Streaming memory (applies at the next start: the heap is built at boot).
    void registerSettings();
    // The hooks on FUN_002E5688 (the streaming heap), FUN_003C9DE8 (the 51-cap) and FUN_002BB768 (the packet heap), and
    // `memstate` (the heaps' sizes and use).
    void install(PS2Runtime &runtime);
    // Whether the packet heap was built up here (0x6F3DB8's base at kPacketHeapBase).
    bool packetHeapMoved();
    // The packet heap's memory [from, to) as FUN_002BB768 built it (here or in the main heap); false before it is built.
    // Only blocks inside it are the game's cached packets to free: a table entry can also hold a packet of the level's
    // own data (game-internals.md 6t).
    bool packetHeapRange(uint32_t &from, uint32_t &to);
    // The streaming heap's size in MB as built at boot (0: the game's 5.4 MB in its main heap).
    int streamingMegabytes();
}
