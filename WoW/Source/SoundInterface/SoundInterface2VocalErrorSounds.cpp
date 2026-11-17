#include <cstddef>
#include <cstdint>
#include <cstring>
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/SoundInterface/SoundInterface2.h"

// STATIC: 0x00DCEAF8
static int32_t s_vocalErrorSounds[68];

// STATIC: 0x00DCEC08
static SE2SoundKitObject s_vocalErrorObject;

// Custom: the race and sex the error voices were loaded for, so an altered form (a Worgen in Human Form) can switch
// them to the displayed race's (PlayVocalErrorSound); the original only loads them once, from the base race, at login
static int32_t s_vocalErrorRace = -1;
static uint32_t s_vocalErrorSex = 0;

// VERIFY: 2b3e2ff2 manual
// OFFSET: 0x00986C90
void SI2::InitVocalErrorSounds(int32_t race, uint32_t sex) {
    memset(s_vocalErrorSounds, 0, sizeof(s_vocalErrorSounds));

    // Custom: remember what was loaded (see s_vocalErrorRace)
    s_vocalErrorRace = race;
    s_vocalErrorSex = sex;

    if (sex > 1) {
        return;
    }

    for (int32_t i = 0; i < g_vocalUISoundsDB.m_numRecords; i++) {
        VocalUISoundsRec* rec = (i < 0 || i >= g_vocalUISoundsDB.m_numRecords) ? nullptr : &g_vocalUISoundsDB.m_records[i];

        if (rec && rec->m_vocalUienum < 68 && rec->m_raceID == race) {
            s_vocalErrorSounds[rec->m_vocalUienum] = rec->m_normalSoundID[sex];
        }
    }
}

// VERIFY: 535f9207 auto
// OFFSET: 0x00986D00
void SI2::PlayVocalErrorSound(int32_t error) {
    static CVar* s_enableErrorSpeech = CVar::Lookup("Sound_EnableErrorSpeech");

    if (!s_enableErrorSpeech || !s_enableErrorSpeech->m_intValue || error >= static_cast<int32_t>(sizeof(s_vocalErrorSounds) / sizeof(s_vocalErrorSounds[0]))) {
        return;
    }

    if (SI2::IsPlaying(&s_vocalErrorObject)) {
        return;
    }

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    // Custom: speak with the displayed race's voice, as the emotes do (GetDisplayRace: a Worgen in Human Form is a
    // Gilnean); fall back to the base race when the displayed one has no VocalUISounds rows
    if (player) {
        int32_t displayRace = static_cast<int32_t>(player->GetDisplayRace());
        uint32_t sex = player->m_player->m_playerBytes3[0];

        if (displayRace != s_vocalErrorRace || sex != s_vocalErrorSex) {
            SI2::InitVocalErrorSounds(displayRace, sex);

            int32_t baseRace = player->m_unit->m_unitFieldBytes0[0];
            bool any = false;

            for (int32_t id : s_vocalErrorSounds) {
                any = any || id != 0;
            }

            if (!any && displayRace != baseRace) {
                SI2::InitVocalErrorSounds(baseRace, sex);
                s_vocalErrorRace = displayRace;    // don't retry on every error
            }
        }
    }

    int32_t soundKitId = s_vocalErrorSounds[error];
    SE2SoundKitProperties properties;
    properties.ResetToDefaults();
    properties.m_group = 7;

    if (player && player->m_unit->m_unitFieldBytes0[1] == 6) {
        properties.m_useDspChain = 1;
        SI2::DK_GetPlayerFilterName(player, properties.m_dspChainName, sizeof(properties.m_dspChainName));
    }

    SI2::PlaySoundKit(soundKitId, nullptr, &s_vocalErrorObject, &properties, 0, nullptr, 1, 0);
}
