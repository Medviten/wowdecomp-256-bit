#ifndef ENGINE_SOURCE_RETAIL_CREATIONPOLICY_H
#define ENGINE_SOURCE_RETAIL_CREATIONPOLICY_H
#include "Engine/Source/Retail/SourceContext.h"
#include <cstdint>
#include <string_view>

namespace Retail {
inline bool AllowCreationChoice(SourceKind source, uint32_t model, uint32_t requirement, std::string_view name) {
    // Requirement 10 is the internal Transmog placeholder, including unnamed Forever copies.
    if (requirement == 10 || name == "Transmog") return false;
    // The pinned Forever Skyborne models copy Blood Elf choices under requirement 12.
    // Their playable choices instead have requirements 141/144/145 or authored dependencies.
    // Scope this exclusion to those two models; do not impose a blanket ReqType filter.
    return !(source == SourceKind::Forever && (model == 218 || model == 219) && requirement == 12);
}
}
#endif
