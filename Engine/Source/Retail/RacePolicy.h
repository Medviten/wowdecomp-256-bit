#ifndef ENGINE_SOURCE_RETAIL_RACEPOLICY_H
#define ENGINE_SOURCE_RETAIL_RACEPOLICY_H
#include <cstdint>
#include "Engine/Source/Retail/SourceContext.h"

namespace Retail {
// The race's own ChrRaceXChrModel row is its native form (Dracthyr: the Dragon, ChrModel 89). The Visage models (127 ->
// 4395382 male, 128 -> 4220448 female) are reached explicitly, as a form (app::FormVisage), not as the race default.
inline uint32_t CharacterModel(uint32_t /*race*/, uint32_t /*sex*/, uint32_t defaultModel) {
    return defaultModel;
}
// Helmet masks describe visual anatomy, not faction/skill eligibility. Always bound the shift:
// target custom races above 31 cannot index the installed 32-bit HelmetGeosetVisData directly.
inline uint32_t HelmetMaskRace(uint32_t retailRace) {
    switch (retailRace) {
        case 0xf0010000u: return 1;          // SlimeSkeleton shares Kul Tiran's Human helmet-mask alias.
        case 0xf0020000u: return 11;         // Centaur legacy helmets use the fitted Draenei family.
        case 27: return 4;                  // Nightborne -> Night Elf
        case 28: return 6;                  // Highmountain -> Tauren
        case 29: return 10;                 // Void Elf -> Blood Elf
        case 30: return 11;                 // Lightforged -> Draenei
        case 31: return 8;                  // Zandalari -> Troll
        case 32: return 1;                  // Kul Tiran -> Human
        case 34: case 84: case 85: return 3; // Dark Iron / Earthen -> Dwarf
        case 35: return 9;                  // Vulpera -> Goblin
        case 36: return 2;                  // Mag'har -> Orc
        case 37: return 7;                  // Mechagnome -> Gnome
        case 52: case 70: return 10;        // Dracthyr Visage -> Blood Elf (male donor)
        case 95: case 96: return 10;        // Forever Skyborne -> Blood Elf
        case 86: case 91: return 4;         // Haranir -> Night Elf (both factions)
        default: return retailRace < 32 ? retailRace : 0;
    }
}

inline bool UsesLegacyClosedFeet(uint32_t retailRace) {
    switch (retailRace) {
        case 1: case 2: case 3: case 4: case 5: case 7: case 9: case 10:
        case 24: case 25: case 26: case 27: case 29: case 32: case 34: case 36:
            return true;
        default: return false; // Preserve hooves, exposed paws/toes and authored mechanical/Feet choices.
    }
}
}
#endif
