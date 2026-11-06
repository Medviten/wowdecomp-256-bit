#ifndef WOW_SOURCE_DB_RACEMASK_H
#define WOW_SOURCE_DB_RACEMASK_H

#include <cstdint>

class CDataStore;
class SFileImpl;

// Custom: 256-bit race mask, bit (race - 1) set per race, so race ids 1-255 fit. Stored exactly as in the DBC files
// written by DBCTool-256-bit: 8 little-endian uint32 fields, races 1-32 first. Replaces the stock int32 race masks of
// Faction, SkillLineAbility, SkillRaceClassInfo and TalentTab, and HelmetGeosetVisData's hide masks, which index
// their bits by race id itself (see HasBit()). Item queries and who requests carry it as 8 uint32 too.
//
// Custom: the client also reads the stock 32-bit layout, so one binary works with both the 256-bit and the 32-bit
// server. Each of those DBCs is loaded in whichever layout its header has (a 32-bit mask is zero-extended, as
// DBCTool-256-bit does, so -1 stays races 1-32). Item queries, who requests and itemcache.wdb use the packet format:
// CVar raceMaskBits = 32 or 256, or 0 (default) for 256 when any of those DBCs is 256-bit and 32 otherwise.
struct RaceMask {
    static const int32_t s_numWords = 8;

    // Words per mask in the DBC being loaded (set by WowClientDB_Common::Load) and in packets
    static void SetDBCWords(int32_t words);
    static int32_t GetPacketWords();
    static void SetPacketBits(int32_t bits);
    static bool ReadDBC(SFileImpl* file, RaceMask* masks, int32_t count);
    void GetPacket(CDataStore* msg);
    void PutPacket(CDataStore* msg) const;

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

    void SetBit(int32_t bit) {
        if (bit >= 0 && bit < s_numWords * 32) {
            this->m_words[bit / 32] |= 1u << (bit % 32);
        }
    }

    // True if at least two bits are set (the 32-bit idiom `(mask - 1) & mask`)
    bool HasMultipleBits() const {
        int32_t count = 0;

        for (int32_t i = 0; i < s_numWords; i++) {
            if (this->m_words[i]) {
                if ((this->m_words[i] - 1) & this->m_words[i]) {
                    return true;
                }

                count++;
            }
        }

        return count > 1;
    }

    static RaceMask All() {
        RaceMask mask;

        for (int32_t i = 0; i < s_numWords; i++) {
            mask.m_words[i] = 0xFFFFFFFF;
        }

        return mask;
    }

    static RaceMask None() {
        RaceMask mask;

        for (int32_t i = 0; i < s_numWords; i++) {
            mask.m_words[i] = 0;
        }

        return mask;
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
