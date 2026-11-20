// [Minty1804 fork] Who calls a game function: VCS_TRACE_CALLERS=0xADDR[,0xADDR...] (up to 8) hooks each listed function
// and counts its calls by caller ($ra) and first argument (a0). The rig's `callers` command prints the counts
// (`callers reset` clears them), so a draw seen in F12's draw log can be traced to the code that emits it: start the
// scene, `callers reset`, wait a few frames, `callers`. The hook only counts and runs the original (DECISIONS.md: hooks on
// recompiled functions). Nothing is hooked without the variable.
#include "debug/call_trace.h"

#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "runtime/ps2_control.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace vcs::calltrace
{
    namespace
    {
        constexpr int kSlots = 8;
        uint32_t g_address[kSlots] = {};
        PS2Runtime::RecompiledFunction g_orig[kSlots] = {};
        std::mutex g_lock;
        std::map<std::pair<uint32_t, uint32_t>, uint64_t> g_calls[kSlots]; // (ra, a0) -> calls

        template <int I>
        PS2_GUEST_CC void traced(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
        {
            {
                std::lock_guard<std::mutex> lock(g_lock);
                ++g_calls[I][{GPR_U32(ctx, 31), GPR_U32(ctx, 4)}];
            }
            g_orig[I](rdram, ctx, runtime);
        }

        template <int... I>
        constexpr PS2Runtime::RecompiledFunction hookFor(int i, std::integer_sequence<int, I...>)
        {
            constexpr PS2Runtime::RecompiledFunction table[] = {&traced<I>...};
            return table[i];
        }
    }

    void install(PS2Runtime &runtime)
    {
        const char *env = std::getenv("VCS_TRACE_CALLERS");
        if (!env || !*env)
            return;
        std::string list = env;
        int n = 0;
        for (size_t pos = 0; pos < list.size() && n < kSlots;)
        {
            const size_t end = list.find(',', pos);
            const std::string item = list.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            pos = end == std::string::npos ? list.size() : end + 1;
            const uint32_t address = static_cast<uint32_t>(std::strtoul(item.c_str(), nullptr, 16));
            PS2Runtime::RecompiledFunction orig = address ? runtime.lookupFunction(address) : nullptr;
            if (!orig)
            {
                std::fprintf(stderr, "[calltrace] 0x%08x: no function there\n", address);
                continue;
            }
            g_address[n] = address;
            g_orig[n] = orig;
            runtime.replaceFunction(address, hookFor(n, std::make_integer_sequence<int, kSlots>{}));
            std::fprintf(stderr, "[calltrace] counting the callers of 0x%08x\n", address);
            ++n;
        }

        // callers [reset] -> ok <function>: <ra>/<a0> x<calls> ... (most calls first, 12 per function)
        ps2control::addCommand("callers", [](const std::string &args, ps2control::Reply reply) {
            std::lock_guard<std::mutex> lock(g_lock);
            if (args.find("reset") != std::string::npos)
            {
                for (auto &m : g_calls)
                    m.clear();
                reply("ok reset");
                return;
            }
            std::string text = "ok";
            char item[96];
            for (int i = 0; i < kSlots && g_address[i]; ++i)
            {
                std::multimap<uint64_t, std::pair<uint32_t, uint32_t>, std::greater<uint64_t>> byCount;
                for (const auto &[key, count] : g_calls[i])
                    byCount.emplace(count, key);
                std::snprintf(item, sizeof(item), " | %08x:", g_address[i]);
                text += item;
                int shown = 0;
                for (const auto &[count, key] : byCount)
                {
                    if (++shown > 12)
                        break;
                    std::snprintf(item, sizeof(item), " %08x/%x x%llu", key.first, key.second, (unsigned long long)count);
                    text += item;
                }
            }
            reply(text);
        });
    }
}
