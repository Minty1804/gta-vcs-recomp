// [Minty1804 fork] The port's memory above the game's 32 MB (game/vcs_memory.h; docs/game-internals.md 9d). After
// willzmkv's (dejot) vcs_memory.cpp, their fork's T10 and HD world.
//
// THE STREAMING HEAP. FUN_002E5688(heap 0x51F420) builds it once at boot: FUN_002BD178(0x567000) takes 5.4 MB from the
// main heap (0x51D6A0), the pointer goes to heap + 0x130, FUN_002BB768(heap, it, 0x567000, 0) builds the heap there, and
// heap + 0x12C = 1. CStreaming allocates every model and texture dictionary from it (FUN_003C6478) and, when it is full,
// unloads what it can and defragments (FUN_0021C300) - the heap's size is the game's whole streaming budget
// (FUN_003CB760 makes room against its free space alone). With Game > Memory > Streaming memory above the PS2's the hook
// does the same in the upper memory: no main heap allocation (the main heap keeps the 5.4 MB), the two fields, and
// FUN_002BB768 as a guest tail call (DECISIONS.md, hooks) with the larger size.
//
// THE CACHE CAP. CStreaming's request (FUN_003C7ED8) also unloads the least recently used resource
// (FUN_003C9DE8(streaming, 0x83), the call at 0x3C8030) whenever its loaded list (streaming + 0xD8) holds 51 (0x33) or
// more, whatever the memory. With the larger heap that call is skipped (v0 = 0: the caller does not read it), so what
// stays loaded is bounded by memory: FUN_003CB760 still unloads (through FUN_003C9F10, the other caller) when an entry
// does not fit.
//
// THE SECTOR PACKET HEAP. FUN_002BB768's heap at 0x6F3DB8 holds the world sectors' cached packets, built once at boot
// (called from 0x369384) with 512 KB taken from the main heap (game-internals.md 6t). With the detailed city on at boot it
// is built in the upper memory with 8 MB instead: the extra cells' sectors add their packets, and a full heap makes the
// game build packets again every frame. The main heap's block stays allocated, unused.
//
// The layout (vcs_memory.h): the streaming heap at 0x2000000 (at most 30 MB), the detailed city's cells at 0x4000000
// (48 MB), the packet heap at 0x7000000 (8 MB), the top 8 MB free. PS2 settings leave every heap as the game builds it.
#include "game/vcs_memory.h"

