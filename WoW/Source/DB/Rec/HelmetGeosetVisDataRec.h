#ifndef WOW_SOURCE_DB_REC_HELMETGEOSETVISDATAREC_H
#define WOW_SOURCE_DB_REC_HELMETGEOSETVISDATAREC_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/DB/RaceMask.h"

class SFileImpl;

class HelmetGeosetVisDataRec {
    public:
    static const int32_t s_numColumns = 57;
    static const int32_t s_rowSize = 228;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    // Custom: 256-bit masks (see RaceMask.h), bit = race id
    /* 0004 */ RaceMask m_hideGeoset [7];

    HelmetGeosetVisDataRec();
    ~HelmetGeosetVisDataRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
