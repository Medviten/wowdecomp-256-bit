#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/BusyNotice.h"
#include "Engine/Source/Retail/DecodedCache.h"
#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Retail/ClientTables.h"
#include "Engine/Source/Retail/SlimeGiant.h"
#include "Engine/Source/Retail/Centaur.h"
#include <CascLib.h>
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace Retail {
namespace {
PackMissHandler packMissHandler = nullptr; // Custom: SetPackMissHandler
std::string Normalize(std::string name) {
    for (auto& c : name) {
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    return name;
}
std::string Trim(std::string text) {
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    return text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
}
bool Number(const std::string& s, uint32_t& result, int base = 10) {
    auto parsed = std::from_chars(s.data(), s.data() + s.size(), result, base);
    return !s.empty() && parsed.ec == std::errc() && parsed.ptr == s.data() + s.size() && result;
}
std::string Extension(const std::string& name) {
    const auto dot = name.find_last_of('.');
    return dot == std::string::npos ? "" : name.substr(dot);
}
bool SafeName(const std::string& name) {
    if (name.empty() || name.front() == '/' || name.find(':') != std::string::npos) return false;
    std::istringstream parts(name);
    std::string part;
    while (std::getline(parts, part, '/')) if (part == ".." || part.empty()) return false;
    return true;
}
bool TableName(const std::string& name) {
    for (const char* table : {"item.dbc", "itemdisplayinfo.dbc", "creaturemodeldata.dbc", "creaturedisplayinfo.dbc",
        "spell.dbc", "spellicon.dbc", "skilllineability.dbc"})
        if (name == std::string("dbfilesclient/") + table) return true;
    return false;
}
bool Failure(std::string& error, const char* message) { error = message; return false; }
bool CascFailure(std::string& error, const char* operation) {
    error = std::string(operation) + " (CascLib error " + std::to_string(GetCascError()) + ")";
    return false;
}
struct File {
    HANDLE handle = nullptr;
    ~File() { if (handle) CascCloseFile(handle); }
};
#ifdef _WIN32
struct CacheLock {
    HANDLE handle;
    explicit CacheLock(HANDLE mutex) : handle(mutex) {
        if (handle) {
            const auto status = WaitForSingleObject(handle, INFINITE);
            if (status != WAIT_OBJECT_0 && status != WAIT_ABANDONED) throw std::runtime_error("Cannot lock CDN cache");
        }
    }
    ~CacheLock() { if (handle) ReleaseMutex(handle); }
};
#else
struct CacheLock { explicit CacheLock(void*) {} };
#endif
struct Model {
    std::vector<uint8_t> bytes;
    ModelFiles files;
    bool character = false;
    bool slimeGiant = false;
    bool centaur = false;
    bool centaurFemale = false;
};
struct Reference {
    uint32_t id = 0;
    std::shared_ptr<Model> owner;
    uint16_t animation = 0;
    uint16_t variation = 0;
};
}

struct CascSource::Impl {
    std::mutex mutex;
    HANDLE storage = nullptr;
    HANDLE cacheMutex = nullptr;
    std::string path, product = "wow", region = "us", build, keys, tables, cache, tablePolicy = "tables";
    // Custom: shipped original files (<FileDataID>.bin), read before the decoded cache and CASC in every mode
    std::string pack;
    SourceKind kind = SourceKind::Retail;
    Statistics stats;
    std::string Prefix() const { return kind == SourceKind::Forever ? "forever/" : "retail/"; }
    uint32_t locale = CASC_LOCALE_ENUS;
    bool online = true, configured = false, files = false;
    std::string configurationError = "RetailCasc.ini has not been configured";
    std::map<std::string, uint32_t> names;
    std::map<std::string, Reference> dependencies;
    std::map<std::string, std::pair<std::shared_ptr<Model>, size_t>> models;
    std::map<std::string, uint32_t> cachedSizes;
    std::set<uint32_t> characterModels;
    std::map<uint32_t, HandItemAppearance> handItems;
    HandItemAppearance mainHandPreview;
    ~Impl() {
        if (storage) CascCloseStorage(storage);
#ifdef _WIN32
        if (cacheMutex) CloseHandle(cacheMutex);
#endif
    }

    bool Open(std::string& error) {
        if (!configured) { error = configurationError; return false; }
        if (storage) return true;
        CASC_OPEN_STORAGE_ARGS args = {};
        args.Size = sizeof(args);
        args.szLocalPath = path.c_str();
        args.szCodeName = product.c_str();
        args.szRegion = region.c_str();
        args.szBuildKey = build.empty() ? nullptr : build.c_str();
        args.dwLocaleMask = locale;
        if (!CascOpenStorageEx(nullptr, &args, online, &storage)) return CascFailure(error, "Cannot open CASC storage");
        if (!keys.empty() && !CascImportKeysFromFile(storage, keys.c_str())) {
            CascFailure(error, "Cannot import encryption keys");
            CascCloseStorage(storage);
            storage = nullptr;
            return false;
        }
        return true;
    }

    bool Resolve(const std::string& name, Reference& ref, std::string& internal, std::string& error) {
        if (!SafeName(name)) return Failure(error, "Invalid Retail virtual path");
        const auto dep = dependencies.find(name);
        if (dep != dependencies.end()) { ref = dep->second; return true; }
        const auto alias = names.find(name);
        if (alias != names.end()) { ref.id = alias->second; return true; }
        if (kind == SourceKind::Retail && (ref.id = SlimeGiant::PathModel(name))) return true;
        if (kind == SourceKind::Retail && (ref.id = Centaur::PathModel(name))) return true;
        if (name.compare(0, Prefix().size(), Prefix()) != 0) return Failure(error, "No CDN alias for this path");
        internal = name.substr(Prefix().size());
        const auto dot = internal.find('.');
        const auto stem = internal.substr(0, dot);
        if (Number(stem, ref.id)) { internal.clear(); return true; }
        // Numeric-looking names must not silently turn an overflowing ID into a path.
        if (stem.find_first_not_of("0123456789") == std::string::npos)
            return Failure(error, "Invalid Retail FileDataID");
        return true;
    }

    bool OpenFile(const Reference& ref, const std::string& internal, File& file, std::string& error, bool lenient = false) {
        CacheLock lock(cacheMutex);
        if (!Open(error)) return false;
        const void* name = ref.id ? CASC_FILE_DATA_ID(ref.id) : internal.c_str();
        DWORD flags = (ref.id ? CASC_OPEN_BY_FILEID : CASC_OPEN_BY_NAME) | (lenient ? CASC_OVERCOME_ENCRYPTED : CASC_STRICT_DATA_CHECK);
        if (!CascOpenFile(storage, name, locale, flags, &file.handle)) return CascFailure(error, "Cannot open Retail file");
        return true;
    }

    // Custom: the shipped original of a FileDataID reference, if the pack has it
    std::filesystem::path PackPath(const Reference& ref) const {
        if (pack.empty() || !ref.id) return {};
        return std::filesystem::path(pack) / (std::to_string(ref.id) + ".bin");
    }

    bool ReadPack(const Reference& ref, std::vector<uint8_t>& bytes) const {
        const auto file = PackPath(ref);
        if (file.empty()) return false;
        std::ifstream input(file, std::ios::binary | std::ios::ate);
        if (!input) return false;
        const auto size = input.tellg();
        if (size < 0 || size > 128 * 1024 * 1024) return false;
        bytes.resize(static_cast<size_t>(size));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) { bytes.clear(); return false; }
        return true;
    }

    bool Fetch(const Reference& ref, const std::string& internal, std::vector<uint8_t>& bytes, std::string& error, bool lenient = false) {
        // Custom: files first; the cache and CASC/CDN only for what the pack lacks
        if (ReadPack(ref, bytes)) { ++stats.reused; return true; }
        if (files) {
            if (!configured || !ref.id) return Failure(error, "Offline Retail packs require configured FileDataID references");
            const auto packed = std::filesystem::path(path) / (std::to_string(ref.id) + ".bin");
            // Custom: let a pack builder supply what the folder lacks (SetPackMissHandler)
            std::error_code code;
            if (packMissHandler && !std::filesystem::exists(packed, code)) packMissHandler(ref.id, packed.string());
            std::ifstream input(packed, std::ios::binary | std::ios::ate);
            if (!input) return Failure(error, "Missing original asset in offline Retail pack");
            const auto size = input.tellg();
            if (size < 0 || size > 128 * 1024 * 1024) return Failure(error, "Invalid offline Retail asset size");
            bytes.resize(static_cast<size_t>(size));
            input.seekg(0);
            if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return Failure(error, "Cannot read offline Retail asset");
            return true;
        }
        const auto cached = CachePath(ref, internal, lenient);
        if (!cached.empty() && ReadDecodedCache(cached, bytes)) { ++stats.reused; return true; }
        CacheLock lock(cacheMutex);
        // Another client/downloader may have completed this miss while we waited.
        if (!cached.empty() && ReadDecodedCache(cached, bytes)) { ++stats.reused; return true; }
        // CascLib may satisfy a miss from its encoded disk cache or a local installation.
        // Do not claim every read is a network download.
        BusyNotice busy(kind == SourceKind::Forever ? "Forever: acquiring missing data" : "Retail: acquiring missing data", 1);
        File file;
        if (!OpenFile(ref, internal, file, error, lenient)) return false;
        ULONGLONG size = 0;
        if (!CascGetFileSize64(file.handle, &size)) return CascFailure(error, "Cannot get Retail file size");
        if (size > 128 * 1024 * 1024) return Failure(error, "Retail file exceeds 128 MiB limit");
        bytes.resize(static_cast<size_t>(size));
        DWORD read = 0;
        if (size && (!CascReadFile(file.handle, bytes.data(), static_cast<DWORD>(size), &read) || read != size)) {
            bytes.clear();
            return CascFailure(error, "Cannot read Retail file (missing data or encryption key)");
        }
        if (!cached.empty() && !WriteDecodedCache(cached, bytes, error)) { bytes.clear(); return false; }
        ++stats.acquired;
        return true;
    }

    std::filesystem::path CachePath(const Reference& ref, const std::string& internal, bool lenient) const {
        if (cache.empty()) return {};
        std::filesystem::path key;
        std::string segment = "name-";
        if (!ref.id) {
            constexpr char hex[] = "0123456789abcdef";
            for (unsigned char c : internal) {
                segment += hex[c >> 4]; segment += hex[c & 15];
                if (segment.size() >= 120) { key /= segment; segment.clear(); }
            }
            key /= segment + ".bin";
        } else key = "id-" + std::to_string(ref.id) + ".bin";
        return std::filesystem::path(cache) / std::to_string(locale) / (lenient ? tablePolicy : "strict") / key;
    }

    bool LoadSkeleton(uint32_t id, std::vector<uint8_t>& bytes, std::string& error, std::vector<uint32_t>& chain) {
        if (chain.size() >= 8 || std::find(chain.begin(), chain.end(), id) != chain.end())
            return Failure(error, "Retail skeleton parent cycle or depth limit");
        chain.push_back(id);
        if (!Fetch(Reference{id}, "", bytes, error)) return false;
        uint32_t parent = 0;
        bool found = false;
        const auto word = [&](size_t at) {
            return uint32_t(bytes[at]) | (uint32_t(bytes[at + 1]) << 8) |
                (uint32_t(bytes[at + 2]) << 16) | (uint32_t(bytes[at + 3]) << 24);
        };
        for (size_t p = 0; p < bytes.size();) {
            if (bytes.size() - p < 8) return Failure(error, "Truncated Retail skeleton chunk header");
            const uint32_t tag = word(p), size = word(p + 4);
            p += 8;
            if (size > bytes.size() - p) return Failure(error, "Retail skeleton chunk exceeds file size");
            if (tag == 0x44504b53) { // SKPD: parent FileDataID is payload word 2.
                if (found || size < 12) return Failure(error, "Invalid or duplicate skeleton parent chunk");
                parent = word(p + 8);
                found = true;
            }
            p += size;
        }
        if (parent) {
            std::vector<uint8_t> ancestor, merged;
            if (!LoadSkeleton(parent, ancestor, error, chain) || !MergeSkeleton(bytes, ancestor, merged, error)) return false;
            bytes.swap(merged);
        }
        chain.pop_back();
        return true;
    }
};

CascSource::CascSource(SourceKind kind) : m_impl(new Impl) { m_impl->kind = kind; }
CascSource::~CascSource() = default;

void CascSource::RegisterCharacterModel(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (id) m_impl->characterModels.insert(id);
}

bool CascSource::Configure(const char* configPath, std::string& error) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto& s = *m_impl;
    error.clear();
    if (s.storage || s.configured) return Failure(error, "CASC source is already configured; restart to change builds");
    try {
    // Failed configuration is retained for the client's diagnostic log.
    struct Remember { Impl& state; std::string& error; ~Remember() { state.configurationError = error; } } remember{s, error};
    std::ifstream input(configPath);
    if (!input) return Failure(error, s.kind == SourceKind::Forever
        ? "Cannot read ForeverCasc.ini; set WOW_FOREVER_CONFIG"
        : "Cannot read RetailCasc.ini; set WOW_RETAIL_CONFIG");
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto equal = line.find('=');
        if (equal == std::string::npos) return Failure(error, "Expected key=value in Retail CASC config");
        auto key = Normalize(Trim(line.substr(0, equal)));
        if (!values.emplace(key, Trim(line.substr(equal + 1))).second) return Failure(error, "Duplicate Retail CASC config key");
    }
    for (const auto& value : values) {
        if (value.first != "mode" && value.first != "path" && value.first != "product" && value.first != "region" &&
            value.first != "build" && value.first != "locale" && value.first != "aliases" && value.first != "listfile" && value.first != "keys" && value.first != "equipment" && value.first != "tables" && value.first != "cache" && value.first != "cdn" && value.first != "version" &&
            value.first != "pack") // Custom: shipped original files
            return Failure(error, "Unknown Retail CASC config key");
    }
    if (values["mode"] != "online" && values["mode"] != "local" && values["mode"] != "files") return Failure(error, "mode must be online, local or files");
    s.online = values["mode"] == "online";
    s.files = values["mode"] == "files";
    if (!values["product"].empty()) s.product = values["product"];
    if (!values["region"].empty()) s.region = values["region"];
    if (s.kind == SourceKind::Forever && values["product"].empty()) s.product = "wow_classic_beta";
    if (s.product != "wow" && s.product != "wowt" && s.product != "wow_beta" &&
        !(s.kind == SourceKind::Forever && s.product == "wow_classic_beta")) return Failure(error, "Unsupported CDN product for this source");
    if (s.region.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos || s.region.empty())
        return Failure(error, "Invalid Retail region");
    s.build = Normalize(values["build"]);
    if ((s.online || s.files || !s.build.empty()) && (s.build.size() != 32 || s.build.find_first_not_of("0123456789abcdef") != std::string::npos))
        return Failure(error, "Online mode requires an explicit 32-digit build config key");
    if (!values["locale"].empty() && !Number(values["locale"], s.locale, 16)) return Failure(error, "locale must be a nonzero hexadecimal locale mask");
    const auto base = std::filesystem::absolute(configPath).parent_path();
    auto absolute = [&](const std::string& text) { return (base / std::filesystem::path(text)).lexically_normal(); };
    if (values["path"].empty()) return Failure(error, "CASC storage/cache path is required");
    auto path = absolute(values["path"]);
    if (s.online) {
        path /= s.product + "-" + s.build;
        // MPQ clients can own Data/Retail caches; avoid ancestors of actual CASC installs.
        for (auto parent = path; !parent.empty();) {
            for (const char* file : {".build.info", ".build.db", "data/.build.config"})
                if (std::filesystem::exists(parent / file)) return Failure(error, "Online cache must be outside installed CASC storages");
            if (parent != path && std::filesystem::exists(parent / "versions"))
                return Failure(error, "Online cache must not be nested beneath another versions manifest");
            auto next = parent.parent_path();
            if (next == parent) break;
            parent = next;
        }
    }
    s.path = path.string();
#ifdef _WIN32
    uint64_t identity = 14695981039346656037ull;
    for (unsigned char c : Normalize(s.path)) { identity ^= c; identity *= 1099511628211ull; }
    if (s.cacheMutex) CloseHandle(s.cacheMutex);
    s.cacheMutex = CreateMutexA(nullptr, FALSE, ("Local\\WowCascCache-" + std::to_string(identity)).c_str());
    if (!s.cacheMutex) return Failure(error, "Cannot create CDN cache lock");
#endif
    // CascLib overrides the build key but otherwise uses today's CDN index from /versions.
    // Pin the paired index as well so archived builds keep resolving their data archives.
    if (!values["cdn"].empty()) {
        const auto cdn = Normalize(values["cdn"]);
        const auto version = values["version"];
        if (!s.online || cdn.size() != 32 || cdn.find_first_not_of("0123456789abcdef") != std::string::npos ||
            version.empty() || version.find_first_not_of("0123456789.") != std::string::npos)
            return Failure(error, "cdn requires online mode, a 32-digit CDN config key and a numeric version");
        CacheLock lock(s.cacheMutex);
        std::filesystem::create_directories(path);
        const auto manifest = path / "versions";
        const std::string text = "Region!STRING:0|BuildConfig!HEX:16|CDNConfig!HEX:16|KeyRing!HEX:16|BuildId!DEC:4|VersionsName!String:0|ProductConfig!HEX:16\n" +
            s.region + "|" + s.build + "|" + cdn + "||" + version.substr(version.find_last_of('.') + 1) + "|" + version + "|\n";
        std::ifstream existing(manifest, std::ios::binary);
        const std::string previous((std::istreambuf_iterator<char>(existing)), {});
        existing.close();
        if (previous != text) {
            const auto temporary = path / "versions.tmp";
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output << text;
            output.close();
            if (!output) return Failure(error, "Cannot persist pinned CDN manifest");
#ifdef _WIN32
            if (!MoveFileExW(temporary.c_str(), manifest.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return Failure(error, "Cannot commit pinned CDN manifest");
#else
            std::filesystem::rename(temporary, manifest);
#endif
        }
    } else if (!values["version"].empty()) return Failure(error, "version requires a pinned CDN config key");
    s.tables = values["tables"].empty() ? "" : absolute(values["tables"]).string();
    s.pack = values["pack"].empty() ? "" : absolute(values["pack"]).string(); // Custom
    s.keys = values["keys"].empty() ? "" : absolute(values["keys"]).string();
    if (!s.files && (s.online || !s.build.empty())) {
        auto cache = values["cache"].empty() ? (s.online ? path / "decoded" : base / "CdnCache" / (s.product + "-" + s.build))
            : absolute(values["cache"]) / (s.product + "-" + s.build);
        s.cache = cache.string();
    } else if (!values["cache"].empty()) return Failure(error, "Decoded cache requires a pinned build and online or local mode");
    if (!s.keys.empty()) {
        std::ifstream keyFile(s.keys, std::ios::binary);
        if (!keyFile) return Failure(error, "Cannot read CDN encryption key file");
        uint64_t fingerprint = 14695981039346656037ull;
        for (char c; keyFile.get(c);) { fingerprint ^= static_cast<unsigned char>(c); fingerprint *= 1099511628211ull; }
        s.tablePolicy += "-" + std::to_string(fingerprint);
    }
    std::map<std::string, uint32_t> names;
    for (const char* kind : {"listfile", "aliases"}) {
        if (values[kind].empty()) continue;
        std::ifstream csv(absolute(values[kind]));
        if (!csv) return Failure(error, "Cannot read Retail listfile/aliases");
        while (std::getline(csv, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#') continue;
            auto separator = line.find(';');
            uint32_t id = 0;
            if (separator == std::string::npos || !Number(Trim(line.substr(0, separator)), id))
                return Failure(error, "Retail mapping must use FileDataID;path rows");
            auto name = Normalize(Trim(line.substr(separator + 1)));
            if (!SafeName(name)) return Failure(error, "Invalid Retail mapping path");
            if (std::string(kind) == "listfile") name = s.Prefix() + name;
            const auto result = names.emplace(name, id);
            if (!result.second && result.first->second != id) return Failure(error, "Conflicting Retail mappings");
        }
    }
    std::map<uint32_t, HandItemAppearance> handItems;
    if (!values["equipment"].empty()) {
        std::ifstream csv(absolute(values["equipment"]));
        if (!csv) return Failure(error, "Cannot read Retail equipment mapping");
        while (std::getline(csv, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#') continue;
            const auto first = line.find(';'), second = line.find(';', first == std::string::npos ? first : first + 1);
            uint32_t display = 0;
            HandItemAppearance appearance;
            if (first == std::string::npos || second == std::string::npos ||
                !Number(Trim(line.substr(0, first)), display) ||
                !Number(Trim(line.substr(first + 1, second - first - 1)), appearance.modelId) ||
                !Number(Trim(line.substr(second + 1)), appearance.textureId))
                return Failure(error, "Retail equipment rows must be WrathDisplayID;modelFileDataID;textureFileDataID (positive IDs)");
            if (!handItems.emplace(display, appearance).second) return Failure(error, "Duplicate Retail equipment display ID");
        }
    }
    s.names = std::move(names);
    s.handItems = std::move(handItems);
    s.configured = true;
    return true;
    } catch (const std::filesystem::filesystem_error& exception) {
        error = std::string("Invalid or inaccessible Retail CASC path: ") + exception.what();
        s.configurationError = error;
        return false;
    }
}

bool CascSource::HandItem(uint32_t displayId, HandItemAppearance& appearance, bool mainHand) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (mainHand && m_impl->mainHandPreview.modelId) {
        appearance = m_impl->mainHandPreview;
        return true;
    }
    const auto found = m_impl->handItems.find(displayId);
    if (found == m_impl->handItems.end()) return false;
    appearance = found->second;
    return true;
}

bool CascSource::SetHandItem(uint32_t displayId, HandItemAppearance appearance) {
    if (!displayId || !appearance.modelId || !appearance.textureId) return false;
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->configured) return false;
    m_impl->handItems[displayId] = appearance;
    return true;
}

bool CascSource::SetMainHandPreview(HandItemAppearance appearance) {
    if (!appearance.modelId || !appearance.textureId) return false;
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->configured) return false;
    m_impl->mainHandPreview = appearance;
    return true;
}

