#include "WoW/Source/DB/Rec/FactionRec.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "WoW/Source/Client.h"

FactionRec::FactionRec() {
}

FactionRec::~FactionRec() {
}

// VERIFY: ebc644d5 auto
// OFFSET: 0x008AF1C0
const char* FactionRec::GetFilename() {
    return "DBFilesClient\\Faction.dbc";
}

// VERIFY: a5ce2ad2 auto
// OFFSET: 0x008AF1D0
bool FactionRec::Read(SFileImpl* file, const char* stringBuffer) {
    uint32_t nameOffset[16];
    uint32_t nameFlags;
    uint32_t descriptionOffset[16];
    uint32_t descriptionFlags;

    int32_t idRead = SFileReadFile(file, &this->m_id, sizeof(this->m_id), nullptr, nullptr, 0);
    int32_t reputationIndexRead = SFileReadFile(file, &this->m_reputationIndex, sizeof(this->m_reputationIndex), nullptr, nullptr, 0);
    // Custom: 32-bit or 256-bit masks, whichever this DBC has (see RaceMask.h)
    int32_t reputationRaceMaskRead = RaceMask::ReadDBC(file, this->m_reputationRaceMask, 4);
    int32_t reputationClassMaskRead = SFileReadFile(file, this->m_reputationClassMask, sizeof(this->m_reputationClassMask), nullptr, nullptr, 0);
    int32_t reputationBaseRead = SFileReadFile(file, this->m_reputationBase, sizeof(this->m_reputationBase), nullptr, nullptr, 0);
    int32_t reputationFlagsRead = SFileReadFile(file, this->m_reputationFlags, sizeof(this->m_reputationFlags), nullptr, nullptr, 0);
    int32_t parentFactionIDRead = SFileReadFile(file, &this->m_parentFactionID, sizeof(this->m_parentFactionID), nullptr, nullptr, 0);
    int32_t parentFactionModRead = SFileReadFile(file, this->m_parentFactionMod, sizeof(this->m_parentFactionMod), nullptr, nullptr, 0);
    int32_t parentFactionCapRead = SFileReadFile(file, this->m_parentFactionCap, sizeof(this->m_parentFactionCap), nullptr, nullptr, 0);
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
    int32_t descriptionRead0 = SFileReadFile(file, &descriptionOffset[0], sizeof(descriptionOffset[0]), nullptr, nullptr, 0);
    int32_t descriptionRead1 = SFileReadFile(file, &descriptionOffset[1], sizeof(descriptionOffset[1]), nullptr, nullptr, 0);
    int32_t descriptionRead2 = SFileReadFile(file, &descriptionOffset[2], sizeof(descriptionOffset[2]), nullptr, nullptr, 0);
    int32_t descriptionRead3 = SFileReadFile(file, &descriptionOffset[3], sizeof(descriptionOffset[3]), nullptr, nullptr, 0);
    int32_t descriptionRead4 = SFileReadFile(file, &descriptionOffset[4], sizeof(descriptionOffset[4]), nullptr, nullptr, 0);
    int32_t descriptionRead5 = SFileReadFile(file, &descriptionOffset[5], sizeof(descriptionOffset[5]), nullptr, nullptr, 0);
    int32_t descriptionRead6 = SFileReadFile(file, &descriptionOffset[6], sizeof(descriptionOffset[6]), nullptr, nullptr, 0);
    int32_t descriptionRead7 = SFileReadFile(file, &descriptionOffset[7], sizeof(descriptionOffset[7]), nullptr, nullptr, 0);
    int32_t descriptionRead8 = SFileReadFile(file, &descriptionOffset[8], sizeof(descriptionOffset[8]), nullptr, nullptr, 0);
    int32_t descriptionRead9 = SFileReadFile(file, &descriptionOffset[9], sizeof(descriptionOffset[9]), nullptr, nullptr, 0);
    int32_t descriptionRead10 = SFileReadFile(file, &descriptionOffset[10], sizeof(descriptionOffset[10]), nullptr, nullptr, 0);
    int32_t descriptionRead11 = SFileReadFile(file, &descriptionOffset[11], sizeof(descriptionOffset[11]), nullptr, nullptr, 0);
    int32_t descriptionRead12 = SFileReadFile(file, &descriptionOffset[12], sizeof(descriptionOffset[12]), nullptr, nullptr, 0);
    int32_t descriptionRead13 = SFileReadFile(file, &descriptionOffset[13], sizeof(descriptionOffset[13]), nullptr, nullptr, 0);
    int32_t descriptionRead14 = SFileReadFile(file, &descriptionOffset[14], sizeof(descriptionOffset[14]), nullptr, nullptr, 0);
    int32_t descriptionRead15 = SFileReadFile(file, &descriptionOffset[15], sizeof(descriptionOffset[15]), nullptr, nullptr, 0);
    int32_t descriptionFlagsRead = SFileReadFile(file, &descriptionFlags, sizeof(descriptionFlags), nullptr, nullptr, 0);

    if (!idRead
        | !reputationIndexRead
        | !reputationRaceMaskRead
        | !reputationClassMaskRead
        | !reputationBaseRead
        | !reputationFlagsRead
        | !parentFactionIDRead
        | !parentFactionModRead
        | !parentFactionCapRead
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
        | !descriptionRead0
        | !descriptionRead1
        | !descriptionRead2
        | !descriptionRead3
        | !descriptionRead4
        | !descriptionRead5
        | !descriptionRead6
        | !descriptionRead7
        | !descriptionRead8
        | !descriptionRead9
        | !descriptionRead10
        | !descriptionRead11
        | !descriptionRead12
        | !descriptionRead13
        | !descriptionRead14
        | !descriptionRead15
        | !descriptionFlagsRead) {
        return false;
    }

    if (stringBuffer) {
        this->m_name = &stringBuffer[nameOffset[s_locale]];
        this->m_description = &stringBuffer[descriptionOffset[s_locale]];
    } else {
        this->m_name = "";
        this->m_description = "";
    }

    return true;
}
