// See vcs_footcam.h and game-internals.md 12b ("On foot: the stick, the camera on its string, walls").
#include "game/vcs_footcam.h"

#include "game/vcs_aim.h"
#include "game/vcs_input.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "runtime/ps2_control.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace vcs::footcam
{
    namespace
    {
        constexpr uint32_t kRamMask = PS2_RAM_MASK;
        constexpr uint32_t kFollowPed = 0x0029DAB0u; // the follow camera on foot (mode 4), a0 the CCam
        constexpr uint32_t kTheCamera = 0x006F44D0u;
        constexpr uint32_t kActiveCam = kTheCamera + 0x50u; // byte: which of Cams[3] is active
        constexpr uint32_t kCams = kTheCamera + 0x70u, kCamSize = 0x270u;
        constexpr uint32_t kLooksAtPlayer = kTheCamera + 0x888u; // 0 under a script camera
        constexpr uint32_t kBehindHim = 0x006F4528u, kInFront = 0x006F4D28u; // the update starts afresh while set
        constexpr uint32_t kCamFirstUpdate = 0xACu;
        constexpr uint32_t kCamPitchSpeed = 0x130u, kCamHeadingSpeed = 0x140u;
        constexpr uint32_t kCamPoints[2] = {0x220u, 0x230u}; // where it stands: the next update measures from these
        constexpr uint32_t kModeFollowPed = 4;
        // the swing behind a walking player: a turn capped at table +0x2C a step (outdoors / indoors; the table has no
        // other reader, so 0 there switches every automatic turn off), the turn away from a wall he walks along
        // (added to beta every frame), and the latch of the game's own turn behind him (L1, some guns from the hip)
        constexpr uint32_t kSwingCaps[2] = {0x00487B5Cu, 0x00487B98u};
        constexpr uint32_t kWallTurn = 0x00487E54u;
        constexpr uint32_t kTurnBehind = 0x00487E10u;
        constexpr uint32_t kPullIn = 0x00487E58u; // > 0: the update takes the stick's path at walls (the pull-in)
        constexpr uint32_t kPlayerPed = 0x004E4910u;
        constexpr uint32_t kPedPos = 0x30u;
        constexpr uint32_t kFrameCounter = 0x004CD124u;
        constexpr uint32_t kGameClock = 0x004CD104u; // CTimer::m_snTimeInMilliseconds

        uint8_t *g_rdram = nullptr;
        PS2Runtime::RecompiledFunction g_orig = nullptr;
        std::atomic<int> g_hold{2000};

        // game thread
        bool g_turned = false, g_stick = false;   // since the last update
        bool g_holding = false, g_capsOff = false;
        float g_caps[2] = {};
        uint32_t g_lastTurn = 0;                  // game time of the mouse's last turn
        bool g_everTurned = false;
        uint32_t g_ped = 0, g_frame = 0;
        float g_lastPos[3] = {};
        std::atomic<int> g_shown{0};

        uint32_t rd32(uint32_t a)
        {
            uint32_t v;
            std::memcpy(&v, g_rdram + (a & kRamMask), 4);
            return v;
        }
        float rdf(uint32_t a)
        {
            float v;
            std::memcpy(&v, g_rdram + (a & kRamMask), 4);
            return v;
        }
        void wrf(uint32_t a, float v) { std::memcpy(g_rdram + (a & kRamMask), &v, 4); }

        void capsOff(bool off)
        {
            if (off == g_capsOff)
                return;
            for (int i = 0; i < 2; ++i)
            {
                if (off)
                    g_caps[i] = rdf(kSwingCaps[i]);
                wrf(kSwingCaps[i], off ? 0.0f : g_caps[i]);
            }
            g_capsOff = off;
        }

        void release()
        {
            capsOff(false);
            g_holding = false;
            g_shown.store(0, std::memory_order_relaxed);
        }

        // Before the game's update of the follow camera (he has moved this frame).
        void prepare(uint32_t cam)
        {
            const bool turned = g_turned, stick = g_stick;
            g_turned = g_stick = false;
            const uint32_t ped = rd32(kPlayerPed);
            const uint32_t frame = rd32(kFrameCounter), clock = rd32(kGameClock);
            const bool gap = frame - g_frame > 1;
            g_frame = frame;
            float pos[3] = {}, moved[3] = {};
            if (ped)
                for (int i = 0; i < 3; ++i)
                {
                    pos[i] = rdf(ped + kPedPos + i * 4);
                    moved[i] = pos[i] - g_lastPos[i];
                    g_lastPos[i] = pos[i];
                }
            const bool sameFrameRun = !gap && ped == g_ped;
            g_ped = ped;

            if (turned)
            {
                g_lastTurn = clock;
                g_everTurned = true;
            }
            const int hold = g_hold.load(std::memory_order_relaxed);
            const bool mouse = input::lastDevice() == input::Device::Keyboard;
            bool want = hold != 0 && mouse &&
                        (hold < 0 || (g_everTurned && clock - g_lastTurn <= static_cast<uint32_t>(hold)));
            // the game's camera: another camera, a script's, a first update, the stick, the game turning it behind him,
            // free aim, a jump (a teleport, a new ped)
            const bool usable = cam == kCams + g_rdram[kActiveCam] * kCamSize && (rd32(cam) & 0xFFFFu) == kModeFollowPed &&
                                ped && rd32(kLooksAtPlayer) == 1 && rd32(cam + kCamFirstUpdate) == 0 && rd32(kBehindHim) == 0 &&
                                rd32(kInFront) == 0 && rd32(kTurnBehind) == 0 && !aim::aiming();
            const float jump = std::sqrt(moved[0] * moved[0] + moved[1] * moved[1] + moved[2] * moved[2]);
            if (!usable || stick || !sameFrameRun || jump > 5.0f)
            {
                if (stick)
                    g_everTurned = false; // the stick has it now: a hold starts again at the mouse's next turn
                want = false;
            }
            if (!want)
            {
                if (g_holding)
                    release();
                return;
            }

            // held: the points it stands at move with him (no string), no swing behind him or away from a wall, and
            // the speeds left from before stop - the camera stays where the mouse puts it
            g_holding = true;
            g_shown.store(1, std::memory_order_relaxed);
            capsOff(true);
            for (uint32_t off : kCamPoints)
                for (int i = 0; i < 3; ++i)
                    wrf(cam + off + i * 4, rdf(cam + off + i * 4) + moved[i]);
            wrf(cam + kCamHeadingSpeed, 0.0f);
            wrf(cam + kCamPitchSpeed, 0.0f);
            wrf(kWallTurn, 0.0f);
            // walls: with no turn away from them, the stick's path (the pull-in toward him) for the whole hold, as the
            // mouse's own turn asks for it (vcs_input.cpp, 300 ms after a turn)
            if (rd32(kPullIn) == 0)
            {
                const uint32_t one = 1;
                std::memcpy(g_rdram + kPullIn, &one, 4);
            }
        }

        // FUN_0029DAB0 (CCam::Process's case for mode 4): the hold's say first, then the game's update.
        PS2_GUEST_CC void followPed(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            prepare(GPR_U32(ctx, 4));
            g_orig(rdram, ctx, runtime);
        }
    }

    void install(PS2Runtime &runtime)
    {
        g_rdram = runtime.memory().getRDRAM();
        if (const char *e = std::getenv("VCS_FOOTCAM"); e && e[0] == '0')
        {
            std::fprintf(stderr, "[footcam] off (VCS_FOOTCAM=0)\n");
            return;
        }
        g_orig = runtime.lookupFunction(kFollowPed);
        if (!g_orig)
            return;
        runtime.replaceFunction(kFollowPed, &followPed);
        // footcam -> ok state=<game|hold> (tests)
        ps2control::addCommand("footcam", [](const std::string &, ps2control::Reply reply) {
            reply(g_shown.load(std::memory_order_relaxed) ? "ok state=hold" : "ok state=game");
        });
    }

    void mouseTurned() { g_turned = true; }

    void stickMoved() { g_stick = true; }

    void setHold(int ms) { g_hold.store(ms, std::memory_order_relaxed); }
}