bool CascSource::Handles(const char* name) {
    if (!name) return false;
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    const auto key = Normalize(name);
    return key.compare(0, m_impl->Prefix().size(), m_impl->Prefix()) == 0 || m_impl->names.count(key) || m_impl->dependencies.count(key);
}

bool CascSource::HasClientTable(const char* name) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    const auto key = Normalize(name ? name : "");
    return m_impl->configured && !m_impl->tables.empty() &&
        TableName(key) && std::filesystem::is_regular_file(std::filesystem::path(m_impl->tables) / key.substr(14));
}

bool CascSource::MergeTable(const char* name, const std::vector<uint8_t>& base,
    std::vector<uint8_t>& merged, std::string& error) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    const auto key = Normalize(name ? name : "");
    if (!TableName(key))
        return Failure(error, "Unsupported supplemental table name");
    const auto table = key.substr(14);
    std::ifstream input(std::filesystem::path(m_impl->tables) / table, std::ios::binary | std::ios::ate);
    if (!input) return Failure(error, "Missing configured supplemental client table");
    const auto size = input.tellg();
    if (size < 0 || size > 128 * 1024 * 1024) return Failure(error, "Supplemental table exceeds size limit");
    std::vector<uint8_t> additions(static_cast<size_t>(size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(additions.data()), additions.size())) return Failure(error, "Cannot read supplemental table");
    return MergeClientTable(table, base, additions, merged, error);
}

