#ifndef ENGINE_SOURCE_RETAIL_DECODEDCACHE_H
#define ENGINE_SOURCE_RETAIL_DECODEDCACHE_H
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Retail {
// Completed entries only: bounded payload, exact length and checksum. Cache keys include
// build/product, locale and read policy at the caller. No prepared client bytes are stored.
bool ReadDecodedCache(const std::filesystem::path& path, std::vector<uint8_t>& bytes);
bool WriteDecodedCache(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string& error);
}
#endif
