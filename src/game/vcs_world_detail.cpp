// World: traffic / pedestrian density and draw distance (docs/game-internals.md 6o, 12d).
//
// Density. Mission scripts set the game's density multipliers all the time (0 to clear the streets, 10 for the
// credits; CarDensityMultiplier 0x48A968, PedDensityMultiplier 0x487FF8), and saves keep them, so those stay the
// scripts'. The spawners multiply them by a constant of their own, read nowhere else: cars FUN_00430290 by 0.7 at
// 0x48A680, peds FUN_002B17A8 by 0.4 at 0x48A67C. Scaling those constants scales every script's density the same way
// (0 stays 0). A spawn also happens only where a random 0..15 is at most the road's (or path's) class in the table
// 0x487498 = {0, 4, 7, 15}; each spawner gets that chance times the setting (class 0 - next to no traffic - as it is),
// written before the car spawner FUN_00430290 and the ped spawner FUN_002B17A8 run, the only code that reads it.
//
// The pools (40 vehicles, 70 peds) are where more traffic would go wrong: every 8th frame, with fewer than 8 slots free,
// the game deletes the car (FUN_00433188) or ped (FUN_002B0C00) closest to the camera - in view or not. So each
// spawner's cap (cars: random + the other car counters below 0x48A9A8, 16; peds: below 0x488004 / 0x488008, 25 / 30)
// is set right before it runs: above the original only while 16 slots stay free in each pool (and 2 more peds for a
// car's driver) - the 8 that trigger the deletion and 8 for the missions' own CREATE_CHAR / CREATE_CAR.
//
// Cars in view stay: the game removes a car beyond 160 m x GenerationDistMultiplier even on screen (PossiblyRemoveVehicle
// FUN_00433378; 60 m when it thinks the car is off screen - a 4:3 test). With more traffic or a longer draw distance
// that is where cars vanished in front of the player, so the removal is skipped for a car inside the view and within
// the distance cars are drawn at (160 m x GenerationDistMultiplier x the draw distance). It is removed once out of view.
//
// Draw distance. CCamera::Process multiplies LODDistMultiplier (TheCamera+0x7DC: 70 / FOV, more when high up, at most
// 2) by 0x4887F8 - 1.0 in the executable, never written, the PS2 build's counterpart of VC's ms_lodDistScale - after
// it copied the value into GenerationDistMultiplier, so spawn distances do not change. Every model's LOD distances and
// the entity visibility test scale with it.
//
// 100% on all three writes the game's own values and every hook passes through: the original game.
// VCS_WORLD_DETAIL_HOOKS=0 writes nothing and hooks nothing; VCS_WORLD_KEEP=0 drops the pool reserve and keeps no car
// in view (the caps as scaled: how it was before 2026-11-07's fix, to compare). Either way a car that disappears
// within 100 m in view is counted and logged ([world] car vanished in view).
#include "game/vcs_world_detail.h"