// DB2 tables hold sections encrypted for unreleased content; stock clients skip those, so a
// missing key must not fail the table. Unreadable blocks come back zero-filled.
bool CascSource::ReadTable(const char* name, std::vector<uint8_t>& bytes, std::string& error) {
    error.clear();
    bytes.clear();
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    Reference ref;
    std::string internal;
    return m_impl->Resolve(Normalize(name ? name : ""), ref, internal, error) && m_impl->Fetch(ref, internal, bytes, error, true);
}

bool CascSource::Exists(const char* name, std::string& error) {
    error.clear();
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    Reference ref;
    std::string internal;
    File file;
    if (!m_impl->Resolve(Normalize(name ? name : ""), ref, internal, error)) return false;
    // Custom: shipped original files
    const auto packed = m_impl->PackPath(ref);
    if (!packed.empty() && std::filesystem::is_regular_file(packed)) return true;
    if (m_impl->files) return ref.id && std::filesystem::is_regular_file(std::filesystem::path(m_impl->path) / (std::to_string(ref.id) + ".bin"));
    std::vector<uint8_t> cached;
    const auto path = m_impl->CachePath(ref, internal, false);
    if (!path.empty() && ReadDecodedCache(path, cached)) return true;
    return m_impl->OpenFile(ref, internal, file, error);
}

