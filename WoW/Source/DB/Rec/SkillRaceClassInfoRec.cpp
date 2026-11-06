#include "WoW/Source/DB/Rec/SkillRaceClassInfoRec.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"

SkillRaceClassInfoRec::SkillRaceClassInfoRec() {
}

SkillRaceClassInfoRec::~SkillRaceClassInfoRec() {
}

// VERIFY: a651a663 auto
// OFFSET: 0x008B6730
const char* SkillRaceClassInfoRec::GetFilename() {
    return "DBFilesClient\\SkillRaceClassInfo.dbc";
}

bool SkillRaceClassInfoRec::Read(SFileImpl* file, const char* stringBuffer) {
    int32_t idRead = SFileReadFile(file, &this->m_id, sizeof(this->m_id), nullptr, nullptr, 0);
    int32_t skillIDRead = SFileReadFile(file, &this->m_skillID, sizeof(this->m_skillID), nullptr, nullptr, 0);
    // Custom: 32-bit or 256-bit masks, whichever this DBC has (see RaceMask.h)
    int32_t raceMaskRead = RaceMask::ReadDBC(file, &this->m_raceMask, 1);
    int32_t classMaskRead = SFileReadFile(file, &this->m_classMask, sizeof(this->m_classMask), nullptr, nullptr, 0);
    int32_t flagsRead = SFileReadFile(file, &this->m_flags, sizeof(this->m_flags), nullptr, nullptr, 0);
    int32_t minLevelRead = SFileReadFile(file, &this->m_minLevel, sizeof(this->m_minLevel), nullptr, nullptr, 0);
    int32_t skillTierIDRead = SFileReadFile(file, &this->m_skillTierID, sizeof(this->m_skillTierID), nullptr, nullptr, 0);
    int32_t skillCostIndexRead = SFileReadFile(file, &this->m_skillCostIndex, sizeof(this->m_skillCostIndex), nullptr, nullptr, 0);

    if (!idRead
        | !skillIDRead
        | !raceMaskRead
        | !classMaskRead
        | !flagsRead
        | !minLevelRead
        | !skillTierIDRead
        | !skillCostIndexRead) {
        return false;
    }

    return true;
}
