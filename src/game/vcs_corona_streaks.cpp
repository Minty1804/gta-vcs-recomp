// The headlights' streaks hidden behind walls and peds (the user's reports of 2026-11-20 "car lights through my
// character" and 2026-11-24 "through the walls and Vic"; docs/game-internals.md, "Headlight streaks over the player").
//
// The streaks are CCoronas::Render's (FUN_0027C418): each registered corona (56 of 0x90 bytes at 0x00512560) keeps its
// last 6 screen points (x s16 at +0x4A, y at +0x56, r / g / b at +0x62 / +0x68 / +0x6E, valid u32 at +0x74), shifted
// every frame; with the streak flag (+0x48 bit 3) a line is drawn between two valid points (FUN_002FBD78, 2 vertices),
// with no depth test - so the PS2 draws them over everything. A point is recorded only while +0x48 bit 4 is set, and
// that bit is a line-of-sight test from the corona to the camera's source (CWorld::GetIsLineOfSightClear FUN_0018FC88,
// at 0x27C660: buildings and vehicles, see-through ignored, no peds, no objects), redone only when 2 s of game time have
// passed since +0x24 (0x27C5F8). So a car behind a wall kept its streaks for up to 2 s, and Vic never hid them.
// Here, before Render (AGENTS rule 2), each streak corona's +0x24 is set 2001 ms back once every 33 ms of game time (a
// PS2 frame: the cost stays that of 30 tests a second per corona at any frame rate; ~6 streak coronas in a night drive
// through traffic, the game thread's time within the run-to-run noise), so the test runs every PS2 frame, and that call (told by $ra 0x27C668) also counts peds (the sector walk FUN_001944B8: a2 buildings, a3 vehicles, t0
// peds, t1 objects, t2 dummies). The coronas themselves are depth tested on the GS and are unchanged.
#include "game/vcs_corona_streaks.h"

#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vcs
{
    namespace
    {
        constexpr uint32_t kCoronasRender = 0x0027C418u;  // CCoronas::Render
        constexpr uint32_t kLineOfSight = 0x0018FC88u;    // CWorld::GetIsLineOfSightClear
        constexpr uint32_t kStreakLosReturn = 0x0027C668u; // $ra of Render's streak test
        constexpr uint32_t kCoronas = 0x00512560u;         // CCoronas::aCoronas[56]
        constexpr uint32_t kCoronaCount = 56u, kCoronaSize = 0x90u;
        constexpr uint32_t kGameMs = 0x004CD104u;          // CTimer::m_snTimeInMilliseconds
        constexpr uint32_t kRetestAfterMs = 2000u;         // Render's wait between tests (0x27C600)
        constexpr uint32_t kOurRetestMs = 33u;             // ours: once a PS2 frame, whatever the frame rate

        PS2Runtime::RecompiledFunction g_origRender = nullptr, g_origLineOfSight = nullptr;
        uint32_t g_lastTest[kCoronaCount] = {}; // game ms of the test we last asked for, per corona (game thread)

        // VCS_STREAK_LOG=1: every 5 s of game time, the streak coronas seen by Render and how many of them passed the last
        // test (+0x48 bit 4) - a streak still drawn behind a wall shows up as a pass there
        bool logOn()
        {
            static const bool on = [] { const char *e = std::getenv("VCS_STREAK_LOG"); return e && e[0] == '1'; }();
            return on;
        }
        struct
        {
            uint32_t frames = 0, streaks = 0, clear = 0, lastMs = 0;
        } g_log;

        PS2_GUEST_CC void coronasRender(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            uint32_t now;
            std::memcpy(&now, rdram + kGameMs, 4);
            const uint32_t due = now - (kRetestAfterMs + 1u); // +0x24 + 2000 < now: tested this frame
            for (uint32_t i = 0; i < kCoronaCount; ++i)
            {
                uint8_t *c = rdram + kCoronas + i * kCoronaSize;
                uint32_t id;
                std::memcpy(&id, c + 0x20, 4);
                if (id == 0 || (c[0x48] & 0x08u) == 0)
                    continue;
                if (now - g_lastTest[i] >= kOurRetestMs || now < g_lastTest[i])
                {
                    std::memcpy(c + 0x24, &due, 4);
                    g_lastTest[i] = now;
                }
                if (logOn())
                {
                    ++g_log.streaks;
                    g_log.clear += (c[0x48] & 0x10u) != 0;
                }
            }
            if (logOn())
            {
                ++g_log.frames;
                if (now - g_log.lastMs >= 5000u)
                {
                    std::fprintf(stderr, "[streaks] %u frames: %.1f streak coronas a frame, %.1f in sight\n", g_log.frames,
                                 g_log.frames ? double(g_log.streaks) / g_log.frames : 0.0,
                                 g_log.frames ? double(g_log.clear) / g_log.frames : 0.0);
                    g_log = {};
                    g_log.lastMs = now;
                }
            }
            g_origRender(rdram, ctx, runtime);
        }

        PS2_GUEST_CC void lineOfSight(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (GPR_U32(ctx, 31) == kStreakLosReturn)
                SET_GPR_U32(ctx, 8, 1u); // t0: peds too
            g_origLineOfSight(rdram, ctx, runtime);
        }
    }

    void installCoronaStreakHooks(PS2Runtime &runtime)
    {
        if (const char *e = std::getenv("VCS_STREAK_LOS"); e && e[0] == '0')
        {
            std::fprintf(stderr, "[streaks] off (VCS_STREAK_LOS=0): the streaks' test every 2 s, no peds\n");
            return;
        }
        g_origRender = runtime.lookupFunction(kCoronasRender);
        g_origLineOfSight = runtime.lookupFunction(kLineOfSight);
        if (!g_origRender || !g_origLineOfSight)
        {
            std::fprintf(stderr, "[streaks] 0x%x / 0x%x not found: the streaks' test every 2 s\n", kCoronasRender,
                         kLineOfSight);
            return;
        }
        runtime.replaceFunction(kCoronasRender, &coronasRender);
        runtime.replaceFunction(kLineOfSight, &lineOfSight);
    }
}
