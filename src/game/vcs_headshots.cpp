// [Minty1804 fork] Enhancements > Combat: a gunshot to the head kills (docs/game-internals.md 12c "Headshots").
//
// CPed::InflictDamage is FUN_001D7940: a0 the ped hit, a1 the one who did it, a2 the weapon type, f12 the damage,
// a3 the body piece, t0 the direction. Its switch on the piece (the jump table at 0x4A9C70, pieces 0 - 6) sends piece 6
// to 0x1D7F48, which plays the hit on frame 2 (the head) and sets its own headshot flag ($fp); pieces 2 - 5 are the
// arms and legs (frames 3, 4, 7, 8). A bullet's damage is the weapon's own (weapon info +0x18; x4 up to 255 with the
// player's damage bonus), so the original game's pistol takes a quarter of a man's health whatever it hits.
//
// On: when the player fired (a1 his ped), the piece is the head, the weapon an instant-hit gun (weapon info +4 == 1:
// pistols, SMGs, shotguns, rifles, the sniper, the M60 and minigun; not fists, blades, explosions, fire or cars) and the
// ped hit not the player, the damage becomes enough to take all of his health and armour. Everything else in InflictDamage runs as before: the
// checks that make a ped proof against bullets (mission characters, peds a script protects) come before the damage is
// taken, so they still hold; the death, its animation and the game's own head effects are the game's.
//
// Other shooters' headshots stay the original game's: an enemy's bullet would otherwise kill a mission's ally (an escort)
// outright. VCS_HEADSHOT_HOOKS=0 hooks nothing. `headshot` -> ok on=.. hits=<the player's hits by piece 0..6> raised=..
// (since the start).
#include "game/vcs_headshots.h"

#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "runtime/ps2_control.h"
#include "settings/settings.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace vcs::headshots
{
    namespace
    {
        constexpr uint32_t kInflictDamage = 0x001D7940u; // CPed::InflictDamage
        constexpr uint32_t kPlayerPed = 0x004E4910u;     // CWorld::Players[0].m_pPed
        constexpr uint32_t kWeaponInfo = 0x00712770u;    // *(*this + 4) + type * 0x70; +4 the fire type (1 instant hit)
        constexpr uint32_t kWeaponTypes = 44u;          // the table's rows: types 0 - 43 (fire types 0 - 7); after it, other data
        constexpr uint32_t kPedHealth = 0x4E4u, kPedArmour = 0x4E8u;
        constexpr uint32_t kHead = 6u;

        std::atomic<int> g_on{1};
        bool g_installed = false;
        PS2Runtime::RecompiledFunction g_orig = nullptr;
        std::atomic<uint64_t> g_hits[8], g_raised{0};

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

        bool instantHit(const uint8_t *ram, uint32_t type)
        {
            if (type >= kWeaponTypes) // InflictDamage also takes damage kinds past the table (0x2E, 0x2F)
                return false;
            const uint32_t table = rd32(ram, kWeaponInfo) & PS2_RAM_MASK;
            const uint32_t info = table ? rd32(ram, table + 4) & PS2_RAM_MASK : 0;
            return info && rd32(ram, info + type * 0x70u + 4u) == 1u;
        }

        PS2_GUEST_CC void inflictDamage(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            const uint32_t ped = GPR_U32(ctx, 4), by = GPR_U32(ctx, 5), type = GPR_U32(ctx, 6), piece = GPR_U32(ctx, 7);
            const uint32_t player = rd32(rdram, kPlayerPed);
            if (piece < 8 && by == player)
                g_hits[piece].fetch_add(1, std::memory_order_relaxed);
            if (piece == kHead && ped && by && by == player && ped != player && g_on.load(std::memory_order_relaxed) &&
                instantHit(rdram, type))
            {
                // all his health and armour, four times over: InflictDamage quarters the damage in some states
                // (FUN_00199B68), and the armour is taken first
                const float health = rdf(rdram, ped + kPedHealth), armour = rdf(rdram, ped + kPedArmour);
                const float lethal = (std::max(health, 0.0f) + std::max(armour, 0.0f)) * 4.0f + 100.0f;
                if (ctx->f[12] < lethal)
                {
                    ctx->f[12] = lethal;
                    g_raised.fetch_add(1, std::memory_order_relaxed);
                }
            }
            g_orig(rdram, ctx, runtime);
        }
    }

    void registerSettings()
    {
        settings::Spec s;
        s.key = "enhance.headshots_kill";
        s.page = "Enhancements";
        s.section = "Combat";
        s.label = "Headshots always kill";
        s.help = "Your bullet to the head kills, from any gun - a pistol too. Other hits do the damage they "
                 "always did. Off is the original game, where most guns need several shots to the head.";
        s.type = settings::Type::Toggle;
        s.defaultValue = 1;
        s.env = "VCS_HEADSHOTS_KILL";
        s.onChange = [](int v) { g_on.store(v, std::memory_order_relaxed); };
        settings::add(s);
    }

    void install(PS2Runtime &runtime)
    {
        if (const char *env = std::getenv("VCS_HEADSHOT_HOOKS"); env && env[0] == '0')
            std::fprintf(stderr, "[headshot] InflictDamage left alone (VCS_HEADSHOT_HOOKS=0)\n");
        else if ((g_orig = runtime.lookupFunction(kInflictDamage)))
        {
            runtime.replaceFunction(kInflictDamage, &inflictDamage);
            g_installed = true;
        }
        else
            std::fprintf(stderr, "[headshot] CPed::InflictDamage not found: headshots as in the original game\n");

        // headshot -> ok hooked=.. on=.. hits=p0,p1,..,p6 raised=N (InflictDamage calls by piece, the damage raised)
        ps2control::addCommand("headshot", [](const std::string &, ps2control::Reply reply) {
            std::string hits;
            for (int i = 0; i < 7; ++i)
                hits += (i ? "," : "") + std::to_string(g_hits[i].load());
            char text[192];
            std::snprintf(text, sizeof(text), "ok hooked=%d on=%d hits=%s raised=%llu", g_installed ? 1 : 0,
                          g_on.load(), hits.c_str(), (unsigned long long)g_raised.load());
            reply(text);
        });
    }
}
