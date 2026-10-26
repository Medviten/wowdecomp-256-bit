#ifndef CENTAUR_APPEARANCE_H
#define CENTAUR_APPEARANCE_H

#include <cstdint>

namespace CentaurAppearance
{
    inline constexpr uint32_t HideLegArmorFlag = 0x10000000;

    constexpr bool Hidden(uint32_t race, uint32_t playerFlags)
    {
        return (race == 73 || race == 74) && (playerFlags & HideLegArmorFlag) != 0;
    }

    static_assert(Hidden(73, HideLegArmorFlag) && Hidden(74, HideLegArmorFlag));
    static_assert(!Hidden(73, 0) && !Hidden(1, HideLegArmorFlag));
}

#endif
