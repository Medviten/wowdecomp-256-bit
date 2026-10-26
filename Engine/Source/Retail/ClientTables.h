#ifndef ENGINE_SOURCE_RETAIL_CLIENTTABLES_H
#define ENGINE_SOURCE_RETAIL_CLIENTTABLES_H
#include <cstdint>
#include <string>
#include <vector>

namespace Retail {
// Append-only WDBC metadata. Original model/texture files are not involved.
bool MergeClientTable(const std::string& table, const std::vector<uint8_t>& base,
    const std::vector<uint8_t>& additions, std::vector<uint8_t>& merged, std::string& error);
}
#endif
