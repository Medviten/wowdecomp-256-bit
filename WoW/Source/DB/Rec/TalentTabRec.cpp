#include "WoW/Source/DB/Rec/TalentTabRec.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "WoW/Source/Client.h"

TalentTabRec::TalentTabRec() {
}

TalentTabRec::~TalentTabRec() {
}

// VERIFY: cc2af630 auto
// OFFSET: 0x008B9AC0
const char* TalentTabRec::GetFilename() {
    return "DBFilesClient\\TalentTab.dbc";
}

// VERIFY: 3e197217 auto
// OFFSET: 0x008B9AD0
bool TalentTabRec::Read(SFileImpl* file, const char* stringBuffer) {
    uint32_t nameOffset[16];
    uint32_t nameFlags;
    uint32_t backgroundFileOffset;

    int32_t idRead = SFileReadFile(file, &this->m_id, sizeof(this->m_id), nullptr, nullptr, 0);
    int32_t nameRead0 = SFileReadFile(file, &nameOffset[0], sizeof(nameOffset[0]), nullptr, nullptr, 0);
    int32_t nameRead1 = SFileReadFile(file, &nameOffset[1], sizeof(nameOffset[1]), nullptr, nullptr, 0);
    int32_t nameRead2 = SFileReadFile(file, &nameOffset[2], sizeof(nameOffset[2]), nullptr, nullptr, 0);
    int32_t nameRead3 = SFileReadFile(file, &nameOffset[3], sizeof(nameOffset[3]), nullptr, nullptr, 0);
    int32_t nameRead4 = SFileReadFile(file, &nameOffset[4], sizeof(nameOffset[4]), nullptr, nullptr, 0);
    int32_t nameRead5 = SFileReadFile(file, &nameOffset[5], sizeof(nameOffset[5]), nullptr, nullptr, 0);
    int32_t nameRead6 = SFileReadFile(file, &nameOffset[6], sizeof(nameOffset[6]), nullptr, nullptr, 0);
    int32_t nameRead7 = SFileReadFile(file, &nameOffset[7], sizeof(nameOffset[7]), nullptr, nullptr, 0);
    int32_t nameRead8 = SFileReadFile(file, &nameOffset[8], sizeof(nameOffset[8]), nullptr, nullptr, 0);
    int32_t nameRead9 = SFileReadFile(file, &nameOffset[9], sizeof(nameOffset[9]), nullptr, nullptr, 0);
    int32_t nameRead10 = SFileReadFile(file, &nameOffset[10], sizeof(nameOffset[10]), nullptr, nullptr, 0);
    int32_t nameRead11 = SFileReadFile(file, &nameOffset[11], sizeof(nameOffset[11]), nullptr, nullptr, 0);
    int32_t nameRead12 = SFileReadFile(file, &nameOffset[12], sizeof(nameOffset[12]), nullptr, nullptr, 0);
    int32_t nameRead13 = SFileReadFile(file, &nameOffset[13], sizeof(nameOffset[13]), nullptr, nullptr, 0);
    int32_t nameRead14 = SFileReadFile(file, &nameOffset[14], sizeof(nameOffset[14]), nullptr, nullptr, 0);
    int32_t nameRead15 = SFileReadFile(file, &nameOffset[15], sizeof(nameOffset[15]), nullptr, nullptr, 0);
    int32_t nameFlagsRead = SFileReadFile(file, &nameFlags, sizeof(nameFlags), nullptr, nullptr, 0);
    int32_t spellIconIDRead = SFileReadFile(file, &this->m_spellIconID, sizeof(this->m_spellIconID), nullptr, nullptr, 0);
    // Custom: 32-bit or 256-bit masks, whichever this DBC has (see RaceMask.h)
    int32_t raceMaskRead = RaceMask::ReadDBC(file, &this->m_raceMask, 1);
    int32_t classMaskRead = SFileReadFile(file, &this->m_classMask, sizeof(this->m_classMask), nullptr, nullptr, 0);
    int32_t categoryEnumIDRead = SFileReadFile(file, &this->m_categoryEnumID, sizeof(this->m_categoryEnumID), nullptr, nullptr, 0);
    int32_t orderIndexRead = SFileReadFile(file, &this->m_orderIndex, sizeof(this->m_orderIndex), nullptr, nullptr, 0);
    int32_t backgroundFileRead = SFileReadFile(file, &backgroundFileOffset, sizeof(backgroundFileOffset), nullptr, nullptr, 0);

    if (!idRead
        | !nameRead0
        | !nameRead1
        | !nameRead2
        | !nameRead3
        | !nameRead4
        | !nameRead5
        | !nameRead6
        | !nameRead7
        | !nameRead8
        | !nameRead9
        | !nameRead10
        | !nameRead11
        | !nameRead12
        | !nameRead13
        | !nameRead14
        | !nameRead15
        | !nameFlagsRead
        | !spellIconIDRead
        | !raceMaskRead
        | !classMaskRead
        | !categoryEnumIDRead
        | !orderIndexRead
        | !backgroundFileRead) {
        return false;
    }

    if (stringBuffer) {
        this->m_name = &stringBuffer[nameOffset[s_locale]];
        this->m_backgroundFile = &stringBuffer[backgroundFileOffset];
    } else {
        this->m_name = "";
        this->m_backgroundFile = "";
    }

    return true;
}
