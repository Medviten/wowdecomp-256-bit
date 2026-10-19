#ifndef WOW_SOURCE_DB_RACEMASK_H
#define WOW_SOURCE_DB_RACEMASK_H

#include <cstdint>

// Custom: 256-bit race mask, bit (race - 1) set per race, so race ids 1-255 fit. Stored exactly as in the DBC files
// written by DBCTool-256-bit: 8 little-endian uint32 fields, races 1-32 first. Replaces the stock int32 race masks of
// Faction, SkillLineAbility, SkillRaceClassInfo and TalentTab, and HelmetGeosetVisData's hide masks, which index
// their bits by race id itself (see HasBit()).
struct RaceMask {
    static const int32_t s_numWords = 8;

    /* 0000 */ uint32_t m_words[s_numWords];

    bool IsEmpty() const {
        for (int32_t i = 0; i < s_numWords; i++) {
            if (this->m_words[i]) {
                return false;
            }
        }

        return true;
    }

    bool HasBit(int32_t bit) const {
        if (bit < 0 || bit >= s_numWords * 32) {
            return false;
        }

        return (this->m_words[bit / 32] >> (bit % 32)) & 1;
    }

    bool HasRace(int32_t race) const {
        return race > 0 && this->HasBit(race - 1);
    }

    RaceMask operator~() const {
        RaceMask mask;

        for (int32_t i = 0; i < s_numWords; i++) {
            mask.m_words[i] = ~this->m_words[i];
        }

        return mask;
    }
};

static_assert(sizeof(RaceMask) == 32, "RaceMask must match the 8 uint32 DBC fields");

#endif
