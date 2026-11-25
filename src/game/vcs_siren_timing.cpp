// The horn's history on real time above 30 fps (willzmkv, gothamor#209; docs/game-internals.md 4, "The horn's history").
//
// CAutomobile::ProcessControl (0x0013A0A0, at 0x13E5EC) and CBike::ProcessControl (0x00337208, at 0x33BF9C) read CPad 0's
// ring of 8 horn samples (pad+0x9A..0xA1, its index at pad+0xA2; CPad::Update 0x002843D8 writes one a frame) for a
// vehicle that has a siren (CVehicle::UsesSiren 0x003E9768), at three slots: cur, prev = (cur + 7) & 7, old = (cur + 1) & 7.
// - let go (hist[cur] == 0, prev != 0) with old == 0 - held under 7 frames, ~233 ms at 30 fps -: veh+0x268 bit 0x200000
//   (m_bSirenOrAlarm) toggles, the lights and siren on or off;
// - held (cur != 0) with old != 0 - 7 frames or more -: the continuous horn (veh+0x2B4 = 1).
// At 144 fps 7 frames are 49 ms: no tap is that short, so the lights never came on (Paramedic, Firefighter, Vigilante).
// Here, after CPad::Update(pad 0), the ring is rewritten from the press as timed on CTimer's unscaled ms clock: the same
// three-slot pattern the game reads, the hold threshold at the PS2's ~230 ms. Hooks work before the original (AGENTS
// rule 2): CPad::Update notes the index, CPad::Clear - UpdatePads' next call, told apart by $ra - rewrites the ring.
#include "game/vcs_siren_timing.h"

#include "game/vcs_framerate.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vcs
{
    namespace
    {
        constexpr uint32_t kRealClock = 0x00488280u;         // CTimer: ms, unscaled, also while paused
        constexpr uint32_t kPad0 = 0x005147A8u;              // CPad 0
        constexpr uint32_t kHornHistory = 0x9Au;             // m_bHornHistory[8]
        constexpr uint32_t kHornCurrent = 0xA2u;             // m_nCurrentHornHistory (0..7)
        constexpr uint32_t kPadUpdate = 0x002843D8u;         // CPad::Update(pad, index); only caller CPad::UpdatePads
        constexpr uint32_t kPadClear = 0x00283788u;          // CPad::Clear
        constexpr uint32_t kAfterPad0Update = 0x002843BCu;   // $ra of UpdatePads' call to Clear after Update(pad 0, 0)
        constexpr double kPs2FrameMs = 1000.0 * 1001.0 / 30000.0;
        constexpr double kHoldMs = 6.9 * kPs2FrameMs; // 7 PS2 frames: 231-234 ms at 30 fps with the clock's truncation

        PS2Runtime::RecompiledFunction g_origPadUpdate = nullptr, g_origPadClear = nullptr;

        // game thread
        struct
        {
            bool started = false, down = false, longHold = false;
            uint8_t indexBefore = 0;
            uint32_t lastMs = 0;
            double downMs = 0.0;
        } g_horn;

        bool logOn()
        {
            static const bool on = [] { const char *e = std::getenv("VCS_SIREN_LOG"); return e && e[0] == '1'; }();
            return on;
        }

        // at 30 fps with the PS2's timing the game's frames are the PS2's: nothing to do
        bool active() { return timing() == Timing::Independent || frameCap() != 30; }

        PS2_GUEST_CC void padUpdate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if ((GPR_U32(ctx, 4) & PS2_RAM_MASK) == kPad0)
                g_horn.indexBefore = rdram[kPad0 + kHornCurrent];
            g_origPadUpdate(rdram, ctx, runtime);
        }

        PS2_GUEST_CC void padClear(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (GPR_U32(ctx, 31) == kAfterPad0Update && active())
            {
                const uint8_t index = rdram[kPad0 + kHornCurrent];
                const uint8_t cur = index & 7u, prev = (cur + 7u) & 7u, old = (cur + 1u) & 7u;
                uint8_t *hist = rdram + kPad0 + kHornHistory;
                // the sample Update just wrote (none when it returned before advancing the ring)
                const bool horn = index != g_horn.indexBefore && hist[cur] != 0;

                uint32_t nowMs;
                std::memcpy(&nowMs, rdram + kRealClock, 4);
                double dtMs = 0.0;
                if (g_horn.started && nowMs - g_horn.lastMs <= 500u) // a pause or a load between: no time held
                    dtMs = static_cast<double>(nowMs - g_horn.lastMs);
                g_horn.started = true;
                g_horn.lastMs = nowMs;

                std::memset(hist, 0, 8);
                if (horn)
                {
                    const bool wasDown = g_horn.down;
                    if (!wasDown)
                    {
                        g_horn.down = true;
                        g_horn.longHold = false;
                        g_horn.downMs = 0.0;
                        if (logOn())
                            std::fprintf(stderr, "[siren] horn pressed (ms %u)\n", nowMs);
                    }
                    else if (!g_horn.longHold && (g_horn.downMs += dtMs) >= kHoldMs)
                    {
                        g_horn.longHold = true;
                        if (logOn())
                            std::fprintf(stderr, "[siren] horn held %.1f ms: the continuous horn\n", g_horn.downMs);
                    }
                    hist[cur] = 1;
                    if (wasDown)
                        hist[prev] = 1;
                    if (g_horn.longHold)
                        hist[old] = 1;
                }
                else if (g_horn.down)
                {
                    hist[prev] = 1; // the release the game looks for
                    if (g_horn.longHold)
                        hist[old] = 1;
                    if (logOn())
                        std::fprintf(stderr, "[siren] horn let go after %.1f ms: %s\n", g_horn.downMs,
                                     g_horn.longHold ? "the horn stops (no toggle)" : "lights / siren toggled");
                    g_horn.down = g_horn.longHold = false;
                    g_horn.downMs = 0.0;
                }
            }
            g_origPadClear(rdram, ctx, runtime);
        }
    }

    void installSirenTimingHooks(PS2Runtime &runtime)
    {
        if (const char *e = std::getenv("VCS_SIREN_TIMING"); e && e[0] == '0')
        {
            std::fprintf(stderr, "[siren-timing] off (VCS_SIREN_TIMING=0): the horn's history per frame\n");
            return;
        }
        g_origPadUpdate = runtime.lookupFunction(kPadUpdate);
        g_origPadClear = runtime.lookupFunction(kPadClear);
        if (!g_origPadUpdate || !g_origPadClear)
        {
            std::fprintf(stderr, "[siren-timing] 0x%x / 0x%x not found: the horn's history per frame\n", kPadUpdate,
                         kPadClear);
            return;
        }
        runtime.replaceFunction(kPadUpdate, &padUpdate);
        runtime.replaceFunction(kPadClear, &padClear);
    }
}
