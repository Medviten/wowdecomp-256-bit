#ifndef WOW_SOURCE_DB_REC_SKILLRACECLASSINFOREC_H
#define WOW_SOURCE_DB_REC_SKILLRACECLASSINFOREC_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/DB/RaceMask.h"

class SFileImpl;

class SkillRaceClassInfoRec {
    public:
    static const int32_t s_numColumns = 15;
    static const int32_t s_rowSize = 60;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_skillID;
    /* 0008 */ RaceMask m_raceMask;
    /* 0028 */ int32_t m_classMask;
    /* 002C */ int32_t m_flags;
    /* 0030 */ int32_t m_minLevel;
    /* 0034 */ int32_t m_skillTierID;
    /* 0038 */ int32_t m_skillCostIndex;

    SkillRaceClassInfoRec();
    ~SkillRaceClassInfoRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
