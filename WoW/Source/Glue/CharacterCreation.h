#ifndef WOW_SOURCE_GLUE_CHARACTERCREATION_H
#define WOW_SOURCE_GLUE_CHARACTERCREATION_H

#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Game/NameGen.h"
#include "WoW/Source/Glue/Types.h"

class CCharacterComponent;
class ChrClassesRec;
class ComponentData;
class CSimpleModelFFX;
struct CharacterPreferences;

class CCharacterCreation {
    public:
    // Custom: was [22][2] (race IDs 0-21); race IDs are uint8_t, so 256 covers every possible ChrRaces ID
    static const uint32_t MAX_RACES = 256;
    static CharacterPreferences* m_charPreferences[MAX_RACES][2];
    static CCharacterComponent* m_character;
    // Custom: retail Two Forms creator: a second preview showing the form the main character is not
    static CCharacterComponent* m_formMirror;
    static CNameGen m_nameGen;
    static CSimpleModelFFX* m_charCustomizeFrame;
    static float m_charFacing;
    static TSGrowableArray<int32_t> m_races;
    static TSFixedArray<ChrClassesRec*> m_classes;
    static int32_t m_existingCharacterIndex;
    static int32_t m_selectedClassID;
    static uint32_t m_raceIndex;
    static uint32_t m_prevSkinIndex;
    static uint32_t m_prevFaceIndex;
    static uint32_t m_prevHairColorIndex;
    static uint32_t m_prevHairStyleIndex;
    static uint32_t m_prevFacialHairIndex;

    static void CalcClasses(int32_t raceID);
    static void CreateCharacter(const char* name);
    static void CycleCharCustomization(CHAR_CUSTOMIZATION_TYPE type, int32_t delta);
    static void Dress();
    // Custom: retail races (RetailRace.cpp): one preview dressed or undressed, the Two Forms preview
    static void Dress(CCharacterComponent* target);
    static void Undress(CCharacterComponent* target);
    static void SyncFormMirror();
    static void ClearFormMirror();
    static int32_t GetExistingClassIndex();
    static int32_t GetExistingRaceIndex();
    static int32_t GetRandomClassID();
    static void GetRandomRaceAndSex(ComponentData* data);
    static int32_t Idle(const void* data, void* param);
    static void InitCharacterComponents(ComponentData* data, bool randomize);
    static void Initialize();
    static bool IsClassAvailable(int32_t classID);
    static void OnHide();
    static void RandomizeCharCustomization();
    static void ResetCharCustomizeInfo();
    static void SetCharacterFacing(float facing);
    // Custom: creator camera zoom and pan (Kalibro)
    static void ZoomCamera(float delta);
    static void PanCamera(float dx, float dy);
    static void ResetPanCamera();
    static void UpdateZoomCamera();
    static void ResetZoomCamera();
    static void SetCharCustomizeModel(const char* modelName);
    static void SetSelectedClass(int32_t classID);
    static void SetSelectedRace(uint32_t raceIndex);
    static void SetSelectedSex(uint32_t sexID);
    static void SetToExistingCharacter(uint32_t index);
    static void Shutdown();

    // Custom: Cataclysm-style altered form preview (alteredFormRaces). The altered form is a second character of the
    // altered race with the same choices, attached to the background model at attachment 1 (back) or 0 (front).
    static CCharacterComponent* m_alteredCharacter;
    static int32_t m_alteredFormAtFront;
    static void CreateAlteredCharacter();
    static void SyncAlteredCharacter();
    static void AttachCharacters();
    static void SetViewingAlteredForm(int32_t alteredForm);
    static void DressComponent(CCharacterComponent* component);
};

bool IsRaceClassValid(int32_t raceID, int32_t classID);
void CharacterCreateRegisterScriptFunctions();
void CharacterCreateUnregisterScriptFunctions();

#endif
