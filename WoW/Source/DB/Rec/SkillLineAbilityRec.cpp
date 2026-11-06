#include "WoW/Source/DB/Rec/SkillLineAbilityRec.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"

SkillLineAbilityRec::SkillLineAbilityRec() {
}

SkillLineAbilityRec::~SkillLineAbilityRec() {
}

// VERIFY: b9a78b7b auto
// OFFSET: 0x008B5CF0
const char* SkillLineAbilityRec::GetFilename() {
    return "DBFilesClient\\SkillLineAbility.dbc";
}

// VERIFY: f02255bf auto
// OFFSET: 0x008B5D00
bool SkillLineAbilityRec::Read(SFileImpl* file, const char* stringBuffer) {
    int32_t idRead = SFileReadFile(file, &this->m_id, sizeof(this->m_id), nullptr, nullptr, 0);
    int32_t skillLineRead = SFileReadFile(file, &this->m_skillLine, sizeof(this->m_skillLine), nullptr, nullptr, 0);
    int32_t spellRead = SFileReadFile(file, &this->m_spell, sizeof(this->m_spell), nullptr, nullptr, 0);
    // Custom: 32-bit or 256-bit masks, whichever this DBC has (see RaceMask.h)
    int32_t raceMaskRead = RaceMask::ReadDBC(file, &this->m_raceMask, 1);
    int32_t classMaskRead = SFileReadFile(file, &this->m_classMask, sizeof(this->m_classMask), nullptr, nullptr, 0);
    int32_t excludeRaceRead = SFileReadFile(file, &this->m_excludeRace, sizeof(this->m_excludeRace), nullptr, nullptr, 0);
    int32_t excludeClassRead = SFileReadFile(file, &this->m_excludeClass, sizeof(this->m_excludeClass), nullptr, nullptr, 0);
    int32_t minSkillLineRankRead = SFileReadFile(file, &this->m_minSkillLineRank, sizeof(this->m_minSkillLineRank), nullptr, nullptr, 0);
    int32_t supercededBySpellRead = SFileReadFile(file, &this->m_supercededBySpell, sizeof(this->m_supercededBySpell), nullptr, nullptr, 0);
    int32_t acquireMethodRead = SFileReadFile(file, &this->m_acquireMethod, sizeof(this->m_acquireMethod), nullptr, nullptr, 0);
    int32_t trivialSkillLineRankHighRead = SFileReadFile(file, &this->m_trivialSkillLineRankHigh, sizeof(this->m_trivialSkillLineRankHigh), nullptr, nullptr, 0);
    int32_t trivialSkillLineRankLowRead = SFileReadFile(file, &this->m_trivialSkillLineRankLow, sizeof(this->m_trivialSkillLineRankLow), nullptr, nullptr, 0);
    int32_t characterPointsRead = SFileReadFile(file, this->m_characterPoints, sizeof(this->m_characterPoints), nullptr, nullptr, 0);

    if (!idRead
        | !skillLineRead
        | !spellRead
        | !raceMaskRead
        | !classMaskRead
        | !excludeRaceRead
        | !excludeClassRead
        | !minSkillLineRankRead
        | !supercededBySpellRead
        | !acquireMethodRead
        | !trivialSkillLineRankHighRead
        | !trivialSkillLineRankLowRead
        | !characterPointsRead) {
        return false;
    }

    return true;
}