#include "game/vcs_display.h"
#include "game/vcs_hle_world.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "runtime/ps2_control.h"
#include "settings/settings.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace vcs::world
{
    namespace
    {
        constexpr uint32_t kCarFactor = 0x0048A680u; // float 0.7 (FUN_00430290)
        constexpr uint32_t kPedFactor = 0x0048A67Cu; // float 0.4 (FUN_002B17A8)
        constexpr uint32_t kCarCap = 0x0048A9A8u;    // int 16: random cars + the other car counters
        constexpr uint32_t kCarCounters = 0x0048A96Cu; // NumRandomCars, +4, +8, +0xC, (+0x10 not counted), +0x14, +0x18
        constexpr uint32_t kPedCapOut = 0x00488004u; // int 25: peds outdoors (CGame::currArea 0)
        constexpr uint32_t kPedCapIn = 0x00488008u;  // int 30: peds elsewhere
        constexpr uint32_t kPedCount = 0x004CD550u;  // the ped count the spawner compares (set just before it runs)
        constexpr uint32_t kLodScale = 0x004887F8u;  // float 1.0: LODDistMultiplier's last factor
        constexpr uint32_t kSpawnChance = 0x00487498u; // u8[4] {0, 4, 7, 15}: spawn when rand & 15 <= [road class]
        constexpr uint32_t kCarSpawner = 0x00430290u;
        constexpr uint32_t kPedSpawner = 0x002B17A8u;
        constexpr uint32_t kPossiblyRemoveVehicle = 0x00433378u; // (vehicle a0)
        constexpr uint32_t kCamera = 0x006F44D0u;  // TheCamera: right +0, forward +0x10, up +0x20, position +0x30
        constexpr uint32_t kGenerationDist = kCamera + 0x7D0u; // GenerationDistMultiplier
        constexpr uint32_t kFov = 0x00487484u;     // degrees
        constexpr uint32_t kPedPool = 0x00487A94u;
        constexpr uint32_t kVehiclePool = 0x00487A98u;
        constexpr uint32_t kPackets = 0x00520F90u; // +0x14 half in use, +0x18 cursor up, +0x1C data down, halves from +0x20
        constexpr uint32_t kPacketHalf = 0xE6670u;
        // CPopulation's distance factor (FUN_002B44C8, no arguments, f0): 1 on foot, 1 + |player vehicle velocity xy|
        // - 0.1 clamped to [1, 1.5] in a vehicle (its +0x140 / +0x144). Every population distance is multiplied by it:
        // the ped spawn rings FUN_002AFE80 passes FUN_002B17A8 (on screen 35..60 m, off screen 10..20 m, the first two x
        // GenerationDistMultiplier) and the removal in FUN_002B0200 (65 m x GenerationDistMultiplier, 51 m with a ped
        // flag, 20 m off screen after a while) - so scaling it moves spawning and removal together (game-internals 12d).
        constexpr uint32_t kPopDistFactor = 0x002B44C8u;
        constexpr uint32_t kPlayers = 0x004E4910u, kPlayerIndex = 0x004CD128u, kPlayerSize = 0x190u;
        constexpr uint32_t kPedVehicle = 0x480u;

        constexpr int kMaxPercent = 300;
        constexpr int kCarCapMax = 28;       // FUN_00430148 stops at 30 random cars on its own
        constexpr int kPedCapMax = 45;
        constexpr int kVehicleReserve = 16;  // free slots kept: the game deletes the closest car below 8
        constexpr int kPedReserve = 16;      // the same for peds (FUN_002B0C00)
        constexpr int kDriverSlots = 2;      // a random car's driver (and a passenger) come from the ped pool
        constexpr float kCarRadius = 6.0f;   // the in-view test's margin

        bool g_enabled = false;
        bool g_keep = true; // VCS_WORLD_KEEP=0: no pool reserve, no keeping cars in view
        std::atomic<int> g_traffic{100}, g_peds{100}, g_draw{100}, g_pedDistance{100}, g_carDistance{100};
        constexpr int kPedDistanceMax = 200;
        constexpr int kCarDistanceMax = 200;
        // Car distance: GenerationDistMultiplier (TheCamera+0x7D0, which CCamera::Process 0x378DD8 rewrites every frame
        // at 0x37B068) scaled from the first car spawner call of a frame (FUN_00430148 from CGame::Process 0x3BAC88:
        // the spawn spot 110 m x it ahead) through RemoveDistantCars (0x3BAC98: 160 m x it in view) to
        // RemoveCarsIfThePoolGetsFull (FUN_00433188, 0x3BACA0), whose hook puts the camera's value back - so the next
        // frame's ped step (0x3BAB44, before the camera) sees the game's own.
        constexpr uint32_t kRemoveCarsIfPoolFull = 0x00433188u;
        PS2Runtime::RecompiledFunction g_origRemoveCarsIfPoolFull = nullptr;
        float g_genBase = 0.0f, g_genWritten = -1.0f; // game thread
        bool g_genScaled = false;
        PS2Runtime::RecompiledFunction g_origPopDistFactor = nullptr;

        struct Original
        {
            bool read = false;
            bool valid = false;
            float carFactor = 0, pedFactor = 0, lodScale = 0;
            int carCap = 0, pedCapOut = 0, pedCapIn = 0;
            uint32_t spawnChance = 0; // the four class bytes
        } g_orig;

        // game thread: the spawn-chance table for each spawner (0 until the first frame read the original)
        uint32_t g_carChance = 0, g_pedChance = 0;
        PS2Runtime::RecompiledFunction g_origCarSpawner = nullptr, g_origPedSpawner = nullptr, g_origRemoveVehicle = nullptr;

        // published for the command
        std::atomic<int> g_stCarCap{0}, g_stPedCapOut{0}, g_stPedCapIn{0}, g_stPedFree{-1}, g_stVehicleFree{-1},
            g_stPedsUsed{-1}, g_stVehiclesUsed{-1}, g_stPlayerInView{-1};
        std::atomic<float> g_stLod{1.0f}, g_stPedMean{0.0f}, g_stPedFar{0.0f};
        std::atomic<int> g_stPedCount{0}, g_stCarCount{0};
        std::atomic<float> g_stCarMean{0.0f}, g_stCarFar{0.0f};
        std::atomic<uint64_t> g_stSpawnSum{0}, g_stSpawnCount{0}; // cars made: their distance from the camera
        std::atomic<uint32_t> g_stPacketUsed{0}, g_stPacketPeak{0}, g_stCarChance{0}, g_stPedChance{0};
        std::atomic<uint32_t> g_stMinVehicleFree{999}, g_stMinPedFree{999}; // since the last query
        std::atomic<uint64_t> g_stKeptInView{0}, g_stVanished{0};
        // the vanish detector: per vehicle pool slot, its flag byte (generation) last frame when it was near and in view
        int g_nearInView[256];
        float g_nearDist[256];
        // CWorld::Remove (FUN_0018FA50, entity a0): who removed each vehicle slot last (its return address), for the log
        uint32_t g_removedBy[256];
        PS2Runtime::RecompiledFunction g_origWorldRemove = nullptr;

        // VCS_FAR_CLIP: the far plane (and so the fog end) at this distance instead of 2000 (outdoors only)
        float g_farClip = 0.0f;
        // [Minty1804 fork] World "Far view from interiors" (the user's report, 2026-11-19: from the
        // safehouse window the city showed while aiming and vanished when not). Indoors (currArea not 0 / 4) the
        // follow-ped camera FUN_0029DAB0 sets the far plane to 1000 (jals at 0x29DB7C and 0x29F8B8, near 0.3 before
        // each); every other camera mode - aiming (mode 11) among them - keeps the 2000 FUN_0021E950 sets. The far
        // plane cuts the world (0x4CD12C, IsSphereVisible) and ends the fog, so through a window the follow camera
        // showed the bridge and not the towers behind it, which the aiming camera showed - at 100% draw distance too,
        // more of them with a longer one. On: the follow camera keeps 2000 indoors as well (the near plane stays 0.3).
        // Always on since 2026-11-20 (the user's wish: no setting any more); VCS_INTERIOR_FAR_VIEW=0 is the way back.
        std::atomic<int> g_interiorFarView{1};
        PS2Runtime::RecompiledFunction g_origSetFar = nullptr, g_origProjection = nullptr;

        uint32_t rd32(const uint8_t *ram, uint32_t a)
        {
            uint32_t v;
            std::memcpy(&v, ram + (a & PS2_RAM_MASK), 4);
            return v;
        }
        float rdf(const uint8_t *ram, uint32_t a)
        {
            float v;
            std::memcpy(&v, ram + (a & PS2_RAM_MASK), 4);
            return v;
        }
        void wr32(uint8_t *ram, uint32_t a, uint32_t v)
        {
            if (rd32(ram, a) != v)
                std::memcpy(ram + (a & PS2_RAM_MASK), &v, 4);
        }
        void wrf(uint8_t *ram, uint32_t a, float f)
        {
            uint32_t v;
            std::memcpy(&v, &f, 4);
            wr32(ram, a, v);
        }

        PS2_GUEST_CC void worldRemove(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (const uint32_t pool = rd32(rdram, kVehiclePool) & PS2_RAM_MASK)
            {
                const uint32_t entries = rd32(rdram, pool) & PS2_RAM_MASK, e = GPR_U32(ctx, 4) & PS2_RAM_MASK;
                const uint32_t size = rd32(rdram, pool + 8);
                if (size <= 256 && e >= entries && e < entries + size * 0x8C0u && (e - entries) % 0x8C0u == 0)
                    g_removedBy[(e - entries) / 0x8C0u] = GPR_U32(ctx, 31);
            }
            g_origWorldRemove(rdram, ctx, runtime);
        }

        // free entries of a CPool (+0 entries, +4 flag bytes - bit 0x80 free -, +8 size); -1 before it exists
        int poolFree(const uint8_t *ram, uint32_t poolPtr, int *used = nullptr)
        {
            if (used)
                *used = -1;
            const uint32_t pool = rd32(ram, poolPtr);
            if (!pool)
                return -1;
            const uint32_t flags = rd32(ram, pool + 4);
            const int size = static_cast<int>(rd32(ram, pool + 8));
            if (!flags || size <= 0 || size > 4096)
                return -1;
            int spare = 0;
            const uint8_t *f = ram + (flags & PS2_RAM_MASK);
            for (int i = 0; i < size; ++i)
                spare += (f[i] & 0x80) ? 1 : 0;
            if (used)
                *used = size - spare;
            return spare;
        }

        void noteMin(std::atomic<uint32_t> &m, int v)
        {
            if (v >= 0 && static_cast<uint32_t>(v) < m.load(std::memory_order_relaxed))
                m.store(static_cast<uint32_t>(v), std::memory_order_relaxed);
        }

        int scaledCap(int original, int percent, int limit)
        {
            if (percent <= 100)
                return original;
            return std::clamp(static_cast<int>(std::lround(original * percent / 100.0)), original, std::max(original, limit));
        }

        // a cap above the original only as far as the pools keep their reserve: never below the original (the game's
        // own behaviour), never more spawns than leave `room` slots free
        int poolCap(int original, int scaled, int count, int room)
        {
            if (scaled <= original || !g_keep)
                return scaled;
            return std::max(original, std::min(scaled, count + std::max(room, 0)));
        }

        // the peds in the pool (0xD20 bytes each, vcs_driveby.cpp) other than the player: count, mean and farthest 2D
        // distance from the camera - for the command (tests of the ped distance)
        // (cars: the vehicle pool, 0x8C0 bytes each, the player's own car left out)
        void pedDistances(const uint8_t *ram, int &count, float &mean, float &farthest, bool cars = false)
        {
            count = 0;
            mean = farthest = 0.0f;
            const uint32_t pool = rd32(ram, cars ? kVehiclePool : kPedPool) & PS2_RAM_MASK;
            if (!pool)
                return;
            const uint32_t entries = rd32(ram, pool) & PS2_RAM_MASK, flags = rd32(ram, pool + 4) & PS2_RAM_MASK;
            const int size = std::min(static_cast<int>(rd32(ram, pool + 8)), 256);
            uint32_t player = rd32(ram, kPlayers) & PS2_RAM_MASK;
            if (cars)
                player = player ? rd32(ram, player + kPedVehicle) & PS2_RAM_MASK : 0u;
            const float cx = rdf(ram, kCamera + 0x30), cy = rdf(ram, kCamera + 0x34);
            for (int i = 0; i < size && entries && flags; ++i)
            {
                const uint32_t ped = entries + static_cast<uint32_t>(i) * (cars ? 0x8C0u : 0xD20u);
                if ((ram[flags + i] & 0x80) || ped == player)
                    continue;
                const float d = std::hypot(rdf(ram, ped + 0x30) - cx, rdf(ram, ped + 0x34) - cy);
                ++count;
                mean += d;
                farthest = std::max(farthest, d);
            }
            if (count)
                mean /= static_cast<float>(count);
        }

        int carCount(const uint8_t *ram)
        {
            int sum = 0;
            for (uint32_t off : {0x00u, 0x04u, 0x08u, 0x0Cu, 0x14u, 0x18u})
                sum += static_cast<int>(rd32(ram, kCarCounters + off));
            return sum;
        }

        void writeCarCap(uint8_t *ram)
        {
            const int traffic = std::clamp(g_traffic.load(std::memory_order_relaxed), 100, kMaxPercent);
            const int vehicleFree = poolFree(ram, kVehiclePool), pedFree = poolFree(ram, kPedPool);
            const int room = std::min(vehicleFree - kVehicleReserve, pedFree - kPedReserve - kDriverSlots);
            const int cap = poolCap(g_orig.carCap, scaledCap(g_orig.carCap, traffic, kCarCapMax), carCount(ram), room);
            wr32(ram, kCarCap, static_cast<uint32_t>(cap));
            g_stCarCap.store(cap, std::memory_order_relaxed);
            noteMin(g_stMinVehicleFree, vehicleFree);
            noteMin(g_stMinPedFree, pedFree);
        }

        void writePedCaps(uint8_t *ram)
        {
            const int peds = std::clamp(g_peds.load(std::memory_order_relaxed), 100, kMaxPercent);
            const int pedFree = poolFree(ram, kPedPool);
            const int count = static_cast<int>(rd32(ram, kPedCount));
            const int room = pedFree - kPedReserve;
            const int out = poolCap(g_orig.pedCapOut, scaledCap(g_orig.pedCapOut, peds, kPedCapMax), count, room);
            const int in = poolCap(g_orig.pedCapIn, scaledCap(g_orig.pedCapIn, peds, kPedCapMax), count, room);
            wr32(ram, kPedCapOut, static_cast<uint32_t>(out));
            wr32(ram, kPedCapIn, static_cast<uint32_t>(in));
            g_stPedCapOut.store(out, std::memory_order_relaxed);
            g_stPedCapIn.store(in, std::memory_order_relaxed);
            noteMin(g_stMinPedFree, pedFree);
        }

        // inside the camera's view (a little wider than the picture), whatever is in front: TheCamera's matrix and the
        // FOV global, widened by the Hor+ factor (the view's height stays 4:3's)
        bool inView(const uint8_t *ram, const float *p, float radius)
        {
            const float d[3] = {p[0] - rdf(ram, kCamera + 0x30), p[1] - rdf(ram, kCamera + 0x34), p[2] - rdf(ram, kCamera + 0x38)};
            auto dot = [&](uint32_t row) {
                return d[0] * rdf(ram, kCamera + row) + d[1] * rdf(ram, kCamera + row + 4) + d[2] * rdf(ram, kCamera + row + 8);
            };
            const float y = dot(0x10);
            if (y < -radius)
                return false;
            const float fov = rdf(ram, kFov);
            const float t = std::tan(std::clamp(fov, 10.0f, 170.0f) * 3.14159265f / 360.0f) * 1.15f;
            const float tanH = t * std::max(1.0f, vcs::aspectWidening()), tanV = t * 0.75f;
            return std::fabs(dot(0x00)) <= std::max(y, 0.0f) * tanH + radius &&
                   std::fabs(dot(0x20)) <= std::max(y, 0.0f) * tanV + radius;
        }

        bool active() { return g_orig.valid && (g_traffic.load(std::memory_order_relaxed) > 100 || g_peds.load(std::memory_order_relaxed) > 100 || g_draw.load(std::memory_order_relaxed) > 100); }

        void readOriginals(const uint8_t *ram)
        {
            g_orig.read = true;
            g_orig.carFactor = rdf(ram, kCarFactor);
            g_orig.pedFactor = rdf(ram, kPedFactor);
            g_orig.lodScale = rdf(ram, kLodScale);
            g_orig.carCap = static_cast<int>(rd32(ram, kCarCap));
            g_orig.pedCapOut = static_cast<int>(rd32(ram, kPedCapOut));
            g_orig.pedCapIn = static_cast<int>(rd32(ram, kPedCapIn));
            g_orig.spawnChance = rd32(ram, kSpawnChance);
            // the executable's values (docs/game-internals.md 12d); anything else: another build, leave it alone
            g_orig.valid = g_orig.carFactor == 0.7f && g_orig.pedFactor == 0.4f && g_orig.lodScale == 1.0f &&
                           g_orig.carCap == 16 && g_orig.pedCapOut == 25 && g_orig.pedCapIn == 30 &&
                           g_orig.spawnChance == 0x0F070400u;
            if (!g_orig.valid)
                std::fprintf(stderr,
                             "[world] unexpected spawner / LOD values (car %g cap %d, ped %g caps %d/%d, lod %g): density "
                             "and draw distance left alone\n",
                             g_orig.carFactor, g_orig.carCap, g_orig.pedFactor, g_orig.pedCapOut, g_orig.pedCapIn,
                             g_orig.lodScale);
        }

        // the spawn-chance table at `percent`: each class's chance ((value + 1) / 16) times it, class 0 as it is
        uint32_t scaledChance(uint32_t original, int percent)
        {
            if (percent <= 100)
                return original;
            uint32_t out = original & 0xFFu;
            for (int c = 1; c < 4; ++c)
            {
                const int v = static_cast<int>((original >> (c * 8)) & 0xFFu);
                const int scaled = std::clamp(static_cast<int>(std::lround((v + 1) * percent / 100.0)) - 1, v, 15);
                out |= static_cast<uint32_t>(scaled) << (c * 8);
            }
            return out;
        }

        // the car spawner (FUN_00430290, six calls a frame): its chance table and its cap from the pools as they are now
        PS2_GUEST_CC void carSpawner(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (g_carChance)
            {
                wr32(rdram, kSpawnChance, g_carChance);
                writeCarCap(rdram);
            }
            const int carDistance = std::clamp(g_carDistance.load(std::memory_order_relaxed), 100, kCarDistanceMax);
            if (carDistance > 100 && g_orig.valid)
            {
                const float now = rdf(rdram, kGenerationDist);
                if (!g_genScaled || now != g_genWritten) // the camera wrote a new one: scale that (once a frame)
                    g_genBase = now;
                g_genWritten = g_genBase * (carDistance / 100.0f);
                wrf(rdram, kGenerationDist, g_genWritten);
                g_genScaled = true;
            }
            g_origCarSpawner(rdram, ctx, runtime);
        }

        // [Minty1804 fork] The parked cars (after willzmkv's (dejot) 0c41004). CCarGenerators::Process (FUN_0040A9E0)
        // spawns a generator's car when the player comes within 160 m (25600.0 squared, an immediate at 0x40AB10) and
        // handles a queued one within 170 m (28900.0 at 0x40ACA8) - whatever Car distance says, so with the city drawn
        // far parked cars (a hospital's ambulance, the cars at a Pay 'n' Spray) appeared 160 m ahead, in view. The two
        // are patched (game/vcs/overrides.toml) to read guest RAM kCarGenSlots / + 4, written here before every call:
        // the game's values times Car distance squared - the game's own at 100 %, and the removal (PossiblyRemoveVehicle,
        // 160 m x GenerationDistMultiplier, which Car distance scales the same way) never takes such a car away again.
        // Installed whatever VCS_WORLD_DETAIL_HOOKS says: the patched code reads the slots, so they are always written.
        constexpr uint32_t kCarGenerators = 0x0040A9E0u;
        constexpr uint32_t kCarGenSlots = 0x0007FF4Cu; // (below the runtime's stacks at 0x80000; next to 0x7FF00's)
        PS2Runtime::RecompiledFunction g_origCarGenerators = nullptr;
        PS2_GUEST_CC void carGenerators(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const float k = g_enabled ? std::clamp(g_carDistance.load(std::memory_order_relaxed), 100, kCarDistanceMax) / 100.0f
                                      : 1.0f;
            wrf(rdram, kCarGenSlots, 25600.0f * (k * k));
            wrf(rdram, kCarGenSlots + 4u, 28900.0f * (k * k));
            g_origCarGenerators(rdram, ctx, runtime);
        }

        // RemoveCarsIfThePoolGetsFull (FUN_00433188, right after RemoveDistantCars): the camera's
        // GenerationDistMultiplier back before anything else reads it (see g_carDistance)
        PS2_GUEST_CC void removeCarsIfPoolFull(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (g_genScaled)
            {
                if (rdf(rdram, kGenerationDist) == g_genWritten)
                    wrf(rdram, kGenerationDist, g_genBase);
                g_genScaled = false;
            }
            g_origRemoveCarsIfPoolFull(rdram, ctx, runtime);
        }

        // the ped spawner (FUN_002B17A8; the ped count 0x4CD550 was just recounted by its caller FUN_002AFE80)
        PS2_GUEST_CC void pedSpawner(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (g_pedChance)
            {
                wr32(rdram, kSpawnChance, g_pedChance);
                writePedCaps(rdram);
            }
            g_origPedSpawner(rdram, ctx, runtime);
        }

        // PossiblyRemoveVehicle (FUN_00433378, vehicle a0; from RemoveDistantCars FUN_00433008, every car every frame):
        // not for a car in view within the distance cars are drawn at - the game removes it once it is out of view
        PS2_GUEST_CC void possiblyRemoveVehicle(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t car = GPR_U32(ctx, 4);
            if (car && g_keep && active())
            {
                const float p[3] = {rdf(rdram, car + 0x30), rdf(rdram, car + 0x34), rdf(rdram, car + 0x38)};
                const float dx = p[0] - rdf(rdram, kCamera + 0x30), dy = p[1] - rdf(rdram, kCamera + 0x34);
                const float drawn = 160.0f * rdf(rdram, kGenerationDist) * std::max(1.0f, g_draw.load(std::memory_order_relaxed) / 100.0f);
                if (dx * dx + dy * dy <= drawn * drawn && inView(rdram, p, kCarRadius))
                {
                    g_stKeptInView.fetch_add(1, std::memory_order_relaxed);
                    ctx->pc = GPR_U32(ctx, 31);
                    return;
                }
            }
            g_origRemoveVehicle(rdram, ctx, runtime);
        }

        // FUN_002B44C8 (see kPopDistFactor): at 100% the game's own; above it the same value, worked out here from the
        // player's vehicle (FindPlayerVehicle FUN_0024A2D0: CWorld::Players[index].ped's +0x480 when it is in it; a ped
        // getting in or out has a vehicle standing still, so the speed term is 0 either way), times the setting. All
        // in host code: no guest call, so no unwind can skip the scaling.
        PS2_GUEST_CC void popDistFactor(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const int percent = std::clamp(g_pedDistance.load(std::memory_order_relaxed), 100, kPedDistanceMax);
            if (percent == 100 || !g_orig.valid)
            {
                g_origPopDistFactor(rdram, ctx, runtime);
                return;
            }
            float k = 1.0f;
            const uint32_t index = rdram[kPlayerIndex & PS2_RAM_MASK];
            const uint32_t ped = rd32(rdram, kPlayers + index * kPlayerSize) & PS2_RAM_MASK;
            if (const uint32_t vehicle = ped ? rd32(rdram, ped + kPedVehicle) & PS2_RAM_MASK : 0u)
            {
                const float vx = rdf(rdram, vehicle + 0x140), vy = rdf(rdram, vehicle + 0x144);
                k = std::min(std::max(std::sqrt(vx * vx + vy * vy) - 0.1f + 1.0f, 1.0f), 1.5f);
            }
            ctx->f[0] = k * (percent / 100.0f);
            ctx->pc = GPR_U32(ctx, 31);
        }

        // bytes used in the packet buffer's current half (FUN_00476770): the frame being kicked (reported only)
        uint32_t packetUse(const uint8_t *ram)
        {
            const uint32_t half = rd32(ram, kPackets + 0x14);
            if (half > 1)
                return 0;
            const uint32_t start = kPackets + 0x20 + half * kPacketHalf;
            const uint32_t cursor = rd32(ram, kPackets + 0x18) & PS2_RAM_MASK;
            const uint32_t data = rd32(ram, kPackets + 0x1C) & PS2_RAM_MASK;
            const uint32_t end = start + kPacketHalf;
            if (cursor < start - 0x10 || cursor > end || data < start || data > end || cursor > data + 0x10)
                return 0;
            return ((cursor - (start - 0x10)) & ~0xFu) + (end - data);
        }

        // RwCameraSetFarClipPlane (FUN_0018F400: a0 camera, f12 far) from the per-frame camera setup (0x21EBB0)
        PS2_GUEST_CC void setFarClip(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (g_farClip > 0.0f && GPR_U32(ctx, 31) == 0x0021EBB8u && ctx->f[12] == 2000.0f)
                ctx->f[12] = g_farClip;
            // [Minty1804 fork] the follow-ped camera indoors: 1000 -> the outdoor far (Far view from interiors)
            const uint32_t ra = GPR_U32(ctx, 31);
            if ((ra == 0x0029DB84u || ra == 0x0029F8C0u) && ctx->f[12] == 1000.0f &&
                g_interiorFarView.load(std::memory_order_relaxed))
                ctx->f[12] = g_farClip > 0.0f ? g_farClip : 2000.0f;
            g_origSetFar(rdram, ctx, runtime);
        }

        // the projection / clip planes (FUN_002F57A0: f12 tan, f13 aspect, f14 near, f15 far = the constant 2000) from
        // RwCameraBeginUpdate (0x18F088, $s1 = the camera): the camera's own far when we moved it
        PS2_GUEST_CC void setProjection(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (GPR_U32(ctx, 31) == 0x0018F090u && rdf(rdram, GPR_U32(ctx, 17) + 0x7C) == g_farClip)
                ctx->f[15] = g_farClip;
            g_origProjection(rdram, ctx, runtime);
        }

        // Ped shadows (the user's wish, 2026-11-17: an option to turn off Vic's and the NPCs' shadows). The blob
        // texture "shad_ped" (RwTextureRead at 0x389734, kept at 0x48916C) goes to the shadow store through
        // FUN_0038A000 (StoreShadowToBeRendered: a0 type, a1 texture; CShadows::StoreShadowForPed 0x38AE30 and
        // 0x3C35A8), FUN_003898E8 (StoreStaticShadow, a1 texture; 0x1FA930) and FUN_003899B0 (the projected kind, a2
        // texture; 0x21BC50). All three return nothing the callers read, so a ped's shadow is skipped by returning
        // before the store. Cars, bikes, helis and lamps keep theirs (other textures).
        constexpr uint32_t kPedShadowTex = 0x0048916Cu;
        std::atomic<int> g_pedShadows{1};
        std::atomic<uint64_t> g_pedShadowsSkipped{0};
        // CVisibilityPlugins::SetRenderWareCamera (FUN_0036EBF8, camera a0; once a frame before the world is drawn): the
        // squared distances the vehicles' detail levels switch at - 70 / 90 / 100 m (cars: high detail, low detail, fade)
        // and 60 / 150 m (big vehicles) - scale with GenerationDistMultiplier (TheCamera+0x7D0), not with the
        // LODDistMultiplier the draw distance raises, so cars drawn far away stayed low detail (the player's report,
        // 2026-11-18). At 100% the original runs; above, this reproduction with G x the draw distance (game-internals.md
        // 6o). The rest - the camera globals, the component-cull and ped distances from LODDistMultiplier - as the game.
        PS2Runtime::RecompiledFunction g_origVisCamera = nullptr;
        PS2_GUEST_CC void visibilityCamera(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const int draw = std::clamp(g_draw.load(std::memory_order_relaxed), 100, kMaxPercent);
            if (draw == 100 || !g_orig.valid)
                return g_origVisCamera(rdram, ctx, runtime);
            const uint32_t cam = GPR_U32(ctx, 4);
            wr32(rdram, 0x00488E08u, cam);
            wr32(rdram, 0x00488E0Cu, rd32(rdram, cam + 4u) + 0x40u); // RwCameraGetFrame(cam) + 0x40
            constexpr uint32_t kCamera = 0x006F44D0u;
            const uint32_t mode = (rd32(rdram, kCamera + 0x50u) & 0xFFu) * 0x270u;
            const int16_t camMode = static_cast<int16_t>(rd32(rdram, kCamera + mode + 0x70u) & 0xFFFFu);
            const float lod = rdf(rdram, kCamera + 0x7DCu);
            const float gen = rdf(rdram, kCamera + 0x7D0u) * (draw / 100.0f);
            wrf(rdram, 0x004CD5C0u, camMode == 1 || camMode == 0x25 ? 1.0e6f : lod * (lod * 400.0f));
            const float ped = lod * (lod * 4900.0f);
            wrf(rdram, 0x004CD5D8u, ped);
            wrf(rdram, 0x004CD5CCu, ped);
            wrf(rdram, 0x004CD5BCu, gen * (gen * 4900.0f));  // car: high detail to 70 m
            wrf(rdram, 0x004CD5D0u, gen * (gen * 8100.0f));  // low detail to 90 m
            wrf(rdram, 0x004CD5D4u, gen * (gen * 10000.0f)); // faded out at 100 m
            wrf(rdram, 0x004CD5C4u, gen * (gen * 22500.0f)); // big vehicles: 150 m
            wrf(rdram, 0x004CD5C8u, gen * (gen * 3600.0f));  // and 60 m
            ctx->pc = GPR_U32(ctx, 31);
        }

        PS2Runtime::RecompiledFunction g_origStoreShadow = nullptr, g_origStoreStatic = nullptr, g_origStorePoly = nullptr;

        bool skipPedShadow(const uint8_t *rdram, R5900Context *ctx, int texReg)
        {
            if (g_pedShadows.load(std::memory_order_relaxed))
                return false;
            const uint32_t tex = rd32(rdram, kPedShadowTex);
            if (!tex || GPR_U32(ctx, texReg) != tex)
                return false;
            g_pedShadowsSkipped.fetch_add(1, std::memory_order_relaxed);
            ctx->pc = GPR_U32(ctx, 31);
            return true;
        }
        PS2_GUEST_CC void storeShadow(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (!skipPedShadow(rdram, ctx, 5))
                g_origStoreShadow(rdram, ctx, runtime);
        }
        PS2_GUEST_CC void storeStatic(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (!skipPedShadow(rdram, ctx, 5))
                g_origStoreStatic(rdram, ctx, runtime);
        }
        PS2_GUEST_CC void storePoly(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            if (!skipPedShadow(rdram, ctx, 6))
                g_origStorePoly(rdram, ctx, runtime);
        }

        settings::Spec percentSpec(const char *key, const char *label, const char *help, const char *env, std::atomic<int> &target)
        {
            settings::Spec s;
            s.key = key;
            s.page = "World"; // (each caller sets its section)
            s.section = "World";
            s.label = label;
            s.help = help;
            s.type = settings::Type::Integer;
            s.min = 100;
            s.max = kMaxPercent;
            s.format = "%d%%";
            s.defaultValue = 100;
            s.env = env;
            s.onChange = [&target](int v) { target.store(v, std::memory_order_relaxed); };
            return s;
        }
    }

    void registerSettings()
    {
        {
            settings::Spec traffic = percentSpec("enhance.traffic", "Traffic density",
                                  "How many cars drive around the city. 100% is the original game; missions that clear "
                                  "the streets still do. Costs some CPU time.",
                                  "VCS_TRAFFIC", g_traffic);
            traffic.page = "World";
            traffic.section = "Traffic and people";
            settings::add(traffic);
        }

        {
            settings::Spec peds = percentSpec("enhance.peds", "Pedestrian density",
                                  "How many people walk the streets. 100% is the original game; missions that clear "
                                  "the streets still do. Costs some CPU time.",
                                  "VCS_PEDS", g_peds);
            peds.page = "World";
            peds.section = "Traffic and people";
            settings::add(peds);
        }

        {
            settings::Spec draw = percentSpec("enhance.draw_distance", "Draw distance",
                                  "How far away people, cars, props and detailed models still show. 100% is the original "
                                  "game. Costs GPU time.",
                                  "VCS_DRAW_DISTANCE", g_draw);
            draw.page = "World";
            draw.section = "Draw distance";
            settings::add(draw);
        }

        settings::Spec s = percentSpec("enhance.ped_distance", "Pedestrian distance",
                                       "How far away people appear and are removed. 100% is the original game (they "
                                       "appear 35-60 m away). Raise the draw distance with it so they are drawn that far; "
                                       "the same number of people spreads over a wider area.",
                                       "VCS_PED_DISTANCE", g_pedDistance);
        s.page = "World";
        s.section = "Draw distance";
        s.max = kPedDistanceMax;
        settings::add(s);
        settings::Spec c = percentSpec("enhance.car_distance", "Car distance",
                                       "How far ahead traffic appears and how far away cars are removed. 100% is the "
                                       "original game (cars appear about 110 m ahead). Raise the draw distance with it so "
                                       "they are drawn that far.",
                                       "VCS_CAR_DISTANCE", g_carDistance);
        c.page = "World";
        c.section = "Draw distance";
        c.max = kCarDistanceMax;
        settings::add(c);

        settings::Spec f;
        f.key = "enhance.solid_far_palms";
        f.page = "World";
        f.section = "Details";
        f.label = "Solid far palms";
        f.help = "Far-away palms and trees stay solid. In the original game they fade out with distance, so with a "
                 "longer draw distance whole rows of them look like ghosts. Off is the original game.";
        f.type = settings::Type::Toggle;
        f.defaultValue = 0;
        f.env = "VCS_SOLID_FAR_PALMS";
        f.onChange = [](int v) { vcs::hleworld::setSolidFarFoliage(v != 0); };
        settings::add(f);

        // [Minty1804 fork] Far view from interiors: always on (the user's wish, 2026-11-20); VCS_INTERIOR_FAR_VIEW=0
        // gives the original game's 1000 m indoors back for a run.
        if (const char *e = std::getenv("VCS_INTERIOR_FAR_VIEW"); e && e[0] == '0')
            g_interiorFarView.store(0, std::memory_order_relaxed);

        settings::Spec sh;
        sh.key = "enhance.ped_shadows";
        sh.page = "World";
        sh.section = "Details";
        sh.label = "Character shadows";
        sh.help = "The round shadow under Vic and everyone else on foot. On is the original game; cars and lamps keep "
                  "their shadows either way.";
        sh.type = settings::Type::Toggle;
        sh.defaultValue = 1;
        sh.env = "VCS_PED_SHADOWS";
        sh.onChange = [](int v) { g_pedShadows.store(v, std::memory_order_relaxed); };
        settings::add(sh);
    }

    void install(PS2Runtime &runtime)
    {
        const char *env = std::getenv("VCS_WORLD_DETAIL_HOOKS");
        g_enabled = !(env && env[0] == '0');
        if (!g_enabled)
            std::fprintf(stderr, "[world] density and draw distance left alone (VCS_WORLD_DETAIL_HOOKS=0)\n");
        if (const char *keep = std::getenv("VCS_WORLD_KEEP"); keep && keep[0] == '0')
        {
            g_keep = false;
            std::fprintf(stderr, "[world] no pool reserve, no cars kept in view (VCS_WORLD_KEEP=0)\n");
        }
        std::fill(std::begin(g_nearInView), std::end(g_nearInView), -1);
        if ((g_origCarGenerators = runtime.lookupFunction(kCarGenerators)))
            runtime.replaceFunction(kCarGenerators, &carGenerators);
        if (g_enabled)
        {
            g_origCarSpawner = runtime.lookupFunction(kCarSpawner);
            g_origPedSpawner = runtime.lookupFunction(kPedSpawner);
            g_origRemoveVehicle = runtime.lookupFunction(kPossiblyRemoveVehicle);
            if (g_origCarSpawner && g_origPedSpawner && g_origRemoveVehicle)
            {
                runtime.replaceFunction(kCarSpawner, &carSpawner);
                runtime.replaceFunction(kPedSpawner, &pedSpawner);
                runtime.replaceFunction(kPossiblyRemoveVehicle, &possiblyRemoveVehicle);
                if ((g_origWorldRemove = runtime.lookupFunction(0x0018FA50u)))
                    runtime.replaceFunction(0x0018FA50u, &worldRemove);
                if ((g_origPopDistFactor = runtime.lookupFunction(kPopDistFactor)))
                    runtime.replaceFunction(kPopDistFactor, &popDistFactor);
                if ((g_origRemoveCarsIfPoolFull = runtime.lookupFunction(kRemoveCarsIfPoolFull)))
                    runtime.replaceFunction(kRemoveCarsIfPoolFull, &removeCarsIfPoolFull);
                if ((g_origVisCamera = runtime.lookupFunction(0x0036EBF8u)))
                    runtime.replaceFunction(0x0036EBF8u, &visibilityCamera);
            }
            else
            {
                std::fprintf(stderr, "[world] the spawners / car removal not found: density and draw distance left alone\n");
                g_enabled = false;
            }
        }

        // the ped shadow hooks pass everything through unless the option is off
        g_origStoreShadow = runtime.lookupFunction(0x0038A000u);
        g_origStoreStatic = runtime.lookupFunction(0x003898E8u);
        g_origStorePoly = runtime.lookupFunction(0x003899B0u);
        if (g_origStoreShadow && g_origStoreStatic && g_origStorePoly)
        {
            runtime.replaceFunction(0x0038A000u, &storeShadow);
            runtime.replaceFunction(0x003898E8u, &storeStatic);
            runtime.replaceFunction(0x003899B0u, &storePoly);
        }
        else
            std::fprintf(stderr, "[world] the shadow store not found: character shadows stay on\n");

        // RwCameraSetFarClipPlane: the follow camera's indoor far (Far view from interiors) and VCS_FAR_CLIP; passes
        // everything through while both are off
        if (g_enabled && (g_origSetFar = runtime.lookupFunction(0x0018F400u)))
            runtime.replaceFunction(0x0018F400u, &setFarClip);
        else
            std::fprintf(stderr, "[world] the far plane's setter not found: no far view from interiors\n");
        if (const char *far = std::getenv("VCS_FAR_CLIP"); g_enabled && far && *far)
        {
            const float metres = static_cast<float>(std::atof(far));
            g_origProjection = runtime.lookupFunction(0x002F57A0u);
            if (metres > 2000.0f && metres <= 20000.0f && g_origSetFar && g_origProjection)
            {
                g_farClip = metres;
                runtime.replaceFunction(0x002F57A0u, &setProjection);
                std::fprintf(stderr, "[world] far plane and fog end at %.0f instead of 2000 (VCS_FAR_CLIP)\n", metres);
            }
            else
                std::fprintf(stderr, "[world] VCS_FAR_CLIP=%s ignored (2000 < metres <= 20000)\n", far);
        }

        // worlddetail -> ok enabled valid traffic peds draw carcap pedcap=out/in lod pedfree vehfree pedsused vehused
        //   minpedfree minvehfree (since the last query) kept (cars kept in view, total) playerinview packets peak far
        //   carchance pedchance
        ps2control::addCommand("worlddetail", [](const std::string &, ps2control::Reply reply) {
            char text[640];
            std::snprintf(text, sizeof(text),
                          "ok enabled=%d valid=%d traffic=%d peds=%d draw=%d carcap=%d pedcap=%d/%d lod=%.3f pedfree=%d "
                          "vehfree=%d pedsused=%d vehused=%d minpedfree=%u minvehfree=%u kept=%llu vanished=%llu playerinview=%d "
                          "packets=%u peak=%u far=%.0f carchance=%08x pedchance=%08x",
                          g_enabled ? 1 : 0, g_orig.valid ? 1 : 0, g_traffic.load(), g_peds.load(), g_draw.load(),
                          g_stCarCap.load(), g_stPedCapOut.load(), g_stPedCapIn.load(), g_stLod.load(), g_stPedFree.load(),
                          g_stVehicleFree.load(), g_stPedsUsed.load(), g_stVehiclesUsed.load(), g_stMinPedFree.exchange(999),
                          g_stMinVehicleFree.exchange(999), (unsigned long long)g_stKeptInView.load(),
                          (unsigned long long)g_stVanished.load(),                          g_stPlayerInView.load(), g_stPacketUsed.load(), g_stPacketPeak.exchange(0),
                          g_farClip > 0 ? g_farClip : 2000.0f, g_stCarChance.load(), g_stPedChance.load());
            const size_t n = std::strlen(text);
            std::snprintf(text + n, sizeof(text) - n,
                          " peddist=%d peds=%d pedmean=%.1f pedfar=%.1f cardist=%d cars=%d carmean=%.1f carfar=%.1f foliage=%llu "
                          "shadows=%d shadowsskipped=%llu carspawns=%llu carspawnmean=%.1f",
                          g_pedDistance.load(), g_stPedCount.load(), g_stPedMean.load(), g_stPedFar.load(), g_carDistance.load(),
                          g_stCarCount.load(), g_stCarMean.load(), g_stCarFar.load(),
                          (unsigned long long)vcs::hleworld::solidFarFoliageRaised(), g_pedShadows.load(),
                          (unsigned long long)g_pedShadowsSkipped.load(), (unsigned long long)g_stSpawnCount.load(),
                          g_stSpawnCount.load() ? double(g_stSpawnSum.load()) / double(g_stSpawnCount.load()) : 0.0);
            reply(text);
        });
    }

    void onGameFrame(uint8_t *rdram)
    {
        if (!g_enabled)
            return;
        if (!g_orig.read)
            readOriginals(rdram);
        if (!g_orig.valid)
            return;

        // ---- density: factors and chance tables; the caps here for FUN_0043BE00 (and the command), the spawners set
        // theirs again right before they run
        const int traffic = std::clamp(g_traffic.load(std::memory_order_relaxed), 100, kMaxPercent);
        const int peds = std::clamp(g_peds.load(std::memory_order_relaxed), 100, kMaxPercent);
        wrf(rdram, kCarFactor, traffic == 100 ? g_orig.carFactor : g_orig.carFactor * (traffic / 100.0f));
        wrf(rdram, kPedFactor, peds == 100 ? g_orig.pedFactor : g_orig.pedFactor * (peds / 100.0f));
        g_carChance = scaledChance(g_orig.spawnChance, traffic);
        g_pedChance = scaledChance(g_orig.spawnChance, peds);
        writeCarCap(rdram);
        writePedCaps(rdram);

        // ---- draw distance
        const int draw = std::clamp(g_draw.load(std::memory_order_relaxed), 100, kMaxPercent);
        const float lod = draw == 100 ? g_orig.lodScale : draw / 100.0f;
        wrf(rdram, kLodScale, lod);

        // ---- state for the command
        int pedsUsed = -1, vehiclesUsed = -1;
        g_stPedFree.store(poolFree(rdram, kPedPool, &pedsUsed), std::memory_order_relaxed);
        g_stVehicleFree.store(poolFree(rdram, kVehiclePool, &vehiclesUsed), std::memory_order_relaxed);
        g_stPedsUsed.store(pedsUsed, std::memory_order_relaxed);
        g_stVehiclesUsed.store(vehiclesUsed, std::memory_order_relaxed);
        g_stCarChance.store(g_carChance, std::memory_order_relaxed);
        g_stPedChance.store(g_pedChance, std::memory_order_relaxed);
        g_stLod.store(lod, std::memory_order_relaxed);
        {
            int count;
            float mean, farthest;
            pedDistances(rdram, count, mean, farthest);
            g_stPedCount.store(count, std::memory_order_relaxed);
            g_stPedMean.store(mean, std::memory_order_relaxed);
            g_stPedFar.store(farthest, std::memory_order_relaxed);
            pedDistances(rdram, count, mean, farthest, true);
            g_stCarCount.store(count, std::memory_order_relaxed);
            g_stCarMean.store(mean, std::memory_order_relaxed);
            g_stCarFar.store(farthest, std::memory_order_relaxed);
        }
        if (const uint32_t player = rd32(rdram, 0x004E4910u) & PS2_RAM_MASK)
        {
            const float p[3] = {rdf(rdram, player + 0x30), rdf(rdram, player + 0x34), rdf(rdram, player + 0x38)};
            g_stPlayerInView.store(inView(rdram, p, 0.5f) ? 1 : 0, std::memory_order_relaxed);
        }
        // ---- the vanish detector: a car in view last frame, within the distance cars are drawn at, whose slot is free
        // or reused now (not across a camera cut or teleport: the camera moving more than 30 m in a frame)
        if (const uint32_t pool = rd32(rdram, kVehiclePool) & PS2_RAM_MASK)
        {
            static float lastCam[2] = {0, 0};
            const float cam[2] = {rdf(rdram, kCamera + 0x30), rdf(rdram, kCamera + 0x34)};
            const bool cut = std::hypot(cam[0] - lastCam[0], cam[1] - lastCam[1]) > 30.0f;
            lastCam[0] = cam[0];
            lastCam[1] = cam[1];
            const float drawn = 160.0f * rdf(rdram, kGenerationDist) * std::max(1.0f, draw / 100.0f);
            const uint32_t entries = rd32(rdram, pool) & PS2_RAM_MASK, flags = rd32(rdram, pool + 4) & PS2_RAM_MASK;
            const int size = std::min(static_cast<int>(rd32(rdram, pool + 8)), 256);
            for (int i = 0; i < size && entries && flags; ++i)
            {
                const int flag = rdram[flags + i];
                if (g_nearInView[i] >= 0 && g_nearInView[i] != flag && !cut)
                {
                    g_stVanished.fetch_add(1, std::memory_order_relaxed);
                    std::fprintf(stderr,
                                 "[world] car vanished in view (slot %d, %.0f m, removed from 0x%x; traffic %d%%, draw %d%%, "
                                 "vehicles free %d)\n",
                                 i, g_nearDist[i], g_removedBy[i], traffic, draw, poolFree(rdram, kVehiclePool));
                }
                g_nearInView[i] = -1;
                static int lastFlag[256];
                static bool lastFlagInit = false;
                if (!lastFlagInit)
                {
                    std::fill(std::begin(lastFlag), std::end(lastFlag), -2);
                    lastFlagInit = true;
                }
                const int before = lastFlag[i];
                lastFlag[i] = flag;
                if (flag & 0x80)
                    continue;
                const uint32_t car = entries + static_cast<uint32_t>(i) * 0x8C0u;
                const float p[3] = {rdf(rdram, car + 0x30), rdf(rdram, car + 0x34), rdf(rdram, car + 0x38)};
                const float dist = std::hypot(p[0] - cam[0], p[1] - cam[1]);
                if (before != -2 && before != flag && !cut) // a car made this frame: how far away (the command's spawn=)
                {
                    g_stSpawnSum.fetch_add(static_cast<uint64_t>(dist), std::memory_order_relaxed);
                    g_stSpawnCount.fetch_add(1, std::memory_order_relaxed);
                }
                if (dist < drawn && inView(rdram, p, 1.0f))
                {
                    g_nearInView[i] = flag;
                    g_nearDist[i] = dist;
                }
            }
        }
        const uint32_t used = packetUse(rdram);
        g_stPacketUsed.store(used, std::memory_order_relaxed);
        if (used > g_stPacketPeak.load(std::memory_order_relaxed))
            g_stPacketPeak.store(used, std::memory_order_relaxed);
    }
}
