// VCS display settings applied to the game (docs/SETTINGS_MENU_WIDESCREEN.md).
//
// Widescreen is Hor+: FUN_001ab9e0 (sets the camera's view window: x = tan(FOV / 2), y = x / aspect, then
// RwCameraSetViewWindow) is wrapped so the horizontal extent grows by r = display aspect / (4:3) while the vertical
// one stays (both arguments times r). RenderWare derives the frustum (EE culling) and the VU matrices from the view
// window, so the 3D, the sector culling and both renderers follow; the picture (still 640 wide) is presented at the
// wider aspect and the renderer keeps the HUD at 4:3 (gsHwSetHudSqueeze).
//
// Entities are culled by the game's own camera planes, which do not come from the view window:
// CCamera::IsSphereVisible (FUN_003816F8) tests camera-space side planes that FUN_0037b2c8 builds from the FOV global
// alone (right (cos h, -sin h) at cam+0xB60, left (-cos h, -sin h) at +0xB70, h = FOV / 2: the 4:3 view). Without a
// fix buildings, peds and cars at the screen edges vanish in widescreen. The test is wrapped: before it runs, side
// planes the game has rebuilt are widened to the Hor+ view (edge at tan h x r). VCS_WIDE_CULL=0: the game's planes.
// The bottom / top planes (+0xB80 / +0xB90, (0, -sin v, -+cos v)) use v = 0.7 x h, which ends inside the picture at
// any aspect (tan v 0.456 against the view window's 0.525 at FOV 70): objects in the top and bottom eighths of the
// picture - a dish on a roof, a lamp's head - came and went as the camera tilted (the same planes as on the PS2: they
// come from the FOV alone). They are moved to the view window's vertical edge as well (never narrowed); its ratio to
// tan(FOV / 2) comes from the per-frame view window call (the resize-time 16:9 default only lowers it for a frame, and
// a lower one keeps the game's planes). VCS_VERT_CULL=0: the game's vertical planes.
//
// The game's own widescreen preference (pause menu Display, option 14; *(*0x48F3D0 + 0x30)) is the PS2's anamorphic
// mode for a TV that stretches the picture: a Vert- 16:9 aspect, the HUD, text, pause map and menu layouts squeezed to
// 3/4 width, and no cutscene letterbox (game-internals.md 5a). On a PC every part of it but the bars fights the Hor+
// view and the renderer's HUD squeeze, so it is kept off (the user's decision, DECISIONS.md 2026-11-06): its setter
// FUN_002AF660 stores 0 - the pause menu's row is hidden (vcs_frontend.cpp) and a save's 1 (block +0x80, restored
// at 0x3AD0F8) is not taken - and the frame kick puts 0 back if anything wrote the field directly. The bars are a
// setting of their own (Display > Cutscene black bars): FUN_002804C8, which draws only them, is skipped when off.
// VCS_WIDESCREEN=1 forces the PS2's mode on (comparisons), VCS_WIDESCREEN=game leaves it to the game (before).
#include "game/vcs_display.h"

