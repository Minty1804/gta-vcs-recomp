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
// Images are PNG or DDS (BC1 / BC2 / BC3 / BC7, or uncompressed RGBA8). [Minty1804 fork] A block-compressed DDS with
// its whole mip chain, width and height multiples of 4 and no clear texel goes to the GPU as stored (after
// willzmkv's (dejot) 37c9194; gs_hw.h GSHwReplacement::blocks): its own mips, a quarter or an eighth of the memory, no
// decode. Any other DDS has its top level decoded to RGBA on the loader thread and the mip chain made from it, as for a
// PNG - cut-out textures that way keep the renderer's alpha-coverage mips and the cut-out split. PS2X_HW_BLOCK_TEXTURES=0
// decodes them all (the reference).
// Not supported (yet): region-clamped textures' "-r<w>x<h>-" names and "-mip<n>" levels (counted at start).
// VCS_TEXPACK=0: nothing hashed or replaced (the original).
#include "game/vcs_texture_pack.h"

#include "platform/platform.h"
#include "runtime/gs/gs_hw.h"
#include "runtime/ps2_control.h"
#include "settings/settings.h"

#define XXH_INLINE_ALL
#include "xxhash.h"

#define BCDEC_STATIC
#define BCDEC_IMPLEMENTATION
#include "bcdec.h" // third_party/bcdec (MIT / public domain): BC1-BC7 blocks to RGBA

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
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

        // A DDS file's top mip level as RGBA8 (bytes R, G, B, A in memory, as readPng gives). BC1 / BC2 / BC3 / BC7 by
        // their DX10 DXGI format or legacy FourCC, and 32-bit uncompressed RGBA / BGRA; anything else fails (logged).
        // [Minty1804 fork] With `keepBlocks` (the device takes them) a block-compressed file that can go up as stored
        // fills image.blocks instead (the file's bytes, moved: no copy, no decode).
        bool readDds(const std::string &path, GSHwReplacement &image, bool keepBlocks, std::string &why)
        {
            uint32_t &width = image.width, &height = image.height;
            std::vector<uint32_t> &pixels = image.pixels;
            FILE *f = nullptr;
#ifdef _WIN32
            const std::u8string u(path.begin(), path.end());
            f = _wfopen(fs::path(u).c_str(), L"rb");
#else
            f = std::fopen(path.c_str(), "rb");
#endif
            if (!f)
            {
                why = "can't open";
                return false;
            }
            std::vector<uint8_t> file;
            std::fseek(f, 0, SEEK_END);
            const long size = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (size > 0)
            {
                file.resize(static_cast<size_t>(size));
                if (std::fread(file.data(), 1, file.size(), f) != file.size())
                    file.clear();
            }
            std::fclose(f);
            auto u32 = [&](size_t off) {
                uint32_t v;
                std::memcpy(&v, file.data() + off, 4);
                return v;
            };
            if (file.size() < 128 || std::memcmp(file.data(), "DDS ", 4) != 0 || u32(4) != 124u)
            {
                why = "not a DDS file";
                return false;
            }
            height = u32(12);
            width = u32(16);
            const uint32_t pfFlags = u32(80), fourcc = u32(84), rgbBits = u32(88), rMask = u32(92), aMask = u32(104);
            size_t offset = 128;
            enum class Kind { None, BC1, BC2, BC3, BC7, RGBA, BGRA } kind = Kind::None;
            auto cc = [](const char *s) { return uint32_t(s[0]) | uint32_t(s[1]) << 8 | uint32_t(s[2]) << 16 | uint32_t(s[3]) << 24; };
            if ((pfFlags & 4u) && fourcc == cc("DX10"))
            {
                if (file.size() < 148)
                {
                    why = "truncated DX10 header";
                    return false;
                }
                offset = 148;
                switch (u32(128)) // DXGI_FORMAT
                {
                case 70: case 71: case 72: kind = Kind::BC1; break;
                case 73: case 74: case 75: kind = Kind::BC2; break;
                case 76: case 77: case 78: kind = Kind::BC3; break;
                case 97: case 98: case 99: kind = Kind::BC7; break;
                case 27: case 28: case 29: kind = Kind::RGBA; break;
                case 87: case 90: case 91: kind = Kind::BGRA; break;
                default: break;
                }
                if (kind == Kind::None)
                    why = "DXGI format " + std::to_string(u32(128)) + " not supported";
            }
            else if (pfFlags & 4u)
            {
                if (fourcc == cc("DXT1"))
                    kind = Kind::BC1;
                else if (fourcc == cc("DXT2") || fourcc == cc("DXT3"))
                    kind = Kind::BC2;
                else if (fourcc == cc("DXT4") || fourcc == cc("DXT5"))
                    kind = Kind::BC3;
                else
                    why = "FourCC not supported";
            }
            else if (rgbBits == 32u)
                kind = rMask == 0x000000FFu ? Kind::RGBA : Kind::BGRA;
            else
                why = "uncompressed format with " + std::to_string(rgbBits) + " bits not supported";
            if (kind == Kind::None)
                return false;
            if (!width || !height || width > 8192u || height > 8192u)
            {
                why = "bad size";
                return false;
            }
            const size_t blockBytes = (kind == Kind::BC1) ? 8u : 16u;
            // [Minty1804 fork] as stored: the whole chain in the file (mip count in the header, the bytes there),
            // level 0's sides multiples of 4 (D3D11's rule), no clear texel in level 0 (below)
            if (keepBlocks && kind != Kind::RGBA && kind != Kind::BGRA && width % 4u == 0u && height % 4u == 0u)
            {
                uint32_t full = 1u;
                for (uint32_t s = std::max(width, height); s > 1u; s >>= 1)
                    ++full;
                size_t chain = 0;
                for (uint32_t i = 0, lw = width, lh = height; i < full; ++i, lw = std::max(1u, lw >> 1), lh = std::max(1u, lh >> 1))
                    chain += size_t((lw + 3u) / 4u) * ((lh + 3u) / 4u) * blockBytes;
                // No clear texel in level 0 (none at a tenth of its top alpha or under): gs_hw's hasHoles / isCutOut,
                // and with them the cut-out split and the alpha-coverage mips, all need a share of clear texels, so such
                // an image loses nothing as stored. (Not "one alpha everywhere": none of the 1379 BC7 images a real pack
                // loaded in a drive had that.) Each block's alpha range is read from its endpoints where the format allows -
                // BC1 / BC2 / BC3, BC7 modes 0-3 (no alpha: 255), 5 (rotation 0), 6, 7 - an upper bound for the top and a
                // lower bound for the least, so the test is never looser than the texels'; BC7 modes 4 and 5 rotated are
                // decoded (bcdec).
                auto noClearTexels = [&] {
                    const uint8_t *src = file.data() + offset;
                    const size_t blocks = size_t(width / 4u) * (height / 4u);
                    unsigned least = 255u, top = 0u;
                    auto range = [&](unsigned lo, unsigned hi) {
                        least = std::min(least, lo);
                        top = std::max(top, hi);
                    };
                    // bit `pos` (n bits, n <= 8) of a 128-bit BC7 block
                    auto bits = [](const uint8_t *b, unsigned pos, unsigned n) {
                        unsigned v = 0;
                        for (unsigned i = 0; i < n; ++i, ++pos)
                            v |= unsigned((b[pos >> 3] >> (pos & 7u)) & 1u) << i;
                        return v;
                    };
                    uint32_t block[16];
                    for (size_t i = 0; i < blocks; ++i, src += blockBytes)
                    {
                        if (kind == Kind::BC1) // 255, or 0 for index 3 of a block in 3-colour mode (colour0 <= colour1)
                        {
                            uint16_t c0, c1;
                            uint32_t idx;
                            std::memcpy(&c0, src, 2);
                            std::memcpy(&c1, src + 2, 2);
                            std::memcpy(&idx, src + 4, 4);
                            range(c0 <= c1 && ((idx & (idx >> 1)) & 0x55555555u) ? 0u : 255u, 255u);
                            continue;
                        }
                        if (kind == Kind::BC2) // 16 explicit 4-bit alphas (x 17)
                        {
                            for (int k = 0; k < 8; ++k)
                                range(std::min(src[k] & 15u, unsigned(src[k] >> 4)) * 17u,
                                      std::max(src[k] & 15u, unsigned(src[k] >> 4)) * 17u);
                            continue;
                        }
                        if (kind == Kind::BC3) // by the indices used: 0 / 1 the endpoints, 2-7 between them - except
                        {                      // 6 / 7 with alpha0 <= alpha1: 0 / 255
                            const unsigned a0 = src[0], a1 = src[1];
                            uint64_t idx = 0;
                            std::memcpy(&idx, src + 2, 6);
                            for (int k = 0; k < 16; ++k)
                            {
                                const unsigned v = unsigned(idx >> (3 * k)) & 7u;
                                if (v == 0u)
                                    range(a0, a0);
                                else if (v == 1u)
                                    range(a1, a1);
                                else if (a0 <= a1 && v >= 6u)
                                    range(v == 6u ? 0u : 255u, v == 6u ? 0u : 255u);
                                else
                                    range(std::min(a0, a1), std::max(a0, a1));
                            }
                            continue;
                        }
                        const unsigned mode = src[0] ? static_cast<unsigned>(__builtin_ctz(src[0])) : 8u;
                        if (mode < 4u)
                        {
                            range(255u, 255u);
                            continue;
                        }
                        if (mode == 5u && bits(src, 6, 2) == 0u) // alpha endpoints: 8 bits at 50 and 58
                        {
                            const unsigned a0 = bits(src, 50, 8), a1 = bits(src, 58, 8);
                            range(std::min(a0, a1), std::max(a0, a1));
                            continue;
                        }
                        if (mode == 6u) // 7 bits at 49 / 56, p-bits at 63 / 64
                        {
                            const unsigned a0 = (bits(src, 49, 7) << 1) | bits(src, 63, 1);
                            const unsigned a1 = (bits(src, 56, 7) << 1) | bits(src, 64, 1);
                            range(std::min(a0, a1), std::max(a0, a1));
                            continue;
                        }
                        if (mode == 7u) // 4 endpoints: 5 bits at 74 + 5k, p-bits at 94 + k, 6 bits expanded to 8
                        {
                            for (unsigned k = 0; k < 4; ++k)
                            {
                                const unsigned v = (bits(src, 74 + 5 * k, 5) << 1) | bits(src, 94 + k, 1);
                                const unsigned a = (v << 2) | (v >> 4);
                                range(a, a);
                            }
                            continue;
                        }
                        bcdec_bc7(src, block, 16); // modes 4, 5 rotated (an invalid block decodes to 0: clear)
                        for (uint32_t texel : block)
                            range(texel >> 24, texel >> 24);
                    }
                    const bool ok = top > 0u && least > top / 10u;
                    // VCS_TEXPACK_VERIFY=1: every image the bounds accept is decoded and its texels checked (the
                    // reference; texpack_check's real-pack run)
                    static const bool verify = [] {
                        const char *e = std::getenv("VCS_TEXPACK_VERIFY");
                        return e && *e == '1';
                    }();
                    if (verify)
                    {
                        unsigned realLeast = 255u, realTop = 0u;
                        const uint8_t *b = file.data() + offset;
                        for (size_t i = 0; i < blocks; ++i, b += blockBytes)
                        {
                            switch (kind)
                            {
                            case Kind::BC1: bcdec_bc1(b, block, 16); break;
                            case Kind::BC2: bcdec_bc2(b, block, 16); break;
                            case Kind::BC3: bcdec_bc3(b, block, 16); break;
                            default: bcdec_bc7(b, block, 16); break;
                            }
                            for (uint32_t texel : block)
                                realLeast = std::min(realLeast, unsigned(texel >> 24)), realTop = std::max(realTop, unsigned(texel >> 24));
                        }
                        const bool real = realTop > 0u && realLeast > realTop / 10u;
                        std::fprintf(stderr, "[texpack] verify %s: bounds %u..%u, texels %u..%u: %s\n", path.c_str(), least,
                                     top, realLeast, realTop,
                                     ok == real ? "agree" : ok ? "MISMATCH (accepted with clear texels)" : "stricter (kept decoded)");
                    }
                    return ok;
                };
                if (u32(28) >= full && file.size() - offset >= chain && noClearTexels())
                {
                    image.blockFormat = kind == Kind::BC1   ? GSHwBlockBC1
                                        : kind == Kind::BC2 ? GSHwBlockBC2
                                        : kind == Kind::BC3 ? GSHwBlockBC3
                                                            : GSHwBlockBC7;
                    file.erase(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(offset));
                    file.resize(chain);
                    file.shrink_to_fit();
                    image.blocks = std::move(file);
                    return true;
                }
            }
            pixels.assign(size_t(width) * height, 0u);
            if (kind == Kind::RGBA || kind == Kind::BGRA)
            {
                const size_t need = size_t(width) * height * 4u;
                if (file.size() - offset < need)
                {
                    why = "truncated";
                    return false;
                }
                std::memcpy(pixels.data(), file.data() + offset, need);
                const bool hasAlpha = ((pfFlags & 1u) && aMask) || offset == 148; // without: opaque on the GS scale
                for (uint32_t &p : pixels)
                {
                    if (kind == Kind::BGRA)
                        p = (p & 0xFF00FF00u) | ((p & 0xFFu) << 16) | ((p >> 16) & 0xFFu);
                    if (!hasAlpha)
                        p = (p & 0x00FFFFFFu) | 0x80000000u;
                }
                return true;
            }
            const uint32_t bw = (width + 3u) / 4u, bh = (height + 3u) / 4u;
            if (file.size() - offset < size_t(bw) * bh * blockBytes)
            {
                why = "truncated";
                return false;
            }
            const uint8_t *src = file.data() + offset;
            uint32_t block[16];
            for (uint32_t by = 0; by < bh; ++by)
                for (uint32_t bx = 0; bx < bw; ++bx, src += blockBytes)
                {
                    switch (kind)
                    {
                    case Kind::BC1: bcdec_bc1(src, block, 16); break;
                    case Kind::BC2: bcdec_bc2(src, block, 16); break;
                    case Kind::BC3: bcdec_bc3(src, block, 16); break;
                    default: bcdec_bc7(src, block, 16); break;
                    }
                    const uint32_t w = std::min(4u, width - bx * 4u), h = std::min(4u, height - by * 4u);
                    for (uint32_t y = 0; y < h; ++y)
                        std::memcpy(&pixels[size_t(by * 4u + y) * width + bx * 4u], &block[y * 4u], w * 4u);
                }
            return true;
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
                              "ok indexed=%zu hashed=%llu/%llu replaced=%llu loaded=%llu failed=%llu dumped=%llu cache_mb=%zu "
                              "load_ms_avg=%.1f blocks=%llu cache_kb=%zu",
                              m_entries.size(), (unsigned long long)m_hashed.load(), (unsigned long long)hashed,
                              (unsigned long long)replaced,
                              (unsigned long long)m_loaded, (unsigned long long)m_failed, (unsigned long long)m_written.load(),
                              m_cacheBytes >> 20, m_loaded ? m_loadUs / 1000.0 / double(m_loaded) : 0.0,
                              (unsigned long long)m_blockLoaded, m_cacheBytes >> 10);
                return text;
            }

        private:
            struct Entry
            {
                std::string path; // UTF-8
                std::shared_ptr<const GSHwReplacement> image;
                uint64_t lastUse = 0;
                bool queued = false, failed = false, dds = false;
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
                    const bool isDds = endsWith(name, ".dds");
                    if (!isDds && !endsWith(name, ".png"))
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
                        m_entries.back().dds = isDds;
                        dds += isDds;
                    }
                    else if (name.find("-r") != std::string::npos || name.find("-mip") != std::string::npos)
                        ++region;
                    else
                        ++other;
                }
                std::fprintf(stderr, "[texpack] %zu replacement textures in %s (%zu DDS)", m_entries.size(),
                             utf8(dir).c_str(), dds);
                if (region || other || dupes)
                    std::fprintf(stderr, " (skipped: %zu region / mip level names, %zu other, %zu duplicates)", region,
                                 other, dupes);
                std::fprintf(stderr, "\n");
            }

            void loaderLoop()
            {
                for (;;)
                {
                    size_t index;
                    std::string path;
                    bool dds;
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_cv.wait(lock, [&] { return m_stop || !m_loads.empty(); });
                        if (m_stop)
                            return;
                        index = m_loads.front();
                        m_loads.pop_front();
                        path = m_entries[index].path;
                        dds = m_entries[index].dds;
                    }
                    auto image = std::make_shared<GSHwReplacement>();
                    const auto start = std::chrono::steady_clock::now();
                    std::string why;
                    static const uint32_t maxSize = [] {
                        const char *e = std::getenv("VCS_TEXPACK_MAX");
                        return e && *e ? static_cast<uint32_t>(std::strtoul(e, nullptr, 10)) : 0u;
                    }();
                    bool ok = (dds ? readDds(path, *image, gsHwBlockCompression(), why)
                                   : platform::readPng(path.c_str(), image->width, image->height, image->pixels)) &&
                              image->width <= 8192u && image->height <= 8192u;
                    // [Minty1804 fork] block-compressed, larger than the limit: the file's first level that fits becomes
                    // the image (its sides multiples of 4 still; otherwise decoded as any other DDS)
                    if (ok && image->blockFormat && maxSize >= 16u && (image->width > maxSize || image->height > maxSize))
                    {
                        const bool bc1 = image->blockFormat == GSHwBlockBC1;
                        uint32_t w = image->width, h = image->height;
                        size_t at = 0;
                        while (w > maxSize || h > maxSize)
                        {
                            at += size_t((w + 3u) / 4u) * ((h + 3u) / 4u) * (bc1 ? 8u : 16u);
                            w = std::max(1u, w >> 1), h = std::max(1u, h >> 1);
                        }
                        if (w % 4u == 0u && h % 4u == 0u && at < image->blocks.size())
                        {
                            image->blocks.erase(image->blocks.begin(), image->blocks.begin() + static_cast<std::ptrdiff_t>(at));
                            image->blocks.shrink_to_fit();
                            image->width = w, image->height = h;
                        }
                        else
                        {
                            image = std::make_shared<GSHwReplacement>();
                            ok = readDds(path, *image, false, why);
                        }
                    }
                    if (ok && !image->blockFormat) // the mip chain here, once per image: the renderer only uploads it (PERFORMANCE.md 33)
                    {
                        gsHwBuildMipChain(image->pixels, image->width, image->height, image->mips);
                        // larger than the limit: the chain's first level that fits becomes the image
                        if (maxSize >= 16u && (image->width > maxSize || image->height > maxSize))
                        {
                            uint32_t w = image->width, h = image->height;
                            size_t at = 0;
                            while (w > maxSize || h > maxSize)
                            {
                                w = std::max(1u, w >> 1), h = std::max(1u, h >> 1);
                                if (w > maxSize || h > maxSize)
                                    at += size_t(w) * h;
                            }
                            std::vector<uint32_t> base(image->mips.begin() + at, image->mips.begin() + at + size_t(w) * h);
                            image->pixels = std::move(base);
                            image->width = w, image->height = h;
                            gsHwBuildMipChain(image->pixels, w, h, image->mips);
                        }
                    }
                    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::steady_clock::now() - start).count();
                    std::lock_guard<std::mutex> lock(m_mutex);
                    Entry &e = m_entries[index];
                    e.queued = false;
                    if (!ok)
                    {
                        e.failed = true;
                        ++m_failed;
                        std::fprintf(stderr, "[texpack] can't read %s%s%s\n", path.c_str(), why.empty() ? "" : ": ",
                                     why.c_str());
                        continue;
                    }
                    ++m_loaded;
                    m_blockLoaded += image->blockFormat ? 1u : 0u;
                    m_loadUs += static_cast<uint64_t>(us);
                    if (m_loaded <= 5u)
                        std::fprintf(stderr, "[texpack] loaded %s (%ux%u%s, %.1f ms)\n",
                                     utf8(fs::u8path(path).filename()).c_str(), image->width, image->height,
                                     image->blockFormat ? ", block-compressed as stored" : "", us / 1000.0);
                    m_cacheBytes += image->bytes();
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
                    m_cacheBytes -= oldest->image->bytes();
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
            uint64_t m_useClock = 0, m_loaded = 0, m_failed = 0, m_loadUs = 0, m_blockLoaded = 0;
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
                 "textures/SLUS-21590/replacements next to vcs.exe. PNG or DDS files. With no pack "
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
