#ifndef ENGINE_SOURCE_RETAIL_RACEGEOMETRY_H
#define ENGINE_SOURCE_RETAIL_RACEGEOMETRY_H

#include <cstdint>
#include <vector>
#include "Engine/Source/Retail/SourceContext.h"

namespace Retail {

// These anatomy groups are hidden by the retail visibility reset. A recipe only names groups
// controlled by its choices: Dwarves omit the head, while Night/Blood/Void Elves omit their
// separate hand mesh (23). Glove cuffs use group 4 and do not supply those fingers/palms.
// Explicit choices (including a hidden group value) always take precedence over these defaults.
inline std::vector<uint16_t> DefaultRaceGeosets(const std::vector<uint16_t>& recipe) {
    std::vector<uint16_t> defaults;
    for (const uint16_t base : {2001, 2201, 2301, 3202, 3301, 5101}) {
        bool chosen = false;
        for (const uint16_t geoset : recipe) chosen = chosen || geoset / 100 == base / 100;
        if (!chosen) defaults.push_back(base);
    }
    return defaults;
}

// Eye lightning is opt-in for every mapped character. Apply after Wrath's
// selector/DK pass so it cannot restore 1701/1703 behind the creator's toggle.
// Normal eyes (33) and authored eyesight/blindness overlays (51) stay independent.
template <class Model>
void ApplyRaceEyeGeometry(Model& model, bool lightning = false) {
    model.SetGeometryVisible(1701, 1799, 0);
    if (lightning) model.SetGeometryVisible(1701, 1701, 1);
}

// Legacy boot styles select the shin (group 5), not Retail foot shapes (group 20). Even style
// zero boots need the closed shoe mesh. A robe changes the shins but does not unequip the boots.
template <class Model>
void ApplyLegacyFootGeometry(Model& model, uint32_t bootsDisplayId, bool hasClosedFoot) {
    const uint16_t foot = bootsDisplayId && hasClosedFoot ? 2002 : 2001;
    model.SetGeometryVisible(2001, 2099, 0);
    model.SetGeometryVisible(foot, foot, 1);
}

}
#endif
