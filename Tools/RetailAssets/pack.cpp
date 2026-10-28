// Custom: builds a pack (Data/Retail/Pack/<FileDataID>.bin, the pack= key of RetailCasc.ini) for retail races, so the
// client reads their files from disk instead of CascLib's CDN mode, which downloads whole 256 MB archives on the game
// thread. Each missing file is downloaded on its own from wago.tools at the pinned build; the walk is
// CdnAssetDownload's (download.cpp): the DB2 tables, then every choice's models, textures and skeletons, then their
// skins, animations and SKEL parents.
#include "Engine/Source/Retail/Appearance.h"
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/RetailModel.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <windows.h>
#include <urlmon.h>

namespace app = Retail::Db2::appearance;
namespace {
// The build the Retail table layouts match (RetailCasc.example.ini)
const char* const BuildConfig = "88d5e8ef05ec5a8446cdec93d142d7c6";
const wchar_t* const BuildVersion = L"12.1.5.70448";

unsigned downloaded = 0;

bool Download(uint32_t id, const std::string& file) {
    const std::wstring url = L"https://wago.tools/api/casc/" + std::to_wstring(id) + L"?download&version=" + BuildVersion;
    const std::wstring target = std::filesystem::path(file).wstring(), partial = target + L".part";
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (SUCCEEDED(URLDownloadToFileW(nullptr, url.c_str(), partial.c_str(), 0, nullptr))) {
            std::error_code code;
            if (std::filesystem::file_size(partial, code) && !code) {
                std::filesystem::rename(partial, target, code);
                if (!code) { ++downloaded; return true; }
            }
        }
        Sleep(1000 * (attempt + 1));
    }
    std::error_code code;
    std::filesystem::remove(partial, code);
    std::fprintf(stderr, "%u: download failed\n", id);
    return false;
}

uint32_t Id(const char* text) {
    uint32_t value = 0;
    auto parsed = std::from_chars(text, text + std::strlen(text), value);
    if (!value || parsed.ec != std::errc() || *parsed.ptr) throw std::runtime_error("Invalid race ID");
    return value;
}
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: RetailPackBuild <pack-directory> <retail-race-ID> [retail-race-ID ...]\n"
                             "  e.g. RetailPackBuild Data/Retail/Pack 52 70   (then pack=Pack in Data/Retail/RetailCasc.ini)\n");
        return 2;
    }
    try {
        const std::filesystem::path pack = std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(pack);
        // A files-mode source over the pack; SetPackMissHandler fills it as the walk reads
        const auto config = pack / "RetailPackBuild.ini";
        {
            std::ofstream ini(config, std::ios::trunc);
            ini << "mode=files\npath=.\nproduct=wow\nregion=us\nbuild=" << BuildConfig << "\nlocale=2\n";
            if (!ini) throw std::runtime_error("Cannot write " + config.string());
        }
        _putenv_s("WOW_RETAIL_CONFIG", config.string().c_str());
        Retail::SetPackMissHandler(Download);
        Retail::SourceScope scope(Retail::SourceKind::Retail);
        auto& source = Retail::Source();
        if (!app::Status().loaded) throw std::runtime_error(app::Status().error);
        std::printf("tables ready (%u downloaded)\n", downloaded);

        std::deque<std::pair<uint32_t, std::string>> queue;
        std::set<std::pair<uint32_t, std::string>> queued;
        auto add = [&](uint32_t id, const char* type) {
            if (id && queued.emplace(id, type).second) queue.emplace_back(id, type);
        };
        for (int i = 2; i < argc; ++i) {
            const uint32_t race = Id(argv[i]);
            for (uint32_t sex = 0; sex < 2; ++sex) {
                for (uint32_t form = app::FormDefault; form <= app::FormVisage; ++form) {
                    if (form != app::FormDefault && !app::ChrModelForRace(race, sex, form)) continue;
                    app::CharacterAssets roots;
                    std::string error;
                    if (!app::CollectCharacterAssets(race, sex, roots, error, form)) {
                        if (form == app::FormDefault) throw std::runtime_error("race " + std::to_string(race) + ": " + error);
                        continue;
                    }
                    for (auto id : roots.models) add(id, ".m2");
                    for (auto id : roots.textures) add(id, ".blp");
                    for (auto id : roots.skeletons) add(id, ".skel");
                }
            }
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
                continue;
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
            if (!(done % 50) || queue.empty()) {
                std::printf("%u files ready, %zu pending; %u downloaded, %u failed\n", done, queue.size(), downloaded, failed);
                std::fflush(stdout);
            }
        }
        std::filesystem::remove(config);
        std::printf("%s: %u files ready, %u downloaded, %u failed.\n", failed ? "incomplete (rerun to retry)" : "complete",
                    done, downloaded, failed);
        return failed ? 1 : 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