bool CascSource::Inspect(const char* name, std::vector<uint8_t>& bytes, std::string& error) {
    error.clear();
    bytes.clear();
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    Reference ref;
    std::string internal;
    return m_impl->Resolve(Normalize(name ? name : ""), ref, internal, error) && m_impl->Fetch(ref, internal, bytes, error);
}

bool CascSource::Read(const char* name, std::vector<uint8_t>& bytes, std::string& error) {
    SourceScope scope(m_impl->kind);
    error.clear();
    bytes.clear();
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    const std::string key = Normalize(name ? name : "");
    Reference ref;
    std::string internal;
    auto& s = *m_impl;
    if (!s.Resolve(key, ref, internal, error) || !s.Fetch(ref, internal, bytes, error)) return false;
    const auto ext = Extension(key);
    bool success = true;
    if (ext == ".m2" || ext == ".mdx") {
        auto model = std::make_shared<Model>();
        model->slimeGiant = s.kind == SourceKind::Retail && SlimeGiant::PathModel(key) == ref.id;
        model->centaur = s.kind == SourceKind::Retail && Centaur::PathModel(key) == ref.id;
        model->centaurFemale = model->centaur && ref.id == Centaur::Bodies[1];
        model->character = model->slimeGiant || model->centaur || s.characterModels.count(ref.id) != 0;
        // A shared skeleton lives in its own file; fetch it before the model is prepared.
        ModelFiles inspected;
        std::vector<uint8_t> skeleton;
        if (InspectModel(bytes, inspected, error) && inspected.skeleton) {
            std::vector<uint32_t> chain;
            success = s.LoadSkeleton(inspected.skeleton, skeleton, error, chain);
        }
        success = success && PrepareModel(bytes, model->bytes, model->files, error, skeleton.empty() ? nullptr : &skeleton);
        if (success && model->slimeGiant) success = SlimeGiant::PrepareModel(model->bytes, ref.id, error);
        if (success && model->centaur) success = Centaur::PrepareModel(model->bytes, ref.id, error);
        std::map<std::string, Reference> pending;
        const auto base = key.substr(0, key.find_last_of('.'));
        // Check every advertised profile before exposing the model to the renderer.
        for (size_t i = 0; success && i < model->files.skins.size(); ++i) {
            if (i >= 4 || i >= (model->bytes.size() > 0x44 ? model->bytes[0x44] : 0)) break;
            Reference skin{model->files.skins[i], model};
            std::vector<uint8_t> raw;
            success = s.Fetch(skin, "", raw, error);
            if (success && model->slimeGiant) success = SlimeGiant::PrepareSkin(raw, error);
            if (success && model->centaur) success = Centaur::PrepareSkin(raw, error, model->centaurFemale);
            success = success && PrepareSkin(raw, model->bytes, error, model->character || model->files.skeleton != 0);
            char suffix[16];
            std::snprintf(suffix, sizeof(suffix), "%02u.skin", static_cast<unsigned>(i));
            pending.emplace(base + suffix, std::move(skin));
        }
        for (const auto& anim : model->files.animations) {
            if (!anim.fileId) continue;
            char suffix[32];
            std::snprintf(suffix, sizeof(suffix), "%04u-%02u.anim", anim.id, anim.variation);
            auto added = pending.emplace(base + suffix, Reference{anim.fileId, model, anim.id, anim.variation});
            if (!added.second && added.first->second.id != anim.fileId) success = Failure(error, "Conflicting AFID entries");
        }
        if (success) {
            // Same name always denotes the same file within a source's pinned build.
            auto& owner = s.models[key];
            if (!owner.first) owner.first = model;
            ++owner.second;
            for (auto& dep : pending) {
                dep.second.owner = owner.first;
                s.dependencies[dep.first] = dep.second;
            }
            bytes = model->bytes;
        }
    } else if (ext == ".skin") {
        success = ref.owner != nullptr;
        if (!success) error = "Open the Retail model before its skin";
        if (success && ref.owner->slimeGiant) success = SlimeGiant::PrepareSkin(bytes, error);
        if (success && ref.owner->centaur) success = Centaur::PrepareSkin(bytes, error, ref.owner->centaurFemale);
        success = success && PrepareSkin(bytes, ref.owner->bytes, error, ref.owner->character || ref.owner->files.skeleton != 0);
    } else if (ext == ".anim") {
        success = ref.owner ? (StripAnimationChunk(bytes, error) && ValidateAnimation(bytes, ref.owner->bytes, ref.animation, ref.variation, error)) :
            Failure(error, "Open the Retail model before its animation");
    } else if (ext == ".blp") {
        // BLP2 format version 1 is the existing client's native texture format.
        success = bytes.size() >= 148 && std::memcmp(bytes.data(), "BLP2\1\0\0\0", 8) == 0;
        if (!success) error = "Retail texture is not a supported BLP2 file";
    } else {
        success = Failure(error, "Retail runtime supports M2, SKIN, ANIM and BLP references only");
    }
    if (!success) bytes.clear();
    else s.cachedSizes[key] = static_cast<uint32_t>(bytes.size());
    return success;
}

