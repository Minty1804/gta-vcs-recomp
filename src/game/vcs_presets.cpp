// [Minty1804 fork] Graphics presets by what the PC can run (game/vcs_presets.h).
#include "game/vcs_presets.h"

#include "platform/platform.h"
#include "rhi/rhi.h"
#include "settings/settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace vcs::presets
{
    namespace
    {
        using Values = std::vector<std::pair<std::string, int>>;

        // The settings a preset sets: what costs the graphics card (resolution, anti-aliasing, filtering) and the game
        // thread (the detailed city, draw / car / pedestrian distance, density), and the streaming memory they need.
        // willzmkv's presets in this fork's settings and ranges (theirs are launcher sliders of 0-100 over other
        // scales): Low a step past the PS2, High what a card of 6 GB and a processor of 12 threads hold with room to
        // spare. The display's shape and timing are Balanced's, not these (and fog stays the player's).
        // display.scale 0 is Match window (the window's own resolution).
        const Values kLow = {{"display.scale", 2},          {"display.msaa", 1},        {"display.aniso", 4},
                             {"world.hd_radius", 0},        {"memory.streaming", 12},   {"enhance.draw_distance", 100},
                             {"enhance.car_distance", 100}, {"enhance.ped_distance", 100}, {"enhance.traffic", 100},
                             {"enhance.peds", 100}};
        const Values kMedium = {{"display.scale", 0},          {"display.msaa", 2},        {"display.aniso", 8},
                                {"world.hd_radius", 1},        {"memory.streaming", 20},   {"enhance.draw_distance", 150},
                                {"enhance.car_distance", 125}, {"enhance.ped_distance", 125}, {"enhance.traffic", 100},
                                {"enhance.peds", 100}};
        const Values kHigh = {{"display.scale", 0},          {"display.msaa", 4},        {"display.aniso", 16},
                              {"world.hd_radius", 2},        {"memory.streaming", 30},   {"enhance.draw_distance", 200},
                              {"enhance.car_distance", 150}, {"enhance.ped_distance", 150}, {"enhance.traffic", 125},
                              {"enhance.peds", 125}};
        const Values kUltra = {{"display.scale", 0},          {"display.msaa", 8},        {"display.aniso", 16},
                               {"world.hd_radius", 3},        {"memory.streaming", 30},   {"enhance.draw_distance", 300},
                               {"enhance.car_distance", 200}, {"enhance.ped_distance", 200}, {"enhance.traffic", 150},
                               {"enhance.peds", 150}};

        const Values *values(int tier)
        {
            switch (tier)
            {
            case 1: return &kLow;
            case 2: return &kMedium;
            case 3: return &kHigh;
            case 4: return &kUltra;
            default: return nullptr; // Original: the settings as they are
            }
        }

        void apply(int tier)
        {
            const Values *v = values(tier);
            if (!v)
                return;
            for (const auto &[key, value] : *v)
                if (!settings::set(key, value))
                    std::fprintf(stderr, "[settings] preset %s: %s=%d refused\n", tierName(tier), key.c_str(), value);
            std::fprintf(stderr, "[settings] preset %s\n", tierName(tier));
        }

        void applyForThisPc()
        {
            std::string why;
            const int tier = recommend(probe(), why);
            std::fprintf(stderr, "[settings] for this PC: %s (%s)\n", tierName(tier), why.c_str());
            apply(tier == 0 ? 1 : tier); // the button always changes something: Original is Authentic's
        }
    }

    const char *tierName(int tier)
    {
        static const char *const kNames[] = {"Original", "Low", "Medium", "High", "Ultra"};
        return tier >= 0 && tier <= 4 ? kNames[tier] : "?";
    }

    Hardware probe()
    {
        Hardware hw;
        const std::vector<rhi::Gpu> gpus = rhi::listGpus(); // the fastest first
        const int chosen = settings::value("display.gpu");
        const rhi::Gpu *g = gpus.empty() ? nullptr : &gpus[0];
        for (const rhi::Gpu &c : gpus)
            if (c.id == chosen)
                g = &c;
        if (g)
        {
            hw.gpuName = g->name;
            hw.gpuMb = g->memory >> 20;
            // an integrated chip: DXCore / the device type says so; one that doesn't say has a small carve-out or none
            hw.integrated = g->integrated || (g->memory != 0 && hw.gpuMb < 512);
        }
        hw.threads = platform::logicalProcessors();
        hw.avx2 = platform::cpuHasAvx2();
        hw.ramMb = platform::systemMemoryMb();
        return hw;
    }

    int recommend(const Hardware &hw, std::string &why)
    {
        // each part's ceiling: 0 Original, 1 Low, 2 Medium, 3 High (willzmkv's 9c01e8f). The card decides the
        // resolution and anti-aliasing; the processor the detailed city and the population (the game thread); the
        // memory the streaming and the texture packs. Each one step below what the part could just about hold.
        int gpu = 2; // no card listed, or its memory unknown: Medium
        if (!hw.gpuName.empty() && (hw.gpuMb || hw.integrated))
            gpu = hw.integrated ? 1 : hw.gpuMb < 3072 ? 1 : hw.gpuMb < 6144 ? 2 : 3;
        const int cpu = hw.threads >= 12 && hw.avx2 ? 3 : hw.threads >= 8 ? 2 : hw.threads >= 4 ? 1 : 0;
        // 8 GB reports a little under 8192, so the thresholds sit below the sizes they mean
        const int ram = hw.ramMb <= 0 ? 2 : hw.ramMb < 3500 ? 0 : hw.ramMb < 7000 ? 1 : hw.ramMb < 15000 ? 2 : 3;
        why = hw.gpuName.empty() ? std::string("card unknown") : hw.gpuName;
        if (hw.integrated)
            why += " (integrated)";
        else if (hw.gpuMb)
            why += " (" + std::to_string((hw.gpuMb + 512) >> 10) + " GB)";
        why += ", " + std::to_string(hw.threads) + " threads" + (hw.avx2 ? " (AVX2)" : "") + ", " +
               std::to_string((hw.ramMb + 512) >> 10) + " GB RAM";
        return std::min({gpu, cpu, ram});
    }

    void registerSettings()
    {
        settings::Spec s;
        s.key = "presets.this_pc";
        s.page = "Display";
        s.section = "Presets";
        s.label = "For this PC";
        s.help = "The preset this PC runs with room to spare, from its graphics card, processor and memory (Low, Medium "
                 "or High; never Ultra). A first start picks it by itself. Resolution, anti-aliasing, filtering and "
                 "streaming memory apply after a restart.";
        s.type = settings::Type::Action;
        s.button = "Apply";
        s.onChange = [](int) { applyForThisPc(); };
        settings::add(s);

        struct Row
        {
            const char *key, *label, *help;
            int tier;
        };
        static const Row kRows[] = {
            {"presets.low", "Low",
             "Twice the original resolution, no anti-aliasing, 4x texture filtering; the original city detail, distances "
             "and density; 12 MB streaming memory.",
             1},
            {"presets.medium", "Medium",
             "The window's resolution, 2x anti-aliasing, 8x filtering; the detailed city to ~450 m, draw distance 150%, "
             "cars and people 125% as far; 20 MB streaming memory.",
             2},
            {"presets.high", "High",
             "The window's resolution, 4x anti-aliasing, 16x filtering; the detailed city to ~580 m, draw distance "
             "200%, cars and people 150% as far, 125% density; 30 MB streaming memory.",
             3},
            {"presets.ultra", "Ultra",
             "The window's resolution, 8x anti-aliasing, 16x filtering; the detailed city to ~720 m, draw distance "
             "300%, cars and people twice as far, 150% density; 30 MB streaming memory. For a fast PC.",
             4},
        };
        for (const Row &r : kRows)
        {
            s = {};
            s.key = r.key;
            s.page = "Display";
            s.section = "Presets";
            s.label = r.label;
            s.help = std::string(r.help) + " Resolution, anti-aliasing, filtering and streaming memory apply after a restart.";
            s.type = settings::Type::Action;
            s.button = "Apply";
            const int tier = r.tier;
            s.onChange = [tier](int) { apply(tier); };
            settings::add(s);
        }
    }

    void firstStart(bool cardHasSaves)
    {
        if (const char *e = std::getenv("VCS_AUTO_PRESET"); e && e[0] == '0')
            return;
        if (!settings::state("presets.first_start").empty())
            return;
        if (!settings::fileHoldsOnly({"iso"}) || cardHasSaves)
        {
            // a player's own install (settings of theirs, or saves): what they have stays; no pick from now on
            std::fprintf(stderr, "[settings] first start: not a new install (%s), settings kept\n",
                         cardHasSaves ? "saves on the card" : "vcs.ini holds settings");
            settings::setState({{"presets.first_start", "kept"}});
            return;
        }
        std::string why;
        const int tier = recommend(probe(), why);
        std::fprintf(stderr, "[settings] first start: preset %s for this PC (%s)\n", tierName(tier), why.c_str());
        if (const Values *v = values(tier))
            settings::startWith(*v);
        settings::setState({{"presets.first_start", tierName(tier)}});
    }
}
