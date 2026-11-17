// Graphics > Texture packs: replacement textures in PCSX2's format (docs/game-internals.md 7b).
//
// PCSX2 names a replacement after the texture it replaces: <TEX0 hash>-<bits>.png for direct colour,
// <TEX0 hash>-<CLUT hash>-<bits>.png for paletted textures, the hashes XXH3-64 of the texture's bytes and its palette
// (GSCpuBackend::TexturePackInput gathers them as GSTextureCache::HashTextureLevel does) and the bits TEX0.PSM / TW / TH
// and, for 16 / 24-bit direct colour, TEXA. So the packs made for the PS2 game under PCSX2 work as they are: a pack's
// folder goes into <game folder>/textures/SLUS-21590/replacements (sub-folders allowed), as in PCSX2's own textures
// folder. Dumping writes the textures the game draws to textures/SLUS-21590/dumps, named the same way, for making packs.
//
// The renderer asks here for every texture it makes from VRAM (gs_hw.h GSHwTexturePack, on the GS stage). Images load
// on two worker threads; the renderer draws the original until its image is ready, then swaps it in. Decoded images are
// kept up to VCS_TEXPACK_CACHE_MB (1024) for a texture that is made again; the GPU keeps its own copy while in use.
//
// Not supported (yet): DDS images (PCSX2 packs may hold BC1-BC7 .dds; convert them to PNG), region-clamped textures'
// "-r<w>x<h>-" names and "-mip<n>" levels (counted at start). VCS_TEXPACK=0: nothing hashed or replaced (the original).
#include "game/vcs_texture_pack.h"

#include "platform/platform.h"
#include "runtime/gs/gs_hw.h"
#include "runtime/ps2_control.h"
#include "settings/settings.h"

#define XXH_INLINE_ALL
#include "xxhash.h"

