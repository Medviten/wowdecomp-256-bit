#ifndef WOW_SOURCE_DB_REC_TALENTTABREC_H
#define WOW_SOURCE_DB_REC_TALENTTABREC_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/DB/RaceMask.h"

class SFileImpl;

class TalentTabRec {
    public:
    static const int32_t s_numColumns = 31;
    static const int32_t s_rowSize = 124;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ int32_t m_spellIconID;
    /* 000C */ RaceMask m_raceMask;
    /* 002C */ int32_t m_classMask;
    /* 0030 */ int32_t m_categoryEnumID;
    /* 0034 */ int32_t m_orderIndex;
    /* 0038 */ const char* m_backgroundFile;

    TalentTabRec();
    ~TalentTabRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