#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "runtime/gs/gs_hw.h"
#include <ps2_recompiled_functions.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vcs
{
    namespace
    {
        constexpr uint32_t kPrefsPointer = 0x0048F3D0u; // -> the game's preferences / menu object
        constexpr uint32_t kWidescreenField = 0x30u;    // int: 1 = the game's (Vert-) widescreen; FUN_002af658 reads it
        constexpr uint32_t kRamMask = PS2_RAM_MASK;

        constexpr uint32_t kRightPlane = 0xB60u; // CCamera: camera-space frustum normals, x right, y forward, z up
        constexpr uint32_t kLeftPlane = 0xB70u;
        constexpr uint32_t kBottomPlane = 0xB80u; // (0, -sin v, -cos v): y at +4, z at +8
        constexpr uint32_t kTopPlane = 0xB90u;    // (0, -sin v, +cos v)

        std::atomic<float> g_widening{1.0f};
        std::atomic<int> g_gamePreference{0}; // the value the game's preference is kept at; -1: left to the game
        std::atomic<bool> g_cutsceneBars{true};
        bool g_wideCull = true;
        bool g_vertCull = true;
        // The view window's vertical extent over tan(FOV / 2) (the planes' tan h): 1 / aspect, 3 / (4 r) in PS2 cutscene
        // framing; set by each view window call (setViewWindow), 0 before the first.
        std::atomic<float> g_vertRatio{0.0f};

        // Per camera object, its planes as the game built them and as they were moved (game thread only: the camera's
        // FUN_0037b2c8 and every IsSphereVisible run there). In practice one camera (TheCamera, 0x6F44D0).
        constexpr uint32_t kPlaneFloats[8] = {kRightPlane, kRightPlane + 4, kLeftPlane, kLeftPlane + 4,
                                              kBottomPlane + 4, kBottomPlane + 8, kTopPlane + 4, kTopPlane + 8};
        struct SidePlanes
        {
            uint32_t camera = 0;
            float r = 1.0f;
            float vert = 0.0f;  // the vertical ratio they were moved with (0: the game's)
            float game[8] = {}; // right x, y, left x, y, bottom y, z, top y, z
            float wide[8] = {};
        };
        SidePlanes g_planes[4];
        unsigned g_nextPlanes = 0;

        uint32_t load32(const uint8_t *rdram, uint32_t addr)
        {
            uint32_t v;
            std::memcpy(&v, rdram + (addr & kRamMask), 4);
            return v;
        }

        void store32(uint8_t *rdram, uint32_t addr, uint32_t v) { std::memcpy(rdram + (addr & kRamMask), &v, 4); }

        // [Minty1804 fork] Cutscene framing (Display > Screen), after willzmkv's (dejot) 477cfd0. While the camera's
        // cinematic widescreen is on (m_WideScreenOn, TheCamera + 0x844: cutscenes and the scripts' SWITCH_WIDESCREEN;
        // the game's aspect then 1.25), the PS2 showed the 4:3 width with bars over the top and bottom, and scenes keep
        // actors just outside that width until they walk in: Hor+ showed them waiting at the edges (Soldier: Vic at the
        // right of the jet from the shot's first frame). PS2 framing keeps the 4:3 width and takes the height from the
        // display aspect (Vert-: a widescreen picture of what the bars left, square pixels), with no bars over it.
        // Only where the PS2 drew the bars: a .CUT cutscene (the cutscene manager *0x48F820 with +0x34 set, what
        // FUN_002804C8 tests; read from its caller at 0x21F400). A script's SWITCH_WIDESCREEN alone sets the flag too but
        // has no bars (game-internals.md 5a): the PS2 showed the whole picture there, which the Hor+ view keeps (ours;
        // willzmkv's went by the flag alone, and cut the top and bottom of those scenes).
        constexpr uint32_t kCinematic = 0x006F4D14u;
        constexpr uint32_t kCutsceneManager = 0x0048F820u;
        std::atomic<bool> g_cutsceneFraming{true};
        bool ps2Framing(const uint8_t *rdram, float r)
        {
            if (r <= 1.0001f || !g_cutsceneFraming.load(std::memory_order_relaxed) || load32(rdram, kCinematic) == 0)
                return false;
            const uint32_t manager = load32(rdram, kCutsceneManager);
            return manager >= 0x00100000u && manager < PS2_RAM_SIZE && load32(rdram, manager + 0x34u) != 0;
        }

        // FUN_001ab9e0(camera a0, raster rect a1 (null: 640x448), tan(FOV / 2) f12, aspect f13)
        PS2_GUEST_CC void setViewWindow(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const float r = g_widening.load(std::memory_order_relaxed);
            const float tanHalf = ctx->f[12];
            if (ps2Framing(rdram, r))
                ctx->f[13] = (4.0f / 3.0f) * r; // the 4:3 width (f12 as it is), the display's height
            else if (r > 1.0001f || r < 0.9999f)
            {
                ctx->f[12] *= r; // horizontal extent x r
                ctx->f[13] *= r; // vertical extent (x / aspect) unchanged
            }
            if (ctx->f[12] > 0.0f && ctx->f[13] > 0.0f && tanHalf > 0.0f)
                g_vertRatio.store(ctx->f[12] / ctx->f[13] / tanHalf, std::memory_order_relaxed);
            FUN_001ab9e0_0x1ab9e0(rdram, ctx, runtime);
        }

        float loadFloat(const uint8_t *rdram, uint32_t addr)
        {
            float v;
            std::memcpy(&v, rdram + (addr & kRamMask), 4);
            return v;
        }

        void storeFloat(uint8_t *rdram, uint32_t addr, float v) { std::memcpy(rdram + (addr & kRamMask), &v, 4); }

        // A side plane (a, b) = (+-cos h, -sin h) has its edge at x / y = tan h; the Hor+ edge is at tan h x r:
        // (a, r b), normalised (the test compares the distance with the sphere's radius).
        void widenPlane(const float *game, float r, float *wide)
        {
            const float a = game[0], b = game[1] * r;
            const float len = std::sqrt(a * a + b * b);
            wide[0] = len > 0.0f ? a / len : game[0];
            wide[1] = len > 0.0f ? b / len : game[1];
        }

        // A vertical plane (y, z) = (-sin v, +-cos v) has its edge at |z| / y = tan v; moved to tan h x ratio (h from
        // the game's right plane (cos h, -sin h)), never inside the game's edge: (-t, +-1), normalised.
        void openPlane(const float *game, float tanH, float ratio, float *wide)
        {
            const float c = std::fabs(game[1]);
            const float t = std::fmax(tanH * ratio, c > 0.0f ? -game[0] / c : 0.0f);
            const float len = std::sqrt(t * t + 1.0f);
            wide[0] = -t / len;
            wide[1] = (game[1] < 0.0f ? -1.0f : 1.0f) / len;
        }
    }

    void prepareSphereTest(uint8_t *rdram, uint32_t camera)
    {
        if (!g_wideCull && !g_vertCull)
            return;
        // (PS2 cutscene framing: the 4:3 width, the game's own side planes)
        const float widening = g_widening.load(std::memory_order_relaxed);
        const float r = !g_wideCull || ps2Framing(rdram, widening) ? 1.0f : widening;
        const float vert = g_vertCull ? g_vertRatio.load(std::memory_order_relaxed) : 0.0f;
        SidePlanes *found = nullptr;
        for (SidePlanes &e : g_planes)
            if (e.camera == camera)
                found = &e;
        if (r == 1.0f && vert == 0.0f && (!found || (found->r == 1.0f && found->vert == 0.0f)))
            return; // the game's planes
        float now[8];
        for (int i = 0; i < 8; ++i)
            now[i] = loadFloat(rdram, camera + kPlaneFloats[i]);
        const bool ours = found && std::memcmp(now, found->wide, sizeof(now)) == 0;
        if (ours && found->r == r && found->vert == vert)
            return; // moved already (the game rebuilds them in FUN_0037b2c8, once per camera update)
        SidePlanes &p = found ? *found : g_planes[g_nextPlanes++ % 4u];
        if (!ours)
            std::memcpy(p.game, now, sizeof(now)); // rebuilt by the game: its planes
        // else: the ratios changed since they were moved (r = 1 / vert = 0: the game's planes go back)
        p.camera = camera;
        p.r = r;
        p.vert = vert;
        std::memcpy(p.wide, p.game, sizeof(p.wide));
        if (r != 1.0f)
        {
            widenPlane(p.game, r, p.wide);
            widenPlane(p.game + 2, r, p.wide + 2);
        }
        if (vert > 0.0f && p.game[0] > 0.0f)
        {
            const float tanH = -p.game[1] / p.game[0]; // the game's right plane: the 4:3 half width
            openPlane(p.game + 4, tanH, vert, p.wide + 4);
            openPlane(p.game + 6, tanH, vert, p.wide + 6);
        }
        for (int i = 0; i < 8; ++i)
            storeFloat(rdram, camera + kPlaneFloats[i], p.wide[i]);
    }

    namespace
    {
        // CCamera::IsSphereVisible(camera a0, centre a1, camera matrix a2, radius f12): a leaf
        PS2_GUEST_CC void sphereVisible(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            prepareSphereTest(rdram, GPR_U32(ctx, 4));
            FUN_003816f8_0x3816f8(rdram, ctx, runtime);
        }

        // FUN_002AF660(prefs a0, value a1): the widescreen preference's setter (a leaf: sw a1, 0x30(a0)). Callers: the
        // pause menu's Display page (0x2AF06C) and the save load (0x3AD0F8).
        PS2_GUEST_CC void setWidescreenPreference(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const int wanted = g_gamePreference.load(std::memory_order_relaxed);
            if (wanted >= 0 && static_cast<int>(GPR_U32(ctx, 5)) != wanted)
            {
                static bool logged = false;
                if (!logged)
                    std::fprintf(stderr, "[display] the game sets its widescreen preference %u (ra=0x%x): kept at %d\n",
                                 GPR_U32(ctx, 5), GPR_U32(ctx, 31), wanted);
                logged = true;
                SET_GPR_U32(ctx, 5, static_cast<uint32_t>(wanted));
            }
            FUN_002af660_0x2af660(rdram, ctx, runtime);
        }

        // FUN_002804C8(cutscene manager a0): the cutscene letterbox - two black bars, 0..34 and 238..272 of the 2D pass's
        // 272, while the manager's +0x34 is set and the widescreen preference is off. Draws nothing else.
        PS2_GUEST_CC void cutsceneBars(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            // (PS2 cutscene framing shows what the bars left: none over it)
            if (!g_cutsceneBars.load(std::memory_order_relaxed) || ps2Framing(rdram, g_widening.load(std::memory_order_relaxed)))
            {
                ctx->pc = GPR_U32(ctx, 31);
                return;
            }
            FUN_002804c8_0x2804c8(rdram, ctx, runtime);
        }
    }

    void setCutsceneBars(bool on) { g_cutsceneBars.store(on, std::memory_order_relaxed); }

    void setCutsceneFraming(bool ps2) { g_cutsceneFraming.store(ps2, std::memory_order_relaxed); }

    void setAspectWidening(float r) { g_widening.store(r, std::memory_order_relaxed); }

    float aspectWidening() { return g_widening.load(std::memory_order_relaxed); }

    // The reflection map is the 128x128 copy of the frame into FBP 420 that car paint samples (game-internals.md 6g): a
    // flat panel shows one of its texels, so the GS's four taps out of ~5 x 3.5 frame pixels make the panel jump as the
    // camera moves. Smooth reads the copy through the renderer's filter (gsHwSetTargetFilter); the bloom at FBP 420 is
    // 256 wide (FBW 4), so only the reflection map's draws are filtered.
    void setReflectionFilter(int mode) { gsHwSetTargetFilter(420u, 2u, mode == 1 ? 1 : mode == 2 ? 2 : 0); }

    void installDisplayHooks(PS2Runtime &runtime)
    {
        runtime.replaceFunction(0x001AB9E0u, &setViewWindow);
        if (const char *e = std::getenv("VCS_WIDE_CULL"); e && *e == '0')
            g_wideCull = false;
        if (const char *e = std::getenv("VCS_VERT_CULL"); e && *e == '0')
            g_vertCull = false;
        runtime.replaceFunction(0x003816F8u, &sphereVisible); // VCS_TRACE_VISIBLE replaces it again, calling prepareSphereTest
        if (const char *e = std::getenv("VCS_WIDESCREEN"); e && *e) // testing: the PS2's own (Vert-, anamorphic) mode
            g_gamePreference.store(std::strcmp(e, "game") == 0 ? -1 : *e == '0' ? 0 : 1, std::memory_order_relaxed);
        if (g_gamePreference.load(std::memory_order_relaxed) >= 0)
            runtime.replaceFunction(0x002AF660u, &setWidescreenPreference);
        runtime.replaceFunction(0x002804C8u, &cutsceneBars);
        // VCS's render targets by what they are (game-internals.md 6g; docs/FULLY_NATIVE.md step 3d). The reflection map
        // is only ever sampled (vehicles' paint), so it follows "Internal resolution: Match window" like the picture.
        gsHwSetTargetRole(140u, 10u, "frame", false);
        gsHwSetTargetRole(0u, 10u, "shown picture (the previous frame)", false);
        gsHwSetTargetRole(280u, 10u, "Z buffer", false);
        gsHwSetTargetRole(420u, 2u, "reflection map (128x128 sample of the frame + sun glow)", true);
        gsHwSetTargetRole(420u, 4u, "bloom", false);
        gsHwSetTargetRole(452u, 4u, "bloom (second buffer)", false);
        gsHwSetTargetRole(384u, 8u, "trails", false);
        // The flip (FUN_003706c8) reads GS CSR at 0x370714 for the FIELD bit alone ($s4 = (CSR >> 13) & 1, the rest of
        // the value is overwritten in the next delay slot). FIELD is set at VBlank on the game thread: no need to wait
        // for the render worker to finish the frame's GS work (that fence was ~25% of the game thread uncapped,
        // game-internals.md 6). VCS_CSR_FENCE=1: fence as before.
        if (!std::getenv("VCS_CSR_FENCE"))
            runtime.addCsrReadWithoutFence(0x00370714u);
    }

    void applyDisplay(uint8_t *rdram)
    {
        const int wanted = g_gamePreference.load(std::memory_order_relaxed);
        if (wanted < 0)
            return;
        const uint32_t prefs = load32(rdram, kPrefsPointer);
        if (prefs == 0u || (prefs & kRamMask) + kWidescreenField + 4u > PS2_RAM_SIZE)
            return; // the game has not created its preferences yet
        if (static_cast<int>(load32(rdram, prefs + kWidescreenField)) != wanted)
            store32(rdram, prefs + kWidescreenField, static_cast<uint32_t>(wanted)); // FUN_002653f0 picks the aspect up
    }
}
