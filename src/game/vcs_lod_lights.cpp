// Distant lights: Project2DFX's LOD lights drawn by the host renderer (see game/vcs_lod_lights.h, game-internals.md 7c).
//
// Each VIF1 chain (one frame) the first world packet's VU1 memory gives the frame's camera exactly as the world is drawn
// with it: qwords 16-19 view x projection from render space to clip space, 12-15 clip space to GS window coordinates
// (the viewport, Hor+ included), the same two the setup program folds into 24-31 for every model (vcs_hle_world.cpp
// setupMatrices). Render space is the world moved by an origin, the centre of a map cell (game-internals.md 7c): the cell
// centre nearest to TheCamera's position at the chain's kick minus the camera in render space, so the lights are moved by
// it first. They are projected on the GS stage, in stream order, and handed to the renderer (gsHwSetOverlay), which puts
// them after the game's coronas, depth-tested against the world at the buildings' depth layer.
// Cost: ~20-60 us a frame on the GS stage (a grid of the lights by place, a sine table, no allocation a frame).
//
// The rules are Project2DFX's (source/GTAVCS.PCSX2F.Project2DFX/lodl.c, RenderLODLightsBuffered, and its PCSX2 ini):
// - 19:00 - 7:00 only, the strength rising from 30 / 255 at 19:00 to 255 at midnight, full until 3:00, down to 30 at 7:00;
// - a light shows from a third of its own corona distance (farclip) out to 1000 m, its radius growing from 0 there to
//   3.5 at farclip (x the size factor, x 1.5), and with distance (x 0.0025 d + 0.25, at most x 4);
// - blinking lights (show mode 1-7) on / off by the game's ms clock; traffic lights (size 0.45) by the minute.
// VCS_LODLIGHTS_FARCLIP / VCS_LODLIGHTS_RADIUS / VCS_LODLIGHTS_INTENSITY change the 1000 m, the 1.5 and the strength;
// VCS_LODLIGHTS_LOG=1 prints what was drawn every 5 s (and the camera, to check the projection), and how the origin was
// found; =2 the cameras and depth layers of a chain's world packets. VCS_LODLIGHTS_ORIGIN / _DEPTH / _PULL: see onMscal and
// drawLights.
#include "game/vcs_lod_lights.h"

