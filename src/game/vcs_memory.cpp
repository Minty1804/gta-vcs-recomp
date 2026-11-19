// [Minty1804 fork] The port's memory above the game's 32 MB (game/vcs_memory.h). After willzmkv's vcs_memory.cpp (their
// fork: T10 and the HD world), the packet heap part only.
//
// FUN_002BB768(heap, base, size, granularity) builds a heap; the world sectors' cached packets live in the one at
// 0x6F3DB8, built once at boot (called from 0x369384) with 512 KB taken from the main heap (game-internals.md 6o,
// willzmkv's 9j). With the detailed city on at boot it is built in the upper memory with 8 MB instead: the extra cells'
// sectors add their packets, and a full heap makes the game build packets again every frame. The main heap's block
// stays allocated, unused. Every other heap as it was; with the detailed city off at boot nothing changes.
#include "game/vcs_memory.h"

#include "game/vcs_world_hd.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "runtime/ps2_control.h"

#include <atomic>
#include <cstdio>
#include <cstring>

namespace vcs::memory
{
    namespace
    {
        constexpr uint32_t kBuildHeap = 0x002BB768u;
        constexpr uint32_t kPacketHeap = 0x006F3DB8u; // heap object: +0 size, +0xC first block, +0x14 in use

        PS2Runtime::RecompiledFunction g_originalBuild = nullptr;
        std::atomic<bool> g_moved{false};
        std::atomic<uint8_t *> g_rdram{nullptr};

        uint32_t rd32(const uint8_t *rdram, uint32_t a)
        {
            uint32_t v;
            std::memcpy(&v, rdram + (a & PS2_RAM_MASK), 4);
            return v;
        }

        PS2_GUEST_CC void buildHeap(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            g_rdram.store(rdram, std::memory_order_relaxed);
            if (GPR_U32(ctx, 4) == kPacketHeap && vcs::world_hd::radius() > 0)
            {
                std::fprintf(stderr, "[memory] sector packet heap: %u KB at 0x%08x (the game's: %u KB at 0x%08x)\n",
                             kPacketHeapSize / 1024u, kPacketHeapBase, GPR_U32(ctx, 6) / 1024u, GPR_U32(ctx, 5));
                SET_GPR_U32(ctx, 5, kPacketHeapBase);
                SET_GPR_U32(ctx, 6, kPacketHeapSize);
                g_moved.store(true, std::memory_order_relaxed);
            }
            g_originalBuild(rdram, ctx, runtime);
        }
    }

    bool packetHeapMoved() { return g_moved.load(std::memory_order_relaxed); }

    void install(PS2Runtime &runtime)
    {
        if ((g_originalBuild = runtime.lookupFunction(kBuildHeap)))
            runtime.replaceFunction(kBuildHeap, &buildHeap);
        else
            std::fprintf(stderr, "[memory] FUN_%08X not found: the sector packet heap stays the game's\n", kBuildHeap);
        // memstate -> ok packets=<base>,<size>,<in use> moved=<0|1> (bytes)
        ps2control::addCommand("memstate", [](const std::string &, ps2control::Reply reply) {
            const uint8_t *rdram = g_rdram.load(std::memory_order_relaxed);
            if (!rdram)
                return reply("error the heaps are not built yet");
            char text[160];
            std::snprintf(text, sizeof(text), "ok packets=0x%08x,%u,%u moved=%d", rd32(rdram, kPacketHeap + 0xCu),
                          rd32(rdram, kPacketHeap), rd32(rdram, kPacketHeap + 0x14u), packetHeapMoved() ? 1 : 0);
            reply(text);
        });
    }
}
