#ifndef WOW_SOURCE_DB_REC_FACTIONREC_H
#define WOW_SOURCE_DB_REC_FACTIONREC_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/DB/RaceMask.h"

class SFileImpl;

class FactionRec {
    public:
    static const int32_t s_numColumns = 85;
    static const int32_t s_rowSize = 340;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_reputationIndex;
    /* 0008 */ RaceMask m_reputationRaceMask [4];
    /* 0088 */ int32_t m_reputationClassMask [4];
    /* 0098 */ int32_t m_reputationBase [4];
    /* 00A8 */ int32_t m_reputationFlags [4];
    /* 00B8 */ int32_t m_parentFactionID;
    /* 00BC */ float m_parentFactionMod [2];
    /* 00C4 */ int32_t m_parentFactionCap [2];
    /* 00CC */ const char* m_name;
    /* 00D0 */ const char* m_description;

    FactionRec();
    ~FactionRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