bool CascSource::CachedSize(const char* name, uint32_t& size) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    const auto found = m_impl->cachedSizes.find(Normalize(name ? name : ""));
    if (found == m_impl->cachedSizes.end()) return false;
    size = found->second;
    return true;
}

bool CascSource::CachedOriginalSize(const char* name, uint32_t& size) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    size = 0;
    const auto key = Normalize(name ? name : "");
    const auto prepared = m_impl->cachedSizes.find(key);
    if (prepared != m_impl->cachedSizes.end()) { size = prepared->second; return true; }
    Reference ref;
    std::string internal, error;
    if (!m_impl->Resolve(key, ref, internal, error)) return false;
    // Custom: shipped original files
    const auto packed = m_impl->PackPath(ref);
    if (!packed.empty()) {
        std::error_code ec;
        const auto length = std::filesystem::file_size(packed, ec);
        if (!ec && length <= 128 * 1024 * 1024) { size = static_cast<uint32_t>(length); return true; }
    }
    if (m_impl->files) {
        std::error_code ec;
        const auto length = std::filesystem::file_size(std::filesystem::path(m_impl->path) / (std::to_string(ref.id) + ".bin"), ec);
        if (!ref.id || ec || length > 128 * 1024 * 1024) return false;
        size = static_cast<uint32_t>(length);
        return true;
    }
    const auto path = m_impl->CachePath(ref, internal, false);
    std::vector<uint8_t> bytes;
    if (path.empty() || !ReadDecodedCache(path, bytes)) return false;
    size = static_cast<uint32_t>(bytes.size());
    return true;
}

