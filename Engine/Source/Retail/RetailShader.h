#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace Retail {
// Keep the stock shadow/fog/alpha-test permutations and add Retail's glow layer.
bool AddGlowLayer(const std::vector<uint8_t>& source, std::vector<uint8_t>& result, std::string& error);
}
