#ifndef WOW_SOURCE_DB_REC_SKILLLINEABILITYREC_H
#define WOW_SOURCE_DB_REC_SKILLLINEABILITYREC_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/DB/RaceMask.h"

class SFileImpl;

class SkillLineAbilityRec {
    public:
    static const int32_t s_numColumns = 21;
    static const int32_t s_rowSize = 84;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_skillLine;
    /* 0008 */ int32_t m_spell;
    /* 000C */ RaceMask m_raceMask;
    /* 002C */ int32_t m_classMask;
    /* 0030 */ int32_t m_excludeRace;
    /* 0034 */ int32_t m_excludeClass;
    /* 0038 */ int32_t m_minSkillLineRank;
    /* 003C */ int32_t m_supercededBySpell;
    /* 0040 */ int32_t m_acquireMethod;
    /* 0044 */ int32_t m_trivialSkillLineRankHigh;
    /* 0048 */ int32_t m_trivialSkillLineRankLow;
    /* 004C */ int32_t m_characterPoints [2];

    SkillLineAbilityRec();
    ~SkillLineAbilityRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