void CascSource::ReleaseModel(const char* name) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    const auto model = m_impl->models.find(Normalize(name ? name : ""));
    if (model == m_impl->models.end() || --model->second.second) return;
    for (auto it = m_impl->dependencies.begin(); it != m_impl->dependencies.end();) {
        if (it->second.owner == model->second.first) it = m_impl->dependencies.erase(it);
        else ++it;
    }
    m_impl->models.erase(model);
}

CascSource::Statistics CascSource::Stats() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->stats;
}

CascSource& Source() {
    static CascSource retail, forever(SourceKind::Forever);
    static std::once_flag retailOnce, foreverOnce;
    const bool isForever = currentSource == SourceKind::Forever;
    auto& source = isForever ? forever : retail;
    std::call_once(isForever ? foreverOnce : retailOnce, [&] {
        const char* path = std::getenv(isForever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG");
        const char* filename = isForever ? "ForeverCasc.ini" : "RetailCasc.ini";
        std::string error, fallback = filename;
#ifdef _WIN32
        if (!path || !*path) {
            // Launching the .exe directly: use the installed pack's config beside it (Data\\Retail\\RetailCasc.ini).
            char exe[MAX_PATH];
            const DWORD length = GetModuleFileNameA(nullptr, exe, sizeof(exe));
            if (length && length < sizeof(exe)) {
                const auto data = std::filesystem::path(exe).parent_path() / "Data";
                const auto installed = data / "Retail" / filename;
                const auto legacy = data / "Forever" / filename;
                std::error_code code;
                if (std::filesystem::exists(installed, code)) fallback = installed.string();
                else if (isForever && std::filesystem::exists(legacy, code)) fallback = legacy.string();
            }
        }
#endif
        if (!source.Configure(path && *path ? path : fallback.c_str(), error))
            std::fprintf(stderr, "%s CASC: %s\n", isForever ? "Forever" : "Retail", error.c_str());
    });
    return source;
}
CascSource& SourceForPath(const char* name) {
    const auto key = Normalize(name ? name : "");
    SourceScope scope(key.compare(0, 8, "forever/") == 0 ? SourceKind::Forever : SourceKind::Retail);
    return Source();
}
void SetPackMissHandler(PackMissHandler handler) { packMissHandler = handler; }
}
