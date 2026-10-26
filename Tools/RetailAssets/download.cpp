#include "Engine/Source/Retail/Appearance.h"
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/RetailModel.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <deque>
#include <set>
#include <stdexcept>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif

namespace app = Retail::Db2::appearance;
namespace {
#ifdef _WIN32
struct WorkerLock {
    HANDLE handle = nullptr;
    bool owned = false;
    WorkerLock(const char* source, const char* config) {
        uint64_t hash = 14695981039346656037ull;
        auto identity = std::string(source) + ":" + std::filesystem::weakly_canonical(config).string();
        for (unsigned char c : identity) {
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            hash ^= c; hash *= 1099511628211ull;
        }
        handle = CreateMutexA(nullptr, FALSE, ("Local\\WowCdnWorker-" + std::to_string(hash)).c_str());
        if (!handle) throw std::runtime_error("Cannot create CDN worker lock");
        const auto status = WaitForSingleObject(handle, 0);
        if (status == WAIT_FAILED) { CloseHandle(handle); handle = nullptr; throw std::runtime_error("Cannot lock CDN worker"); }
        owned = status == WAIT_OBJECT_0 || status == WAIT_ABANDONED;
    }
    ~WorkerLock() { if (owned) ReleaseMutex(handle); if (handle) CloseHandle(handle); }
};
#endif
uint32_t Id(const char* text) {
    uint32_t value = 0;
    auto parsed = std::from_chars(text, text + std::strlen(text), value);
    if (!value || parsed.ec != std::errc() || *parsed.ptr) throw std::runtime_error("Invalid race ID");
    return value;
}
}
int main(int argc, char** argv) {
    if (argc < 4 || (std::string(argv[1]) != "retail" && std::string(argv[1]) != "forever")) {
        std::fprintf(stderr, "Usage: CdnAssetDownload <retail|forever> <config.ini> <source-race-ID> [source-race-ID ...]\n");
        return 2;
    }
    try {
#ifdef _WIN32
        WorkerLock worker(argv[1], argv[2]);
        if (!worker.owned) { std::printf("%s: downloader already running for this config.\n", argv[1]); return 0; }
#endif
        const bool forever = std::string(argv[1]) == "forever";
        Retail::SourceScope scope(forever ? Retail::SourceKind::Forever : Retail::SourceKind::Retail);
        _putenv_s(forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[2]);
        // Each invocation owns one source and process. No Retail worker settings are modified.
        auto& source = Retail::Source();
        if (!app::Status().loaded) throw std::runtime_error(app::Status().error);
        std::deque<std::pair<uint32_t, std::string>> queue;
        std::set<std::pair<uint32_t, std::string>> queued;
        auto add = [&](uint32_t id, const char* type) {
            if (id && queued.emplace(id, type).second) queue.emplace_back(id, type);
        };
        for (int i = 3; i < argc; ++i) for (uint32_t sex = 0; sex < 2; ++sex) {
            app::CharacterAssets roots;
            std::string error;
            if (!app::CollectCharacterAssets(Id(argv[i]), sex, roots, error)) throw std::runtime_error(error);
            for (auto id : roots.models) add(id, ".m2");
            for (auto id : roots.textures) add(id, ".blp");
            for (auto id : roots.skeletons) add(id, ".skel");
        }
        unsigned done = 0, failed = 0;
        while (!queue.empty()) {
            const auto asset = queue.front(); queue.pop_front();
            const std::string path = Retail::FileName(asset.first, asset.second.c_str());
            std::vector<uint8_t> bytes;
            std::string error;
            if (!source.Inspect(path.c_str(), bytes, error)) {
                ++failed;
                std::fprintf(stderr, "%s: %s\n", path.c_str(), error.c_str());
                continue; // Other completed files remain durable; restart retries only misses.
            }
            if (asset.second == ".m2") {
                Retail::ModelFiles graph;
                if (!Retail::InspectModel(bytes, graph, error)) {
                    ++failed;
                    std::fprintf(stderr, "%s: cannot inspect dependencies: %s\n", path.c_str(), error.c_str());
                    continue;
                }
                add(graph.skeleton, ".skel");
                for (auto id : graph.skins) add(id, ".skin");
                for (auto id : graph.textures) add(id, ".blp");
                for (const auto& animation : graph.animations) add(animation.fileId, ".anim");
            } else if (asset.second == ".skel") {
                // SKPD parent and AFID dependencies. Use bounded chunk reads, never client conversion.
                auto word = [&](size_t at) { uint32_t value; std::memcpy(&value, bytes.data() + at, 4); return value; };
                for (size_t p = 0; p < bytes.size();) {
                    if (bytes.size() - p < 8 || word(p + 4) > bytes.size() - p - 8) { ++failed; break; }
                    const uint32_t tag = word(p), size = word(p + 4); p += 8;
                    if (tag == 0x44504b53 && size >= 12) add(word(p + 8), ".skel"); // SKPD
                    if (tag == 0x44494641) { // AFID (u16 animation, u16 variation, u32 FDID)
                        if (size % 8) { ++failed; break; }
                        for (size_t row = p; row < p + size; row += 8) add(word(row + 4), ".anim");
                    }
                    p += size;
                }
            }
            ++done;
            if (!(done % 25) || queue.empty()) {
                const auto stats = source.Stats();
                std::printf("%s: %u files ready, %zu pending; %llu reused, %llu acquired, %u failed\n", argv[1], done, queue.size(),
                    static_cast<unsigned long long>(stats.reused), static_cast<unsigned long long>(stats.acquired), failed);
                std::fflush(stdout);
            }
        }
        const auto stats = source.Stats();
        std::printf("%s: %s; %u files ready; %llu reused, %llu acquired, %u failed.\n", argv[1], failed ? "incomplete" : "complete", done,
            static_cast<unsigned long long>(stats.reused), static_cast<unsigned long long>(stats.acquired), failed);
        return failed ? 1 : 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