#include "ps2_runtime.h"
#include "runtime/gs/gs_hw.h"
#include "runtime/ps2_chain_observer.h"
#include "runtime/ps2_render_worker.h"
#include "settings/settings.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace vcs::lodlights
{
    namespace
    {
        constexpr uint32_t kClockHours = 0x004CD148u; // CClock: hours, minutes (bytes; game-internals.md 20)
        constexpr uint32_t kCurrArea = 0x00489F7Cu;   // CGame::currArea: 0 (and 4) outdoors - the far clip's own test
        constexpr uint32_t kTimeMs = 0x004CD104u;     // CTimer::m_snTimeInMilliseconds (stands still while paused)
        // The world is drawn relative to an origin (VU1 qwords 16-19 and the object matrices; qword 32 is the camera
        // from it): the centre of the current map cell, its z the camera's rounded to 32 m, stored negated at 0x6FE980 by
        // FUN_0023E328 when the cell changes (game-internals.md 7c). TheCamera + 0x1170 holds a copy that is not always
        // updated (a teleport leaves it behind), and its z is not there.
        constexpr uint32_t kCellOrigin = 0x006FE980u;
        constexpr uint32_t kOldOrigin = 0x006F5640u; // (VCS_LODLIGHTS_LOG: compared)
        constexpr uint32_t kCameraPos = 0x006F4500u; // TheCamera (0x6F44D0) + 0x30: the camera in the world
        constexpr float kPi = 3.14159265f;

        struct Light
        {
            float x, y, z, size, farClip, heading;
            uint8_t r, g, b, a;
            int show;
            bool noDistance;
        };

        std::atomic<int> g_enabled{0};
        std::atomic<uint32_t> g_origin[3]{}; // the origin at the last game frame (floats): a chain not seen queued
        std::atomic<int> g_clock{-1}; // hours * 60 + minutes; -1: not read yet
        std::atomic<uint32_t> g_timeMs{0};
        fs::path g_folder;
        std::mutex g_tableMutex;
        std::vector<Light> g_lights;
        int g_tableState = 0; // 0 not read, 1 read, -1 missing
        bool g_capturedThisChain = false; // the processing thread's
        std::atomic<uint8_t *> g_rdram{nullptr};

        // The render origin and the camera as the game kicked each chain (the EE thread, chain queued), found again when
        // that chain is processed: the game thread runs a frame ahead of the processing, so the origin read at its frame
        // is the next frame's - a step of 12.5 / 108.25 m off for one frame each time it moves.
        struct ChainView
        {
            const void *key = nullptr;
            unsigned seq = 0;
            float origin[3] = {}, oldOrigin[2] = {}, camera[3] = {};
        };
        std::mutex g_chainMutex;
        ChainView g_chainViews[16];
        unsigned g_chainNext = 0;
        ChainView g_view;          // the processing thread's: the chain being processed
        bool g_viewValid = false;
        struct OriginStats
        {
            uint64_t frames = 0, back[6] = {}, oldOff = 0, missing = 0;
            float maxLag = 0.0f;
        };
        OriginStats g_originStats; // the processing thread's (VCS_LODLIGHTS_LOG)

        float envFloat(const char *name, float fallback)
        {
            const char *e = std::getenv(name);
            return e && *e ? static_cast<float>(std::atof(e)) : fallback;
        }
        bool logOn()
        {
            static const bool on = std::getenv("VCS_LODLIGHTS_LOG") != nullptr;
            return on;
        }

        // One `{ x, y, z, size, farclip, heading, r, g, b, a, show, nodist },` row of lodl.c's aLodLights[]
        bool parseRow(std::string line, Light &l)
        {
            for (size_t c = line.find("/*"); c != std::string::npos; c = line.find("/*"))
            {
                const size_t e = line.find("*/", c + 2);
                line.erase(c, e == std::string::npos ? std::string::npos : e + 2 - c);
            }
            const size_t a = line.find('{'), b = line.find('}');
            if (a == std::string::npos || b == std::string::npos || b < a)
                return false;
            double v[12];
            int n = 0;
            const char *p = line.c_str() + a + 1, *end = line.c_str() + b;
            while (n < 12 && p < end)
            {
                char *next = nullptr;
                v[n] = std::strtod(p, &next);
                if (next == p)
                    return false;
                ++n;
                p = next;
                while (p < end && (*p == 'f' || *p == 'F' || *p == ' ' || *p == '\t'))
                    ++p;
                if (p < end && *p == ',')
                    ++p;
            }
            if (n != 12 || !(v[4] > 0.0))
                return false;
            l.x = static_cast<float>(v[0]);
            l.y = static_cast<float>(v[1]);
            l.z = static_cast<float>(v[2]);
            l.size = static_cast<float>(v[3]);
            l.farClip = static_cast<float>(v[4]);
            l.heading = static_cast<float>(v[5]);
            auto byte = [](double d) { return static_cast<uint8_t>(std::clamp(d, 0.0, 255.0)); };
            l.r = byte(v[6]);
            l.g = byte(v[7]);
            l.b = byte(v[8]);
            l.a = byte(v[9]);
            l.show = static_cast<int>(v[10]);
            l.noDistance = v[11] != 0.0;
            return std::isfinite(l.x) && std::isfinite(l.y) && std::isfinite(l.z);
        }

        // The table, read once (any thread; the first caller reads it)
        // The lights by place: kGrid x kGrid cells of kCell metres, the table sorted by cell (row by row), so the cells of
        // one row in range are one run of it; lights shown at any distance (nodist) kept apart. Only the cells within the
        // far clip are looked at (a fifth of the map at 1000 m) instead of all ~4400 lights.
        constexpr int kGrid = 32;
        constexpr float kCell = 150.0f, kGridOrigin = -kGrid * kCell / 2.0f;
        std::vector<uint32_t> g_cellStart; // kGrid * kGrid + 1 offsets into g_lights
        std::vector<Light> g_always;       // nodist
        int cellOf(float v) { return std::clamp(static_cast<int>(std::floor((v - kGridOrigin) / kCell)), 0, kGrid - 1); }
        void buildGrid()
        {
            std::vector<Light> placed;
            for (const Light &l : g_lights)
                (l.noDistance ? g_always : placed).push_back(l);
            auto key = [](const Light &l) { return cellOf(l.y) * kGrid + cellOf(l.x); };
            std::stable_sort(placed.begin(), placed.end(), [&](const Light &a, const Light &b) { return key(a) < key(b); });
            g_cellStart.assign(kGrid * kGrid + 1, 0u);
            for (const Light &l : placed)
                ++g_cellStart[key(l) + 1];
            for (int i = 0; i < kGrid * kGrid; ++i)
                g_cellStart[i + 1] += g_cellStart[i];
            g_lights = std::move(placed);
        }

        const std::vector<Light> *table()
        {
            std::lock_guard<std::mutex> lock(g_tableMutex);
            if (g_tableState == 0)
            {
                g_tableState = -1;
                std::vector<fs::path> candidates;
                if (const char *e = std::getenv("VCS_LODLIGHTS_FILE"); e && *e)
                    candidates.push_back(fs::u8path(e));
                candidates.push_back(g_folder / "project2dfx" / "lodl.c");
                for (const fs::path &path : candidates)
                {
                    std::ifstream in(path);
                    if (!in)
                        continue;
                    std::string line;
                    bool inside = false;
                    while (std::getline(in, line))
                    {
                        if (!inside)
                        {
                            inside = line.find("aLodLights[]") != std::string::npos;
                            continue;
                        }
                        if (line.find("};") != std::string::npos)
                            break;
                        Light l{};
                        if (parseRow(line, l))
                            g_lights.push_back(l);
                    }
                    if (!g_lights.empty())
                    {
                        buildGrid();
                        g_tableState = 1;
                        std::fprintf(stderr, "[lodlights] %zu lights from %s\n", g_lights.size() + g_always.size(), path.u8string().c_str());
                        break;
                    }
                }
                if (g_tableState != 1)
                    std::fprintf(stderr, "[lodlights] no light table (project2dfx/lodl.c next to vcs.exe; python tools/build.py "
                                         "downloads it): Distant lights stay off\n");
            }
            return g_tableState == 1 ? &g_lights : nullptr;
        }

        // Project2DFX's strength by the clock (0 by day)
        float nightStrength(int clock)
        {
            const int h = clock / 60;
            if (!(h >= 19 || h < 7))
                return 0.0f;
            const float t = static_cast<float>(clock);
            if (h >= 19)
                return std::min(30.0f + (t - 1140.0f) * (225.0f / 300.0f), 255.0f) / 255.0f;
            if (h < 3)
                return 1.0f;
            return std::min(30.0f + (420.0f - t) * (225.0f / 240.0f), 255.0f) / 255.0f;
        }

        bool blinkOn(int mode, uint32_t ms)
        {
            uint32_t on = 0, off = 0;
            switch (mode)
            {
            case 1: on = 500; off = 500; break;
            case 2: on = 1000; off = 1000; break;
            case 3: on = 2000; off = 2000; break;
            case 4: on = 3000; off = 3000; break;
            case 5: on = 4000; off = 4000; break;
            case 6: on = 5000; off = 5000; break;
            case 7: on = 6000; off = 4000; break;
            default: return true;
            }
            return ms % (on + off) < on;
        }

        // Traffic lights (size 0.45): which colour shows this minute (Project2DFX's cycle; facing decides the phase)
        bool trafficOn(const Light &l, int minute)
        {
            const bool yellow = l.r >= 250 && l.g >= 100 && l.b <= 100;
            if (yellow)
                return minute % 10 == 9;
            const bool redWindow = minute < 9 || (minute >= 20 && minute < 29) || (minute >= 40 && minute < 49);
            const bool greenWindow = (minute > 9 && minute < 19) || (minute > 29 && minute < 39) || (minute > 49 && minute < 59);
            const bool red = l.r >= 250 && l.g < 100 && l.b == 0, green = l.r == 0 && l.g >= 250 && l.b == 0;
            if (std::fabs(l.heading) > kPi * 0.5f)
                return (red && redWindow) || (green && greenWindow);
            return (green && redWindow) || (red && greenWindow);
        }

        struct Camera
        {
            float v[4][4];    // qwords 16-19: render space -> clip space (view x projection; rows: x, y, z, translation)
            float p[4][4];    // qwords 12-15: clip space -> GS window (the viewport: x 320, y -224, centre 2048, Z range)
            float origin[3];  // render space = world - origin (the current cell's centre: kCellOrigin)
            float pos[3];     // the camera in the world
            float scale[2];   // a metre across the view in clip x / y (x w)
        };

        // c = x V0 + y V1 + z V2 + V3, then g = c.x P0 + c.y P1 + c.z P2 + c.w P3 (as the VU's MULA / MADDA chains)
        void toView(const Camera &cam, const float w[3], float c[4])
        {
            const float x = w[0] - cam.origin[0], y = w[1] - cam.origin[1], z = w[2] - cam.origin[2];
            for (int k = 0; k < 4; ++k)
                c[k] = cam.v[0][k] * x + cam.v[1][k] * y + cam.v[2][k] * z + cam.v[3][k];
        }
        void toWindow(const Camera &cam, const float c[4], float g[4])
        {
            for (int k = 0; k < 4; ++k)
                g[k] = cam.p[0][k] * c[0] + cam.p[1][k] * c[1] + cam.p[2][k] * c[2] + cam.p[3][k] * c[3];
        }

        struct Stats
        {
            uint64_t frames = 0, inRange = 0, drawn = 0;
            double us = 0.0; // time spent in drawLights
        };
        Stats g_stats; // the GS stage's

        // A round glow as a 12-triangle fan (a triangle list): the colour at the centre, black at the rim (added: nothing)
        constexpr int kSegments = 12;
        struct Circle
        {
            float c[kSegments + 1], s[kSegments + 1];
            Circle()
            {
                for (int i = 0; i <= kSegments; ++i)
                {
                    const float a = 2.0f * kPi * static_cast<float>(i) / kSegments;
                    c[i] = std::cos(a);
                    s[i] = std::sin(a);
                }
            }
        };
        const Circle g_circle;

        void glow(std::vector<GSHwOverlayVertex> &out, float x, float y, float z, float rx, float ry, float r, float g, float b)
        {
            const GSHwOverlayVertex centre{x, y, z, static_cast<uint8_t>(std::min(r, 255.0f)), static_cast<uint8_t>(std::min(g, 255.0f)),
                                           static_cast<uint8_t>(std::min(b, 255.0f)), 0x80};
            const size_t at = out.size();
            out.resize(at + 3u * kSegments);
            GSHwOverlayVertex *v = out.data() + at;
            for (int i = 0; i < kSegments; ++i, v += 3)
            {
                v[0] = centre;
                v[1] = {x + g_circle.c[i] * rx, y + g_circle.s[i] * ry, z, 0, 0, 0, 0x80};
                v[2] = {x + g_circle.c[i + 1] * rx, y + g_circle.s[i + 1] * ry, z, 0, 0, 0, 0x80};
            }
        }

        // On the GS stage, before the frame's world draws: the lights in view as glows (gsHwSetOverlay)
        void drawLights(const Camera &cam, int clock, uint32_t ms)
        {
            const std::vector<Light> *lights = table();
            const float strength = nightStrength(clock);
            static thread_local std::vector<GSHwOverlayVertex> verts; // swapped with the renderer's: no allocation a frame
            verts.clear();
            if (!lights || strength <= 0.0f || !std::isfinite(cam.pos[0]) || !std::isfinite(cam.pos[1]) || !std::isfinite(cam.pos[2]))
            {
                gsHwSetOverlay(verts);
                return;
            }
            static const float farClip = envFloat("VCS_LODLIGHTS_FARCLIP", 1000.0f);
            static const float radiusScale = envFloat("VCS_LODLIGHTS_RADIUS", 1.5f);
            static const float intensity = envFloat("VCS_LODLIGHTS_INTENSITY", 1.0f);
            static const float pullCap = envFloat("VCS_LODLIGHTS_PULL", 0.0f);
            const int minute = clock % 60;
            // the screen in window coordinates: the viewport's centre (P3) +- its half size (P0.x, P1.y)
            const float hx = std::fabs(cam.p[0][0]) / cam.p[3][3], hy = std::fabs(cam.p[1][1]) / cam.p[3][3];
            const float screen[4] = {cam.p[3][0] / cam.p[3][3] - hx, cam.p[3][1] / cam.p[3][3] - hy, cam.p[3][0] / cam.p[3][3] + hx,
                                     cam.p[3][1] / cam.p[3][3] + hy};
            ++g_stats.frames;
            const auto t0 = std::chrono::steady_clock::now();
            auto consider = [&](const Light &l) {
                const float dx = cam.pos[0] - l.x, dy = cam.pos[1] - l.y, dz = cam.pos[2] - l.z;
                const float d2 = dx * dx + dy * dy + dz * dz;
                const float start = l.farClip / 3.0f;
                if (!l.noDistance && !(d2 > start * start && d2 < farClip * farClip))
                    return;
                if (std::fabs(l.size - 0.45f) < 0.0001f ? !trafficOn(l, minute) : !blinkOn(l.show, ms))
                    return;
                ++g_stats.inRange;
                const float dist = std::sqrt(d2);
                float radius = 3.5f;
                if (!l.noDistance)
                    radius = std::min((dist - start) * (3.5f / std::max(l.farClip - start, 1e-3f)), 3.5f);
                radius *= std::min(0.0025f * dist + 0.25f, 4.0f);
                radius *= l.size * radiusScale;
                if (radius <= 0.01f)
                    return;
                const float w[3] = {l.x, l.y, l.z};
                float c[4], g[4];
                toView(cam, w, c);
                toWindow(cam, c, g);
                if (!(g[3] > 0.5f)) // behind the camera / at the near plane
                    return;
                const float iw = 1.0f / g[3];
                const float sx = g[0] * iw, sy = g[1] * iw;
                float sz = g[2] * iw;
                if (pullCap > 0.0f && dist > 1.0f)
                {
                    // VCS_LODLIGHTS_PULL=m (off): the glow's depth taken that many metres nearer (at most its radius, half
                    // the way) - kept for trying; the mod draws each light at its own depth, and the depth layer fix
                    // (onMscal) was enough in the checks
                    const float pull = std::min(std::min(radius, pullCap), 0.5f * dist) / dist;
                    const float wp[3] = {l.x + dx * pull, l.y + dy * pull, l.z + dz * pull};
                    float cp[4], gp[4];
                    toView(cam, wp, cp);
                    toWindow(cam, cp, gp);
                    if (gp[3] > 0.5f)
                        sz = gp[2] / gp[3];
                }
                // the radius on screen: the view's x / y moved by it; the projection is linear, so the window point moves
                // by radius x scale x that row of P (its own aspect, Hor+ included) - w barely changes, P0 / P1 have no w
                const float rx = std::fabs(radius * cam.scale[0] * cam.p[0][0]) * iw;
                const float ry = std::fabs(radius * cam.scale[1] * cam.p[1][1]) * iw;
                if (rx < 0.25f || ry < 0.25f || !std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sz) || sz <= 0.0f)
                    return;
                if (sx + rx < screen[0] || sx - rx > screen[2] || sy + ry < screen[1] || sy - ry > screen[3]) // off screen
                    return;
                const float k = strength * (l.a / 255.0f) * intensity;
                glow(verts, sx, sy, sz, rx, ry, l.r * k, l.g * k, l.b * k);
                ++g_stats.drawn;
            };
            const int cx0 = cellOf(cam.pos[0] - farClip), cx1 = cellOf(cam.pos[0] + farClip);
            const int cy0 = cellOf(cam.pos[1] - farClip), cy1 = cellOf(cam.pos[1] + farClip);
            for (int cy = cy0; cy <= cy1; ++cy)
                for (uint32_t i = g_cellStart[cy * kGrid + cx0], e = g_cellStart[cy * kGrid + cx1 + 1]; i < e; ++i)
                    consider((*lights)[i]);
            for (const Light &l : g_always)
                consider(l);
            gsHwSetOverlay(verts);
            g_stats.us += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
            if (logOn())
            {
                static auto last = std::chrono::steady_clock::now();
                const auto now = std::chrono::steady_clock::now();
                if (now - last >= std::chrono::seconds(5))
                {
                    last = now;
                    std::fprintf(stderr, "[lodlights] %02d:%02d strength %.2f: %llu frames, %.0f lights in range and %.0f drawn a "
                                         "frame (%.1f us); renderer: %llu draws, %llu triangles. camera %.1f %.1f %.1f\n",
                                 clock / 60, clock % 60, strength, (unsigned long long)g_stats.frames,
                                 g_stats.frames ? double(g_stats.inRange) / g_stats.frames : 0.0,
                                 g_stats.frames ? double(g_stats.drawn) / g_stats.frames : 0.0,
                                 g_stats.frames ? g_stats.us / g_stats.frames : 0.0,
                                 (unsigned long long)gsHwOverlayDraws(), (unsigned long long)gsHwOverlayTriangles(), cam.pos[0],
                                 cam.pos[1], cam.pos[2]);
                    for (int r = 0; r < 4; ++r)
                        std::fprintf(stderr, "[lodlights]   V%d %9.4f %9.4f %9.4f %9.4f   P%d %10.4f %10.4f %10.4f %10.4f\n", r,
                                     cam.v[r][0], cam.v[r][1], cam.v[r][2], cam.v[r][3], r, cam.p[r][0], cam.p[r][1], cam.p[r][2],
                                     cam.p[r][3]);
                    g_stats = {};
                }
            }
        }

        // VCS_LODLIGHTS_LOG=2: every 5 s, the cameras one chain's world packets were drawn with (the processing thread's)
        struct SeenCamera
        {
            float v[16], p[16];
            uint32_t packets = 0, first = 0;
        };
        std::vector<SeenCamera> g_seen;
        uint32_t g_packetIndex = 0;
        bool g_logChain = false;

        // The map cell's centre nearest to `world` - `render`, x and y (FUN_00241F00: x = (gx + 0.5 - (gy & 1) / 2) x 125 -
        // 2400, y = (gy + 0.5) x 108.25 - 2000); z is left to the caller (the game's: 0 in the city - its rule rounds the
        // camera's height to 32 m only in a mode not seen on the streets)
        bool snapToCell(const float world[3], const float render[3], float out[2])
        {
            const float dx = world[0] - render[0], dy = world[1] - render[1];
            if (!std::isfinite(dx) || !std::isfinite(dy) || std::fabs(dx) > 1e5f || std::fabs(dy) > 1e5f)
                return false;
            const int gy = static_cast<int>(std::lround((dy + 2000.0f) / 108.25f - 0.5f));
            const float odd = static_cast<float>(gy & 1) * 0.5f;
            const long gx = std::lround((dx + 2400.0f) / 125.0f - 0.5f + odd);
            out[0] = (static_cast<float>(gx) + 0.5f - odd) * 125.0f - 2400.0f;
            out[1] = (static_cast<float>(gy) + 0.5f) * 108.25f - 2000.0f;
            return true;
        }

        void queued(const void *key, uint32_t)
        {
            uint8_t *ram = g_rdram.load(std::memory_order_relaxed);
            if (!ram || !g_enabled.load(std::memory_order_relaxed))
                return;
            ChainView v;
            v.key = key;
            float neg[3];
            std::memcpy(neg, ram + kCellOrigin, 12);
            for (int i = 0; i < 3; ++i)
                v.origin[i] = -neg[i];
            std::memcpy(v.oldOrigin, ram + kOldOrigin, 8);
            std::memcpy(v.camera, ram + kCameraPos, 12);
            std::lock_guard<std::mutex> lock(g_chainMutex);
            v.seq = g_chainNext;
            g_chainViews[g_chainNext++ % std::size(g_chainViews)] = v;
        }

        void processing(const void *key, bool begin)
        {
            static const bool log2 = [] {
                const char *e = std::getenv("VCS_LODLIGHTS_LOG");
                return e && *e == '2';
            }();
            if (begin)
            {
                g_capturedThisChain = false;
                g_viewValid = false;
                {
                    // newest first, and taken once: the chain buffers are reused, so an older entry can hold the same key
                    std::lock_guard<std::mutex> lock(g_chainMutex);
                    constexpr unsigned n = static_cast<unsigned>(std::size(g_chainViews));
                    for (unsigned i = 1; i <= n && i <= g_chainNext; ++i)
                    {
                        ChainView &v = g_chainViews[(g_chainNext - i) % n];
                        if (v.key == key)
                        {
                            g_view = v;
                            g_viewValid = true;
                            v.key = nullptr;
                            break;
                        }
                    }
                }
                g_packetIndex = 0;
                g_seen.clear();
                static auto last = std::chrono::steady_clock::now();
                const auto now = std::chrono::steady_clock::now();
                g_logChain = log2 && now - last >= std::chrono::seconds(5);
                if (g_logChain)
                    last = now;
            }
            else if (g_logChain)
            {
                std::fprintf(stderr, "[lodlights] chain: %u world packets, %zu cameras\n", g_packetIndex, g_seen.size());
                for (const SeenCamera &s : g_seen)
                    std::fprintf(stderr, "[lodlights]   from packet %u, %u packets: V0 %.3f %.3f %.3f %.3f V3 %.2f %.2f %.2f %.2f P0 %.1f "
                                         "P3 %.1f %.1f %.1f\n",
                                 s.first, s.packets, s.v[0], s.v[1], s.v[2], s.v[3], s.v[12], s.v[13], s.v[14], s.v[15], s.p[0],
                                 s.p[12], s.p[13], s.p[14]);
                g_logChain = false;
            }
        }
    }

    void registerSettings()
    {
        settings::Spec s;
        s.key = "enhance.distant_lights";
        s.page = "World";
        s.section = "Details";
        s.label = "Distant lights";
        s.help = "At night, street lamps, windows, signs and traffic lights glow far into the distance (Project2DFX). The "
                 "original game only lights them within about 100 m. Needs the light table the build downloads.";
        s.type = settings::Type::Toggle;
        s.defaultValue = 0;
        s.env = "VCS_DISTANT_LIGHTS";
        s.onChange = [](int v) { g_enabled.store(v, std::memory_order_relaxed); };
        settings::add(s);
    }

    void install(PS2Runtime &, const fs::path &gameFolder)
    {
        g_folder = gameFolder;
        ps2chain::addChainObservers(&queued, &processing);
    }

    void onGameFrame(uint8_t *rdram)
    {
        g_rdram.store(rdram, std::memory_order_relaxed);
        if (!g_enabled.load(std::memory_order_relaxed))
            return;
        const int h = rdram[kClockHours], m = rdram[kClockHours + 1];
        uint32_t area = 0;
        std::memcpy(&area, rdram + kCurrArea, 4);
        // inside an interior (drawn somewhere in the sky / under the map) the city's lights stay off
        g_clock.store(h < 24 && m < 60 && (area == 0u || area == 4u) ? h * 60 + m : -1, std::memory_order_relaxed);
        uint32_t ms = 0;
        std::memcpy(&ms, rdram + kTimeMs, 4);
        g_timeMs.store(ms, std::memory_order_relaxed);
        for (int i = 0; i < 3; ++i)
        {
            float f;
            std::memcpy(&f, rdram + kCellOrigin + 4u * i, 4);
            f = -f;
            uint32_t bits;
            std::memcpy(&bits, &f, 4);
            g_origin[i].store(bits, std::memory_order_relaxed);
        }
    }

    void onMscal(uint32_t startPc, const uint8_t *vu)
    {
        if (g_logChain && startPc == 0x30u)
        {
            SeenCamera s;
            std::memcpy(s.p, vu + 12u * 16u, 64);
            std::memcpy(s.v, vu + 16u * 16u, 64);
            auto same = [&](const SeenCamera &o) { return !std::memcmp(o.v, s.v, 64) && !std::memcmp(o.p, s.p, 64); };
            auto it = std::find_if(g_seen.begin(), g_seen.end(), same);
            if (it == g_seen.end() && g_seen.size() < 16u)
            {
                s.first = g_packetIndex;
                g_seen.push_back(s);
                it = g_seen.end() - 1;
            }
            if (it != g_seen.end())
                ++it->packets;
            ++g_packetIndex;
        }
        if (startPc != 0x30u || g_capturedThisChain || !g_enabled.load(std::memory_order_relaxed))
            return;
        g_capturedThisChain = true;
        const int clock = g_clock.load(std::memory_order_relaxed);
        if (clock < 0)
            return;
        Camera cam;
        std::memcpy(cam.p, vu + 12u * 16u, 64);
        std::memcpy(cam.v, vu + 16u * 16u, 64);
        // The viewport's z offset is a depth layer: the world is drawn in groups 524283.8 / 285.8 / 288 / 290.1 / 292.1
        // (VCS_LODLIGHTS_LOG=2), most of the buildings at 524288, and the first packet - this one - is the farthest. The
        // glows take the buildings' layer, or a light on a wall would sit ~4 units (3 m at 300 m, 36 m at 1 km) behind it.
        static const bool depthFix = envFloat("VCS_LODLIGHTS_DEPTH", 1.0f) != 0.0f;
        if (depthFix && std::fabs(cam.p[3][2] - 524288.0f) < 16.0f)
            cam.p[3][2] = 524288.0f * cam.p[3][3];
        // the camera: the point whose clip x, y and w are 0 (16-19 is view x projection: not orthonormal)
        const float(&m)[4][4] = cam.v;
        const int ks[3] = {0, 1, 3};
        float a[3][3], rhs[3];
        for (int r = 0; r < 3; ++r)
        {
            for (int i = 0; i < 3; ++i)
                a[r][i] = m[i][ks[r]];
            rhs[r] = -m[3][ks[r]];
        }
        auto det3 = [](const float x[3][3]) {
            return x[0][0] * (x[1][1] * x[2][2] - x[1][2] * x[2][1]) - x[0][1] * (x[1][0] * x[2][2] - x[1][2] * x[2][0]) +
                   x[0][2] * (x[1][0] * x[2][1] - x[1][1] * x[2][0]);
        };
        const float d = det3(a);
        if (!(std::fabs(d) > 1e-12f))
            return;
        for (int i = 0; i < 3; ++i)
        {
            float b[3][3];
            std::memcpy(b, a, sizeof(b));
            for (int r = 0; r < 3; ++r)
                b[r][i] = rhs[r];
            cam.pos[i] = det3(b) / d;
        }
        // view x / y -> clip x / y: the lengths of the matrix's first two columns (a rotation times a scale)
        for (int k = 0; k < 2; ++k)
            cam.scale[k] = std::sqrt(m[0][k] * m[0][k] + m[1][k] * m[1][k] + m[2][k] * m[2][k]);
        // The origin this chain's world was drawn from. Always a cell centre, so the one nearest to TheCamera's position
        // at the kick minus the camera in render space (they agree to the centimetre: VCS_LODLIGHTS_LOG=1); 0x6FE980 at
        // the kick is another cell in about one chain in eight on a drive, TheCamera + 0x1170 is left behind for whole
        // stretches (game-internals.md 7c). VCS_LODLIGHTS_ORIGIN=cell: 0x6FE980 at the kick; =old: TheCamera + 0x1170 at
        // the game frame, z 0 (before 2026-11-25).
        static const int originMode = [] {
            const char *e = std::getenv("VCS_LODLIGHTS_ORIGIN");
            return !e || !*e ? 0 : !std::strcmp(e, "cell") ? 1 : !std::strcmp(e, "old") ? 2 : 0;
        }();
        float cellOrigin[3];
        if (g_viewValid)
            std::memcpy(cellOrigin, g_view.origin, 12);
        else
            for (int i = 0; i < 3; ++i)
            {
                const uint32_t bits = g_origin[i].load(std::memory_order_relaxed);
                std::memcpy(&cellOrigin[i], &bits, 4);
            }
        float gridOrigin[3] = {0.0f, 0.0f, cellOrigin[2]};
        const bool gridOk = g_viewValid && snapToCell(g_view.camera, cam.pos, gridOrigin);
        // TheCamera far from the drawn camera even from the nearest cell (a teleport, a camera cut between the frame drawn
        // and its kick): which cell is not known - no glows this frame rather than glows from the wrong cell
        float lag = 0.0f;
        if (gridOk)
        {
            float e2 = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                const float e = cam.pos[i] + gridOrigin[i] - g_view.camera[i];
                e2 += e * e;
            }
            lag = std::sqrt(e2);
        }
        const bool lost = originMode == 0 && gridOk && lag > 30.0f;
        if (originMode == 2 && g_viewValid)
        {
            cam.origin[0] = g_view.oldOrigin[0];
            cam.origin[1] = g_view.oldOrigin[1];
            cam.origin[2] = 0.0f;
        }
        else if (originMode == 0 && gridOk)
            std::memcpy(cam.origin, gridOrigin, 12);
        else
            std::memcpy(cam.origin, cellOrigin, 12);
        if (logOn())
        {
            ++g_originStats.frames;
            if (!gridOk)
                ++g_originStats.missing;
            else
            {
                // which kick's 0x6FE980 the cell found matches: this chain's (0), the one before (1), ... (none: 4)
                int back = 4;
                {
                    std::lock_guard<std::mutex> lock(g_chainMutex);
                    constexpr unsigned n = static_cast<unsigned>(std::size(g_chainViews));
                    for (unsigned k = 0; k < 4 && k <= g_view.seq; ++k)
                    {
                        const ChainView &v = g_chainViews[(g_view.seq - k) % n];
                        if (v.seq == g_view.seq - k && std::fabs(v.origin[0] - gridOrigin[0]) < 0.01f &&
                            std::fabs(v.origin[1] - gridOrigin[1]) < 0.01f)
                        {
                            back = static_cast<int>(k);
                            break;
                        }
                    }
                }
                ++g_originStats.back[lost ? 5 : back];
                g_originStats.oldOff += std::fabs(gridOrigin[0] - g_view.oldOrigin[0]) + std::fabs(gridOrigin[1] - g_view.oldOrigin[1]) > 0.01f;
                if (!lost)
                    g_originStats.maxLag = std::max(g_originStats.maxLag, lag);
            }
            static auto last = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            if (now - last >= std::chrono::seconds(5))
            {
                last = now;
                std::fprintf(stderr, "[lodlights] origin %.2f %.2f %.2f: %llu frames; the cell found is 0x6FE980 at this chain's kick "
                                     "in %llu, 1 / 2 / 3 kicks before %llu / %llu / %llu, none %llu; lost %llu (glows left out); not "
                                     "TheCamera + 0x1170 in %llu; no chain view %llu; TheCamera up to %.1f m from the drawn camera\n",
                             cam.origin[0], cam.origin[1], cam.origin[2], (unsigned long long)g_originStats.frames,
                             (unsigned long long)g_originStats.back[0], (unsigned long long)g_originStats.back[1],
                             (unsigned long long)g_originStats.back[2], (unsigned long long)g_originStats.back[3],
                             (unsigned long long)g_originStats.back[4], (unsigned long long)g_originStats.back[5],
                             (unsigned long long)g_originStats.oldOff, (unsigned long long)g_originStats.missing,
                             g_originStats.maxLag);
                g_originStats = {};
            }
        }
        for (int i = 0; i < 3; ++i)
            cam.pos[i] += cam.origin[i];
        const uint32_t ms = g_timeMs.load(std::memory_order_relaxed);
        const int shownClock = lost ? 12 * 60 : clock; // (noon: nothing drawn, the overlay emptied)
        ps2_render_worker::runOnGsStage([cam, shownClock, ms] { drawLights(cam, shownClock, ms); });
    }
}