#include <atomic>
#include <cctype>
#include <cinttypes>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace vcs::texpack
{
    namespace
    {
        constexpr const char *kSerial = "SLUS-21590";

        std::atomic<bool> g_enabled{true};
        std::atomic<bool> g_dumpSetting{false};

        struct Key
        {
            uint64_t tex0 = 0, clut = 0;
            uint32_t bits = 0;
            bool operator==(const Key &o) const { return tex0 == o.tex0 && clut == o.clut && bits == o.bits; }
        };
        struct KeyHash
        {
            size_t operator()(const Key &k) const
            {
                return static_cast<size_t>(k.tex0 ^ (k.clut * 0x9E3779B97F4A7C15ull) ^ (uint64_t(k.bits) << 21));
            }
        };

        std::string utf8(const fs::path &p)
        {
            const std::u8string s = p.u8string();
            return std::string(s.begin(), s.end());
        }

        // PCSX2's TextureName bits: PSM:6 TW:4 TH:4 (bit 14 was TCC, unused) TA0:8 AEM:1 TA1:8
        uint32_t makeBits(const GSHwTextureInfo &i)
        {
            return (i.psm & 0x3Fu) | ((i.tw & 0xFu) << 6) | ((i.th & 0xFu) << 10) | ((i.ta0 & 0xFFu) << 15) |
                   ((i.aem & 1u) << 23) | ((i.ta1 & 0xFFu) << 24);
        }

        bool endsWith(const std::string &s, const char *suffix)
        {
            const size_t n = std::strlen(suffix);
            if (s.size() < n)
                return false;
            for (size_t i = 0; i < n; ++i)
                if (std::tolower(static_cast<unsigned char>(s[s.size() - n + i])) != suffix[i])
                    return false;
            return true;
        }

        // PCSX2's GSTextureReplacements::ParseReplacementName for the names without a region or mip level
        bool parseName(const std::string &name, Key &key)
        {
            char dot = 0;
            uint64_t a = 0, b = 0;
            uint32_t bits = 0;
            if (std::sscanf(name.c_str(), "%" SCNx64 "-%" SCNx64 "-%08x%c", &a, &b, &bits, &dot) == 4 && dot == '.')
            {
                key = {a, b, bits & ~(1u << 14)};
                return true;
            }
            dot = 0;
            if (std::sscanf(name.c_str(), "%" SCNx64 "-%08x%c", &a, &bits, &dot) == 3 && dot == '.')
            {
                key = {a, 0u, bits & ~(1u << 14)};
                return true;
            }
            return false;
        }

        std::string nameOf(const Key &k)
        {
            char text[64];
            if (k.clut)
                std::snprintf(text, sizeof(text), "%" PRIx64 "-%" PRIx64 "-%08x.png", k.tex0, k.clut, k.bits);
            else
                std::snprintf(text, sizeof(text), "%" PRIx64 "-%08x.png", k.tex0, k.bits);
            return text;
        }

        class Pack final : public GSHwTexturePack
        {
        public:
            void open(const fs::path &root, bool replace, bool dump)
            {
                m_root = root;
                m_dumpDir = root / "dumps";
                if (const char *e = std::getenv("VCS_TEXPACK_CACHE_MB"); e && *e)
                    m_cacheLimit = static_cast<size_t>(std::strtoull(e, nullptr, 10)) << 20;
                if (replace)
                    index(root / "replacements");
                if (dump)
                {
                    std::error_code ec;
                    fs::create_directories(m_dumpDir, ec);
                    if (ec)
                        std::fprintf(stderr, "[texpack] can't make the dump folder %s: %s\n", utf8(m_dumpDir).c_str(),
                                     ec.message().c_str());
                    else
                    {
                        m_dumping = true;
                        // already dumped files are left as they are (PCSX2 too): a dump grows over sessions
                        for (const auto &e : fs::directory_iterator(m_dumpDir, ec))
                        {
                            Key k;
                            if (e.is_regular_file(ec) && parseName(utf8(e.path().filename()), k))
                                m_dumped.insert(k);
                        }
                        std::fprintf(stderr, "[texpack] dumping new textures to %s (%zu there already)\n",
                                     utf8(m_dumpDir).c_str(), m_dumped.size());
                    }
                }
                m_wants = !m_entries.empty() || m_dumping;
                for (int i = 0; i < 2 && !m_entries.empty(); ++i)
                    m_loaders.emplace_back([this] { loaderLoop(); });
                if (m_dumping)
                    m_writer = std::thread([this] { writerLoop(); });
            }

            ~Pack() override
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_stop = true;
                }
                m_cv.notify_all();
                m_writeCv.notify_all();
                for (std::thread &t : m_loaders)
                    t.join();
                if (m_writer.joinable())
                    m_writer.join();
            }

            bool wantsTextures() const override { return m_wants; }
            bool dumping() const override { return m_dumping; }

            uint64_t lookup(const GSHwTextureInfo &info, std::shared_ptr<const GSHwReplacement> &ready) override
            {
                Key key;
                key.tex0 = XXH3_64bits(info.data, info.bytes);
                key.clut = info.clutEntries ? XXH3_64bits(info.clut, size_t(info.clutEntries) * 4u) : 0u;
                key.bits = makeBits(info);
                m_hashed.fetch_add(1, std::memory_order_relaxed);
                std::unique_lock<std::mutex> lock(m_mutex);
                auto it = m_byKey.find(key);
                if (it == m_byKey.end() && key.clut)
                    it = m_byKey.find(Key{key.tex0, 0u, key.bits}); // a pack image for every palette (PCSX2 allows it)
                if (it == m_byKey.end())
                {
                    if (m_dumping && info.pixels && info.width && info.height && m_dumped.insert(key).second)
                    {
                        WriteJob job{nameOf(key), info.width, info.height,
                                     std::vector<uint32_t>(info.pixels, info.pixels + size_t(info.width) * info.height)};
                        m_writes.push_back(std::move(job));
                        m_writeCv.notify_one();
                    }
                    return 0;
                }
                Entry &e = m_entries[it->second];
                if (e.failed)
                    return 0; // unreadable: the original
                e.lastUse = ++m_useClock;
                if (e.image)
                {
                    ready = e.image;
                    return it->second + 1u;
                }
                if (!e.queued && !e.failed)
                {
                    e.queued = true;
                    m_loads.push_back(it->second);
                    m_cv.notify_one();
                }
                return it->second + 1u;
            }

            bool poll(uint64_t handle, std::shared_ptr<const GSHwReplacement> &image) override
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!handle || handle > m_entries.size())
                    return true;
                Entry &e = m_entries[handle - 1u];
                if (e.failed)
                    return true;
                if (!e.image)
                {
                    if (!e.queued) // dropped from the cache before it was taken: load it again
                    {
                        e.queued = true;
                        m_loads.push_back(static_cast<size_t>(handle - 1u));
                        m_cv.notify_one();
                    }
                    return false;
                }
                e.lastUse = ++m_useClock;
                image = e.image;
                return true;
            }

            std::string status()
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                uint64_t hashed = 0, replaced = 0;
                gsHwTexturePackStats(hashed, replaced);
                char text[256];
                std::snprintf(text, sizeof(text),
                              "ok indexed=%zu hashed=%llu/%llu replaced=%llu loaded=%llu failed=%llu dumped=%llu cache_mb=%zu",
                              m_entries.size(), (unsigned long long)m_hashed.load(), (unsigned long long)hashed,
                              (unsigned long long)replaced,
                              (unsigned long long)m_loaded, (unsigned long long)m_failed, (unsigned long long)m_written.load(),
                              m_cacheBytes >> 20);
                return text;
            }

        private:
            struct Entry
            {
                std::string path; // UTF-8
                std::shared_ptr<const GSHwReplacement> image;
                uint64_t lastUse = 0;
                bool queued = false, failed = false;
            };
            struct WriteJob
            {
                std::string name;
                uint32_t width, height;
                std::vector<uint32_t> pixels;
            };

            void index(const fs::path &dir)
            {
                std::error_code ec;
                if (!fs::is_directory(dir, ec))
                {
                    std::fprintf(stderr, "[texpack] no texture pack (%s holds none)\n", utf8(dir).c_str());
                    return;
                }
                size_t dds = 0, region = 0, other = 0, dupes = 0;
                for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
                     !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
                {
                    if (!it->is_regular_file(ec))
                        continue;
                    const std::string name = utf8(it->path().filename());
                    Key k;
                    if (endsWith(name, ".dds"))
                        ++dds;
                    else if (!endsWith(name, ".png"))
                        continue;
                    else if (parseName(name, k))
                    {
                        if (m_byKey.count(k))
                        {
                            ++dupes; // the first found wins
                            continue;
                        }
                        m_byKey.emplace(k, m_entries.size());
                        m_entries.push_back({utf8(it->path())});
                    }
                    else if (name.find("-r") != std::string::npos || name.find("-mip") != std::string::npos)
                        ++region;
                    else
                        ++other;
                }
                std::fprintf(stderr, "[texpack] %zu replacement textures in %s", m_entries.size(), utf8(dir).c_str());
                if (dds || region || other || dupes)
                    std::fprintf(stderr, " (skipped: %zu DDS - convert them to PNG, %zu region / mip level names, %zu other, %zu "
                                         "duplicates)",
                                 dds, region, other, dupes);
                std::fprintf(stderr, "\n");
            }

            void loaderLoop()
            {
                for (;;)
                {
                    size_t index;
                    std::string path;
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_cv.wait(lock, [&] { return m_stop || !m_loads.empty(); });
                        if (m_stop)
                            return;
                        index = m_loads.front();
                        m_loads.pop_front();
                        path = m_entries[index].path;
                    }
                    auto image = std::make_shared<GSHwReplacement>();
                    const bool ok = platform::readPng(path.c_str(), image->width, image->height, image->pixels) &&
                                    image->width <= 8192u && image->height <= 8192u;
                    std::lock_guard<std::mutex> lock(m_mutex);
                    Entry &e = m_entries[index];
                    e.queued = false;
                    if (!ok)
                    {
                        e.failed = true;
                        ++m_failed;
                        std::fprintf(stderr, "[texpack] can't read %s\n", path.c_str());
                        continue;
                    }
                    ++m_loaded;
                    if (m_loaded <= 5u)
                        std::fprintf(stderr, "[texpack] loaded %s (%ux%u)\n", utf8(fs::u8path(path).filename()).c_str(),
                                     image->width, image->height);
                    m_cacheBytes += image->pixels.size() * 4u;
                    e.image = std::move(image);
                    trimCache();
                }
            }

            // the decoded images beyond the limit, least recently used first (m_mutex held)
            void trimCache()
            {
                while (m_cacheBytes > m_cacheLimit)
                {
                    Entry *oldest = nullptr;
                    for (Entry &e : m_entries)
                        if (e.image && (!oldest || e.lastUse < oldest->lastUse))
                            oldest = &e;
                    if (!oldest)
                        return;
                    m_cacheBytes -= oldest->image->pixels.size() * 4u;
                    oldest->image.reset();
                }
            }

            void writerLoop()
            {
                for (;;)
                {
                    WriteJob job;
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_writeCv.wait(lock, [&] { return m_stop || !m_writes.empty(); });
                        if (m_writes.empty())
                            return; // stopping, everything written
                        job = std::move(m_writes.front());
                        m_writes.pop_front();
                    }
                    const fs::path path = m_dumpDir / fs::u8path(job.name);
                    if (platform::writePngRgba(utf8(path).c_str(), reinterpret_cast<const uint8_t *>(job.pixels.data()),
                                               static_cast<int>(job.width), static_cast<int>(job.height)))
                        m_written.fetch_add(1, std::memory_order_relaxed);
                }
            }

            fs::path m_root, m_dumpDir;
            bool m_wants = false, m_dumping = false;
            std::mutex m_mutex;
            std::condition_variable m_cv, m_writeCv;
            bool m_stop = false;
            std::vector<Entry> m_entries;
            std::unordered_map<Key, size_t, KeyHash> m_byKey;
            std::unordered_set<Key, KeyHash> m_dumped;
            std::deque<size_t> m_loads;
            std::deque<WriteJob> m_writes;
            std::vector<std::thread> m_loaders;
            std::thread m_writer;
            uint64_t m_useClock = 0, m_loaded = 0, m_failed = 0;
            size_t m_cacheBytes = 0, m_cacheLimit = size_t(1024) << 20;
            std::atomic<uint64_t> m_hashed{0}, m_written{0};
        };

        Pack *g_pack = nullptr; // lives until the process ends (the renderer holds it)
    }

    void registerSettings()
    {
        settings::Spec s;
        s.key = "graphics.texture_pack";
        s.page = "Graphics";
        s.section = "Texture packs";
        s.label = "Texture packs";
        s.help = "High-resolution textures made for the PS2 game under PCSX2: put the pack's folder into "
                 "textures/SLUS-21590/replacements next to vcs.exe. PNG files only (convert DDS packs). With no pack "
                 "there it changes nothing.";
        s.type = settings::Type::Toggle;
        s.defaultValue = 1;
        s.apply = settings::Apply::Restart;
        s.env = "VCS_TEXPACK";
        s.onChange = [](int v) { g_enabled.store(v != 0); };
        settings::add(s);

        s = {};
        s.key = "graphics.texture_dump";
        s.page = "Graphics";
        s.section = "Texture packs";
        s.label = "Dump textures";
        s.help = "Saves every texture the game draws as a PNG in textures/SLUS-21590/dumps, named as PCSX2 names them: "
                 "the files to edit for a texture pack. Costs some time when new textures appear.";
        s.type = settings::Type::Toggle;
        s.defaultValue = 0;
        s.apply = settings::Apply::Restart;
        s.env = "VCS_TEXTURE_DUMP";
        s.onChange = [](int v) { g_dumpSetting.store(v != 0); };
        settings::add(s);
    }

    void install(PS2Runtime &, const fs::path &gameFolder)
    {
        const bool replace = g_enabled.load(), dump = g_dumpSetting.load();
        if (replace || dump)
        {
            const char *dir = std::getenv("VCS_TEXTURE_DIR");
            const fs::path root = dir && *dir ? fs::u8path(dir) : gameFolder / "textures" / kSerial;
            g_pack = new Pack();
            g_pack->open(root, replace, dump);
            if (g_pack->wantsTextures())
                gsHwSetTexturePack(g_pack);
        }
        else
            std::fprintf(stderr, "[texpack] texture packs off\n");

        // texpack -> ok indexed hashed replaced loaded failed dumped cache_mb (since the start)
        ps2control::addCommand("texpack", [](const std::string &, ps2control::Reply reply) {
            reply(g_pack ? g_pack->status() : std::string("ok off"));
        });
    }
}
