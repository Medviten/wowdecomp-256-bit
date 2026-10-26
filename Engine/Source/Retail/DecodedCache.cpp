#include "Engine/Source/Retail/DecodedCache.h"
#include <atomic>
#include <chrono>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Retail {
namespace {
constexpr uint32_t magic = 0x31434457; // WDC1, decoded cache format 1
constexpr uint32_t limit = 128 * 1024 * 1024;
uint64_t Checksum(const std::vector<uint8_t>& bytes) {
    uint64_t hash = 14695981039346656037ull;
    for (uint8_t byte : bytes) { hash ^= byte; hash *= 1099511628211ull; }
    return hash;
}
}
bool ReadDecodedCache(const std::filesystem::path& path, std::vector<uint8_t>& bytes) {
    bytes.clear();
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    if (!input || size < 16 || size > uint64_t(limit) + 16) return false;
    input.seekg(0);
    uint32_t tag = 0, count = 0;
    uint64_t checksum = 0;
    input.read(reinterpret_cast<char*>(&tag), 4);
    input.read(reinterpret_cast<char*>(&count), 4);
    input.read(reinterpret_cast<char*>(&checksum), 8);
    if (!input || tag != magic || count > limit || uint64_t(size) != uint64_t(count) + 16) return false;
    bytes.resize(count);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), count) || Checksum(bytes) != checksum) {
        bytes.clear();
        return false;
    }
    return true;
}
bool WriteDecodedCache(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string& error) {
    if (bytes.size() > limit) { error = "Decoded asset exceeds cache size limit"; return false; }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) { error = "Cannot create decoded asset cache: " + ec.message(); return false; }
    static std::atomic<uint64_t> serial{0};
    auto temporary = path;
    temporary += "." + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())
        + "." + std::to_string(++serial) + ".tmp";
    const uint32_t count = static_cast<uint32_t>(bytes.size());
    const uint64_t checksum = Checksum(bytes);
    bool written = false;
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(&magic), 4);
        output.write(reinterpret_cast<const char*>(&count), 4);
        output.write(reinterpret_cast<const char*>(&checksum), 8);
        output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        output.flush();
        written = bool(output);
        output.close();
        written = written && !output.fail();
    }
#ifdef _WIN32
    if (written) written = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (written) { std::filesystem::rename(temporary, path, ec); written = !ec; }
#endif
    if (!written) {
        std::filesystem::remove(temporary, ec);
        error = "Cannot persist decoded asset cache: " + path.string();
    }
    return written;
}
}
