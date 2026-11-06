#include "WoW/Source/DB/Rec/HelmetGeosetVisDataRec.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"

HelmetGeosetVisDataRec::HelmetGeosetVisDataRec() {
}

HelmetGeosetVisDataRec::~HelmetGeosetVisDataRec() {
}

// VERIFY: 8bbe5b41 auto
// OFFSET: 0x008B0060
const char* HelmetGeosetVisDataRec::GetFilename() {
    return "DBFilesClient\\HelmetGeosetVisData.dbc";
}

// VERIFY: 522798f8 auto
// OFFSET: 0x008B0070
bool HelmetGeosetVisDataRec::Read(SFileImpl* file, const char* stringBuffer) {
    int32_t idRead = SFileReadFile(file, &this->m_id, sizeof(this->m_id), nullptr, nullptr, 0);
    // Custom: 32-bit or 256-bit masks, whichever this DBC has (see RaceMask.h)
    int32_t hideGeosetRead = RaceMask::ReadDBC(file, this->m_hideGeoset, 7);

    if (!idRead
        | !hideGeosetRead) {
        return false;
    }

    return true;
}