#include "game/vcs_world_hd.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "runtime/ps2_control.h"
#include "settings/settings.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vcs::memory
{
    namespace
    {
        constexpr uint32_t kBuildStreamingHeap = 0x002E5688u;
        constexpr uint32_t kBuildHeap = 0x002BB768u;
        constexpr uint32_t kRemoveLeastUsed = 0x003C9DE8u;
        constexpr uint32_t kCapCallReturn = 0x003C8038u; // the return address of the 51-cap's call in FUN_003C7ED8
        constexpr uint32_t kStreamingHeap = 0x0051F420u; // heap object: +0 size, +0xC first block, +0x14 in use
        constexpr uint32_t kMainHeap = 0x0051D6A0u;
        constexpr uint32_t kPacketHeap = 0x006F3DB8u;
        constexpr uint32_t kGameStreamingSize = 0x00567000u;

        PS2Runtime::RecompiledFunction g_originalStreaming = nullptr;
        PS2Runtime::RecompiledFunction g_originalRemove = nullptr;
        PS2Runtime::RecompiledFunction g_originalBuild = nullptr;
        std::atomic<int> g_megabytes{0};     // the setting
        std::atomic<int> g_builtMegabytes{0}; // what the heap was built with at boot (0: the game's)
        std::atomic<bool> g_packetsMoved{false};
        std::atomic<uint32_t> g_packetFrom{0}, g_packetTo{0};
        std::atomic<uint64_t> g_capSkips{0};
        std::atomic<uint8_t *> g_rdram{nullptr};

        uint32_t rd32(const uint8_t *rdram, uint32_t a)
        {
            uint32_t v;
            std::memcpy(&v, rdram + (a & PS2_RAM_MASK), 4);
            return v;
        }
        void wr32(uint8_t *rdram, uint32_t a, uint32_t v) { std::memcpy(rdram + (a & PS2_RAM_MASK), &v, 4); }

        PS2_GUEST_CC void buildStreamingHeap(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            g_rdram.store(rdram, std::memory_order_relaxed);
            const int megabytes = std::clamp(g_megabytes.load(std::memory_order_relaxed), 0, int(kStreamingMaxSize >> 20));
            if (megabytes <= 0)
                return g_originalStreaming(rdram, ctx, runtime);
            const uint32_t heap = GPR_U32(ctx, 4);
            const uint32_t size = static_cast<uint32_t>(megabytes) << 20;
            wr32(rdram, heap + 0x130u, kStreamingBase); // what FUN_002BD178 would have returned
            wr32(rdram, heap + 0x12Cu, 1u);             // the original sets it after the build; the build does not read it
            SET_GPR_U32(ctx, 5, kStreamingBase);
            SET_GPR_U32(ctx, 6, size);
            SET_GPR_U32(ctx, 7, 0u);
            ctx->pc = kBuildHeap; // FUN_002BB768(heap, base, size, 0), returning to our caller
            g_builtMegabytes.store(megabytes, std::memory_order_relaxed);
            std::fprintf(stderr, "[memory] streaming heap: %u KB at 0x%08x (the game's: %u KB from its main heap)\n",
                         size / 1024u, kStreamingBase, kGameStreamingSize / 1024u);
        }

        // FUN_003C9DE8(streaming, flags): from the 51-cap's call with the larger heap, nothing unloaded (v0 = 0, the call's
        // result is not read there); every other call runs the original
        PS2_GUEST_CC void removeLeastUsed(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (GPR_U32(ctx, 31) == kCapCallReturn && g_builtMegabytes.load(std::memory_order_relaxed) > 0)
            {
                g_capSkips.fetch_add(1, std::memory_order_relaxed);
                SET_GPR_U32(ctx, 2, 0u);
                ctx->pc = GPR_U32(ctx, 31);
                return;
            }
            g_originalRemove(rdram, ctx, runtime);
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
                g_packetsMoved.store(true, std::memory_order_relaxed);
            }
            if (GPR_U32(ctx, 4) == kPacketHeap)
            {
                g_packetFrom.store(GPR_U32(ctx, 5), std::memory_order_relaxed);
                g_packetTo.store(GPR_U32(ctx, 5) + GPR_U32(ctx, 6), std::memory_order_relaxed);
            }
            g_originalBuild(rdram, ctx, runtime);
        }
    }

    bool packetHeapMoved() { return g_packetsMoved.load(std::memory_order_relaxed); }
    bool packetHeapRange(uint32_t &from, uint32_t &to)
    {
        from = g_packetFrom.load(std::memory_order_relaxed);
        to = g_packetTo.load(std::memory_order_relaxed);
        return to > from;
    }
    int streamingMegabytes() { return g_builtMegabytes.load(std::memory_order_relaxed); }

    void registerSettings()
    {
        settings::Spec s;
        s.key = "memory.streaming";
        s.page = "Game";
        s.section = "Memory";
        s.label = "Streaming memory";
        s.help = "Room for the models and textures the game streams from the disc (cars, people, props, weapons). The PS2 "
                 "has 5.4 MB; more keeps more of them loaded (more different cars in traffic, fewer models dropped and read "
                 "again) and leaves room for larger mods. The city's buildings stream on their own (Enhancements > World > "
                 "Detailed city distance). From the next start.";
        s.type = settings::Type::Choice;
        s.choices = {{"PS2 (5.4 MB)", 0}, {"12 MB", 12}, {"20 MB", 20}, {"30 MB", 30}};
        s.defaultValue = 0;
        s.apply = settings::Apply::Restart;
        s.env = "VCS_STREAMING_MB";
        s.onChange = [](int v) { g_megabytes.store(std::max(0, v), std::memory_order_relaxed); };
        settings::add(s);
    }

    void install(PS2Runtime &runtime)
    {
        if ((g_originalStreaming = runtime.lookupFunction(kBuildStreamingHeap)))
            runtime.replaceFunction(kBuildStreamingHeap, &buildStreamingHeap);
        else
            std::fprintf(stderr, "[memory] FUN_%08X not found: the streaming heap stays the game's\n", kBuildStreamingHeap);
        if ((g_originalRemove = runtime.lookupFunction(kRemoveLeastUsed)))
            runtime.replaceFunction(kRemoveLeastUsed, &removeLeastUsed);
        if ((g_originalBuild = runtime.lookupFunction(kBuildHeap)))
            runtime.replaceFunction(kBuildHeap, &buildHeap);
        else
            std::fprintf(stderr, "[memory] FUN_%08X not found: the sector packet heap stays the game's\n", kBuildHeap);
        // memstate -> ok streaming=<base>,<size>,<in use> main=<size>,<in use> packets=<base>,<size>,<in use>
        //             streaming_mb=<n> packets_moved=<0|1> cap_skips=<n> (bytes; heap +0xC first block, +0 size, +0x14 used)
        ps2control::addCommand("memstate", [](const std::string &, ps2control::Reply reply) {
            const uint8_t *rdram = g_rdram.load(std::memory_order_relaxed);
            if (!rdram)
                return reply("error the heaps are not built yet");
            char text[320];
            std::snprintf(text, sizeof(text),
                          "ok streaming=0x%08x,%u,%u main=%u,%u packets=0x%08x,%u,%u streaming_mb=%d packets_moved=%d "
                          "cap_skips=%llu",
                          rd32(rdram, kStreamingHeap + 0xCu), rd32(rdram, kStreamingHeap), rd32(rdram, kStreamingHeap + 0x14u),
                          rd32(rdram, kMainHeap), rd32(rdram, kMainHeap + 0x14u), rd32(rdram, kPacketHeap + 0xCu),
                          rd32(rdram, kPacketHeap), rd32(rdram, kPacketHeap + 0x14u), streamingMegabytes(),
                          packetHeapMoved() ? 1 : 0, static_cast<unsigned long long>(g_capSkips.load()));
            reply(text);
        });
    }
}
