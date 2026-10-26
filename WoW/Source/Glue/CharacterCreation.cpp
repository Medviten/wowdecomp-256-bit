#include "CharacterCreation.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include "WoW/Common/ComponentCore/RetailRace.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Services/Camera.h"
#include "Engine/Source/Services/CameraInt.h"
#include "Engine/Source/Tempest/CRandom.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/CharBaseInfoRec.h"
#include "WoW/Source/DB/Rec/CharStartOutfitRec.h"
#include "WoW/Source/DB/Rec/ChrClassesRec.h"
#include "WoW/Source/DB/Rec/ChrRacesRec.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/ItemRec.h"
#include "WoW/Source/Game/GameClient/CDrawBowString.h"
#include "WoW/Source/Glue/CGlueLoading.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/ComponentData.h"
#include "WoW/Source/Glue/CSimpleModelFFX.h"
#include "WoW/Source/Glue/Types.h"
#include "WoW/Source/LoadingScreen.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/WowServices/Types.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/Types.h"
#include "WoW/Source/Client.h"

// STATIC: 0x00AC4220
int32_t CCharacterCreation::m_selectedClassID = 1;

// STATIC: 0x00AC4224
int32_t CCharacterCreation::m_existingCharacterIndex = -1;

// STATIC: 0x00B6B0D0
CharacterPreferences* CCharacterCreation::m_charPreferences[256][2];

// STATIC: 0x00B6B180
uint32_t CCharacterCreation::m_raceIndex;

// STATIC: 0x00B6B184
CSimpleModelFFX* CCharacterCreation::m_charCustomizeFrame;

// STATIC: 0x00B6B188
float CCharacterCreation::m_charFacing;
// Custom: the retail Two Forms preview of the form the character is not showing
CCharacterComponent* CCharacterCreation::m_formMirror;

// STATIC: 0x00B6B18C
uint32_t CCharacterCreation::m_prevSkinIndex;

// STATIC: 0x00B6B190
uint32_t CCharacterCreation::m_prevFaceIndex;

// STATIC: 0x00B6B194
uint32_t CCharacterCreation::m_prevHairColorIndex;

// STATIC: 0x00B6B198
uint32_t CCharacterCreation::m_prevHairStyleIndex;

// STATIC: 0x00B6B19C
uint32_t CCharacterCreation::m_prevFacialHairIndex;

// STATIC: 0x00B6B1A0
CCharacterComponent* CCharacterCreation::m_character;

// STATIC: 0x00B6B1A4
TSFixedArray<ChrClassesRec*> CCharacterCreation::m_classes;

// STATIC: 0x00B6B1CC
TSGrowableArray<int32_t> CCharacterCreation::m_races;

// STATIC: 0x00B6B1DC
CNameGen CCharacterCreation::m_nameGen;

// Custom: altered form preview (see CharacterCreation.h)
CCharacterComponent* CCharacterCreation::m_alteredCharacter;
int32_t CCharacterCreation::m_alteredFormAtFront;
// Custom (Kalibro): creator camera zoom towards the face and pan (EA_ZOOM / EA_PAN in CycleCharCustomization)
namespace {
float s_creatorZoom = 0.0f;
// Camera pan in view distances (right, up); the screen-relative drag keeps the grabbed point under the cursor.
float s_creatorPanX = 0.0f;
float s_creatorPanY = 0.0f;
CCamera* s_creatorCamera = nullptr;
}

void CCharacterCreation::ZoomCamera(float delta) {
    if (std::isfinite(delta)) {
        s_creatorZoom = std::clamp(s_creatorZoom + delta / 12.0f, 0.0f, 1.0f);
        if (s_creatorZoom < 0.0001f) s_creatorZoom = 0.0f;
    }
}

void CCharacterCreation::PanCamera(float dx, float dy) {
    if (!std::isfinite(dx) || !std::isfinite(dy)) return;
    // Dragging moves the character with the cursor, so the camera moves the other way.
    s_creatorPanX = std::clamp(s_creatorPanX - dx, -1.0f, 1.0f);
    s_creatorPanY = std::clamp(s_creatorPanY - dy, -1.0f, 1.0f);
}

void CCharacterCreation::ResetPanCamera() {
    s_creatorPanX = s_creatorPanY = 0.0f;
}

void CCharacterCreation::ResetZoomCamera() {
    s_creatorZoom = 0.0f;
    ResetPanCamera();
    if (m_charCustomizeFrame && m_charCustomizeFrame->m_camera == s_creatorCamera && s_creatorCamera) {
        m_charCustomizeFrame->SetCameraByIndex(0);
    }
    if (s_creatorCamera) {
        HandleClose(reinterpret_cast<HOBJECT>(s_creatorCamera));
        s_creatorCamera = nullptr;
    }
}

void CCharacterCreation::UpdateZoomCamera() {
    auto* frame = m_charCustomizeFrame;
    auto* body = m_character ? m_character->m_data.m_model : nullptr;
    if (!frame || !frame->m_model || !body || !(body->m_fFlags & 1)
        || !(frame->m_model->m_fFlags & 1)) return;
    if (s_creatorZoom == 0.0f && s_creatorPanX == 0.0f && s_creatorPanY == 0.0f) {
        if (s_creatorCamera && frame->m_camera == s_creatorCamera) frame->SetCameraByIndex(0);
        return;
    }
    if (!frame->m_model->m_shared->m_data->m_cameras.m_count) return;
    auto* base = static_cast<CCamera*>(frame->m_model->GetCameraByIndex(0));
    if (!base) return;

    // Use the loaded body's head, including its pedestal transform and race-specific scale.
    // The stable bind pivot avoids making the camera bob with idle facial animations.
    const auto& bounds = body->m_shared->m_data->m_bounds.m_extent;
    const float height = bounds.t.z - bounds.b.z;
    C3Vector face = body->HasBone(6) ? body->GetBonePivot(6)
        : C3Vector(0.0f, 0.0f, bounds.b.z + height * 0.87f);
    face.z += height * 0.035f;
    face = face * body->GetAnimatedWorldTransform();
    C3Vector position, target;
    DataMgrGetCoord(base, 7, &position);
    DataMgrGetCoord(base, 8, &target);
    const auto offset = position - target;
    target += (face - target) * s_creatorZoom;
    position = target + offset * std::pow(0.18f, s_creatorZoom);
    // Screen axes of the view; the pan scales with the zoomed distance so closeups drag at the same feel.
    const C3Vector forward = target - position;
    const float distance = std::sqrt(forward.SquaredMag());
    C3Vector right = C3Vector::Cross(forward, C3Vector(0.0f, 0.0f, 1.0f));
    if (distance > 0.0f && right.SquaredMag() > 1e-8f) {
        right.Normalize();
        C3Vector up = C3Vector::Cross(right, forward);
        up.Normalize();
        const C3Vector pan = (right * s_creatorPanX + up * s_creatorPanY) * distance;
        position += pan;
        target += pan;
    }
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)
        || !std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z)) return;
    if (!s_creatorCamera) s_creatorCamera = CameraCreate();
    if (!s_creatorCamera) return;
    // A separate camera keeps animated background cameras and other glue screens untouched.
    for (uint32_t field : {2u, 4u, 5u}) CameraSetFloat(s_creatorCamera, field, CameraGetFloat(base, field));
    CameraSetFloat(s_creatorCamera, 3, std::max(0.01f, CameraGetFloat(base, 3) * 0.18f));
    CameraSetCoord(s_creatorCamera, 7, position);
    CameraSetCoord(s_creatorCamera, 8, target);
    if (frame->m_camera != s_creatorCamera) frame->SetCamera(s_creatorCamera);
}

// VERIFY: cbaddeb2 manual
// OFFSET: 0x004E1C20
void CCharacterCreation::Initialize() {
    CCharacterCreation::m_charFacing = 0.0f;
    CCharacterCreation::m_charCustomizeFrame = nullptr;
    CCharacterCreation::m_existingCharacterIndex = -1;
    memset(CCharacterCreation::m_charPreferences, 0, sizeof(CCharacterCreation::m_charPreferences));
    CCharacterCreation::m_races.m_count = 0;

    for (int32_t faction = 0; faction < 2; faction++) {
        for (int32_t i = 0; i < g_chrRacesDB.m_numRecords; i++) {
            ChrRacesRec* race = i >= 0 && i < g_chrRacesDB.m_numRecords ? &g_chrRacesDB.m_records[i] : nullptr;

            if (race && (race->m_flags & 0x1)) {
                continue;
            }

            FactionTemplateRec* factionTemplate = g_factionTemplateDB.GetRecord(race->m_factionID);

            if (!factionTemplate) {
                continue;
            }

            for (int32_t j = 0; j < g_factionGroupDB.m_numRecords; j++) {
                FactionGroupRec* group = j >= 0 && j < g_factionGroupDB.m_numRecords ? &g_factionGroupDB.m_records[j] : nullptr;

                if (!group || !group->m_maskID || !((1 << group->m_maskID) & factionTemplate->m_factionGroup)) {
                    continue;
                }

                if (!SStrCmpI(group->m_internalName, faction == 0 ? "Alliance" : "Horde", STORM_MAX_STR)) {
                    CCharacterCreation::m_races.New(race->m_id);
                }
            }
        }
    }
}

// VERIFY: 3b1595cf manual
// OFFSET: 0x004E1E20
void CCharacterCreation::Shutdown() {
    // Custom: creator zoom camera, retail Two Forms preview and selections
    ResetZoomCamera();
    CCharacterCreation::ClearFormMirror();
    RetailRaceFormClearMemory();

    if (CCharacterCreation::m_character) {
        CCharacterComponent::FreeComponent(CCharacterCreation::m_character);
        CCharacterCreation::m_character = nullptr;
    }

    // Custom: altered form preview
    if (CCharacterCreation::m_alteredCharacter) {
        CCharacterComponent::FreeComponent(CCharacterCreation::m_alteredCharacter);
        CCharacterCreation::m_alteredCharacter = nullptr;
    }

    CCharacterCreation::m_alteredFormAtFront = 0;

    CCharacterCreation::m_nameGen.m_names.Clear();
    CCharacterCreation::m_nameGen.m_dictionary.Clear();

    for (uint32_t i = 0; i < sizeof(CCharacterCreation::m_charPreferences) / sizeof(CCharacterCreation::m_charPreferences[0]); i++) {
        for (uint32_t j = 0; j < 2; j++) {
            delete CCharacterCreation::m_charPreferences[i][j];
            CCharacterCreation::m_charPreferences[i][j] = nullptr;
        }
    }
}

// VERIFY: e90576fb auto
// OFFSET: 0x004DFA50
int32_t CCharacterCreation::Idle(const void* data, void* param) {
    CGlueLoading::Idle(CCharacterCreation::m_character);

    // Custom: keep the altered form preview composited
    if (CCharacterCreation::m_alteredCharacter) {
        CCharacterCreation::m_alteredCharacter->RenderPrep(0);
    }

    return 1;
}

// VERIFY: 95595a1c manual
// OFFSET: 0x004DFF10
void CCharacterCreation::GetRandomRaceAndSex(ComponentData* data) {
    data->m_preferences.m_sexID = static_cast<uint32_t>((static_cast<uint64_t>(2) * CRandom::uint32_(g_rndSeed)) >> 32);

    ChrRacesRec* raceRec;

    do {
        uint32_t count = CCharacterCreation::m_races.Count();
        uint32_t index = count > 0 ? static_cast<uint32_t>((static_cast<uint64_t>(count) * CRandom::uint32_(g_rndSeed)) >> 32) : 0;
        data->m_preferences.m_raceID = CCharacterCreation::m_races.m_data[index];
        raceRec = g_chrRacesDB.GetRecord(data->m_preferences.m_raceID);
    } while (raceRec->m_requiredExpansion > ClientServices::Connection()->m_accountExpansion);
}

// VERIFY: 45c27290 auto
// OFFSET: 0x004DFFC0
void CCharacterCreation::OnHide() {
    // Custom: creator zoom camera, retail Two Forms preview
    ResetZoomCamera();
    CCharacterCreation::ClearFormMirror();
    EventUnregister(EVENT_ID_IDLE, &CCharacterCreation::Idle);
    LoadingScreenMiniDisable();
}

// VERIFY: f3353031 auto
// OFFSET: 0x004DFFE0
bool IsRaceClassValid(int32_t raceID, int32_t classID) {
    for (int32_t i = 0; i < g_charBaseInfoDB.m_numRecords; i++) {
        CharBaseInfoRec* rec = i >= 0 && i < g_charBaseInfoDB.m_numRecords ? &g_charBaseInfoDB.m_records[i] : nullptr;

        if (rec && static_cast<uint8_t>(rec->m_raceID) == raceID && static_cast<uint8_t>(rec->m_classID) == classID) {
            return true;
        }
    }

    return false;
}

// VERIFY: 0df0d554 auto
// OFFSET: 0x004E0040
void CCharacterCreation::SetCharCustomizeModel(const char* modelName) {
    if (!CCharacterCreation::m_charCustomizeFrame || !modelName || !*modelName) {
        return;
    }

    if (CCharacterCreation::m_charCustomizeFrame->m_model && !SStrCmpI(modelName, CCharacterCreation::m_charCustomizeFrame->m_model->m_shared->m_fileName, STORM_MAX_STR)) {
        return;
    }

    CCharacterCreation::m_charCustomizeFrame->SetModel(modelName);
    CCharacterCreation::m_charCustomizeFrame->m_lightArray1[0].unk_01B5 = 1;

    CM2Model* model = CCharacterCreation::m_charCustomizeFrame->m_model;

    if (model) {
        CCharacterCreation::m_charCustomizeFrame->m_lightArray1[0].unk_01BC = 1;
        model->m_lightingCallback = &CCharacterSelection::GenericLightingCallback;
        model->m_lightingArg = CCharacterCreation::m_charCustomizeFrame->m_lightArray1;
        model->IsDrawable(1, 1);
    }

    if (CCharacterCreation::m_charCustomizeFrame->m_model && CCharacterCreation::m_character->m_data.m_model) {
        // Custom: attaches the altered form preview as well
        CCharacterCreation::AttachCharacters();
    }
}

// VERIFY: 218e9a92 auto
// OFFSET: 0x004E0160
void CCharacterCreation::SetCharacterFacing(float facing) {
    CCharacterCreation::m_charFacing = facing;

    // Custom: with an altered form preview, the front model turns and the back one keeps facing forward
    C3Vector position(0.0f, 0.0f, 0.0f);
    bool alteredFront = CCharacterCreation::m_alteredCharacter && CCharacterCreation::m_alteredFormAtFront;

    if (CCharacterCreation::m_character->m_data.m_model) {
        // Custom: retail races (RetailRace.cpp): the Two Forms side-by-side offset and the preview scale
        C3Vector offset = position;
        RetailRaceFormOffset(CCharacterCreation::m_character, offset.x, offset.y);
        CCharacterCreation::m_character->m_data.m_model->SetWorldTransform(offset, alteredFront ? 0.0f : facing,
            RetailRacePreviewScale(CCharacterCreation::m_character->m_data.m_preferences.m_raceID));
    }

    // Custom: the retail Two Forms preview of the form the main character is not showing
    if (CCharacterCreation::m_formMirror && CCharacterCreation::m_formMirror->m_data.m_model) {
        C3Vector offset = position;
        RetailRaceFormOffset(CCharacterCreation::m_formMirror, offset.x, offset.y);
        CCharacterCreation::m_formMirror->m_data.m_model->SetWorldTransform(offset, facing,
            RetailRacePreviewScale(CCharacterCreation::m_formMirror->m_data.m_preferences.m_raceID));
    }

    if (CCharacterCreation::m_alteredCharacter && CCharacterCreation::m_alteredCharacter->m_data.m_model) {
        CCharacterCreation::m_alteredCharacter->m_data.m_model->SetWorldTransform(position, alteredFront ? facing : 0.0f, 1.0f);
    }
}

// Custom: attaches the character (and the altered form preview) to the background model: the front one at attachment
// 0, the back one at attachment 1, like Cataclysm's Worgen/Human character creation
void CCharacterCreation::AttachCharacters() {
    CM2Model* background = CCharacterCreation::m_charCustomizeFrame ? CCharacterCreation::m_charCustomizeFrame->m_model : nullptr;

    if (!background) {
        return;
    }

    CM2Model* character = CCharacterCreation::m_character ? CCharacterCreation::m_character->m_data.m_model : nullptr;
    CM2Model* altered = CCharacterCreation::m_alteredCharacter ? CCharacterCreation::m_alteredCharacter->m_data.m_model : nullptr;

    if (!altered) {
        CCharacterCreation::m_alteredFormAtFront = 0;
    }

    if (character) {
        if (character->m_attachParent) {
            character->DetachFromParent();
        }

        character->AttachToParent(background, CCharacterCreation::m_alteredFormAtFront != 0, nullptr, 0);
    }

    if (altered) {
        if (altered->m_attachParent) {
            altered->DetachFromParent();
        }

        altered->AttachToParent(background, CCharacterCreation::m_alteredFormAtFront == 0, nullptr, 0);
    }

    CCharacterCreation::SetCharacterFacing(CCharacterCreation::m_charFacing);
}

// Custom: builds (or drops) the altered form preview for the current character's race and sex
void CCharacterCreation::CreateAlteredCharacter() {
    if (CCharacterCreation::m_alteredCharacter) {
        if (CCharacterCreation::m_alteredCharacter->m_data.m_model->m_attachParent) {
            CCharacterCreation::m_alteredCharacter->m_data.m_model->DetachFromParent();
        }

        CCharacterComponent::FreeComponent(CCharacterCreation::m_alteredCharacter);
        CCharacterCreation::m_alteredCharacter = nullptr;
    }

    if (!CCharacterCreation::m_character || !CCharacterCreation::m_charCustomizeFrame) {
        return;
    }

    const CharacterPreferences& prefs = CCharacterCreation::m_character->m_data.m_preferences;
    uint32_t alteredRace = ClientGetAlteredFormRace(prefs.m_raceID);
    CreatureModelDataRec* modelRec = alteredRace ? GetModelDataRec(alteredRace, prefs.m_sexID) : nullptr;

    if (!modelRec || !*modelRec->m_modelName) {
        return;
    }

    ComponentData data;
    data.SetPreferences(&prefs);
    data.m_preferences.m_raceID = alteredRace;
    data.m_model = CCharacterCreation::m_charCustomizeFrame->GetScene()->CreateModel(modelRec->m_modelName, 0);

    if (!data.m_model) {
        return;
    }

    data.m_model->m_lightingCallback = &CCharacterSelection::GenericLightingCallback;
    data.m_model->m_lightingArg = CCharacterCreation::m_charCustomizeFrame->m_lightArray2;
    data.m_model->SetBoneSequence(0xFFFFFFFF, 0, 0, 0, 1.0f, 1, 1);
    data.m_flags |= 0x2;
    CCharacterComponent::ValidateComponentData(&data, COMPONENT_CONTEXT_0);

    CCharacterCreation::m_alteredCharacter = CCharacterComponent::AllocComponent();
    CCharacterCreation::m_alteredCharacter->Init(&data, nullptr);
    CCharacterCreation::DressComponent(CCharacterCreation::m_alteredCharacter);
    CCharacterCreation::m_alteredCharacter->RenderPrep(0);
}

// Custom: copies the current choices onto the altered form preview
void CCharacterCreation::SyncAlteredCharacter() {
    if (!CCharacterCreation::m_character || !CCharacterCreation::m_alteredCharacter) {
        return;
    }

    CCharacterCreation::m_character->CopyChoicesToComponent(CCharacterCreation::m_alteredCharacter, COMPONENT_CONTEXT_0);
    CCharacterCreation::m_alteredCharacter->RenderPrep(0);
}

// Custom: swaps which form stands at the front
void CCharacterCreation::SetViewingAlteredForm(int32_t alteredForm) {
    alteredForm = alteredForm && CCharacterCreation::m_alteredCharacter ? 1 : 0;

    if (alteredForm == CCharacterCreation::m_alteredFormAtFront) {
        return;
    }

    CCharacterCreation::m_alteredFormAtFront = alteredForm;
    CCharacterCreation::AttachCharacters();
}

// VERIFY: 308d1147 auto
// OFFSET: 0x004E01B0
bool CCharacterCreation::IsClassAvailable(int32_t classID) {
    for (uint32_t i = 0; i < CCharacterCreation::m_classes.Count(); i++) {
        if (CCharacterCreation::m_classes.m_data[i] && CCharacterCreation::m_classes.m_data[i]->m_id == classID) {
            return true;
        }
    }

    return false;
}

// VERIFY: e48266b8 auto
// OFFSET: 0x004E01F0
void CCharacterCreation::CycleCharCustomization(CHAR_CUSTOMIZATION_TYPE type, int32_t delta) {
    if (!delta) {
        return;
    }

    switch (type) {
        case CHAR_CUSTOMIZATION_SKIN: {
            if (delta > 0) {
                CCharacterCreation::m_character->SetNextSkin(COMPONENT_CONTEXT_0);
            } else {
                CCharacterCreation::m_character->SetPrevSkin(COMPONENT_CONTEXT_0);
            }

            CCharacterCreation::m_prevSkinIndex = CCharacterCreation::m_character->m_data.m_preferences.m_skinID;
            CCharacterCreation::SyncAlteredCharacter(); // Custom: the altered form preview follows the choices
            CGlueLoading::StartLoad(CCharacterCreation::m_character, 1);
            return;
        }

        case CHAR_CUSTOMIZATION_FACE: {
            if (delta > 0) {
                CCharacterCreation::m_character->SetNextFace(COMPONENT_CONTEXT_0, CCharacterCreation::m_prevSkinIndex);
            } else {
                CCharacterCreation::m_character->SetPrevFace(COMPONENT_CONTEXT_0, CCharacterCreation::m_prevSkinIndex);
            }

            break;
        }

        case CHAR_CUSTOMIZATION_HAIR_STYLE: {
            if (delta > 0) {
                CCharacterCreation::m_character->SetNextHairStyle(COMPONENT_CONTEXT_0);
            } else {
                CCharacterCreation::m_character->SetPrevHairStyle(COMPONENT_CONTEXT_0);
            }

            break;
        }

        case CHAR_CUSTOMIZATION_HAIR_COLOR: {
            if (delta > 0) {
                CCharacterCreation::m_character->SetNextHairColor(COMPONENT_CONTEXT_0);
            } else {
                CCharacterCreation::m_character->SetPrevHairColor(COMPONENT_CONTEXT_0);
            }

            break;
        }

        case CHAR_CUSTOMIZATION_FACIAL_FEATURE: {
            if (delta > 0) {
                CCharacterCreation::m_character->SetNextFacialFeature(COMPONENT_CONTEXT_0);
            } else {
                CCharacterCreation::m_character->SetPrevFacialFeature(COMPONENT_CONTEXT_0);
            }

            break;
        }

        default: {
            break;
        }
    }

    CCharacterCreation::SyncAlteredCharacter(); // Custom: the altered form preview follows the choices
    CGlueLoading::StartLoad(CCharacterCreation::m_character, 1);
}

// VERIFY: 0b8a8ec0 manual
// OFFSET: 0x004E0380
void CCharacterCreation::CreateCharacter(const char* name) {
    WOWGUID guid = WOWGUID();
    CharacterSelectionDisplay* display = nullptr;
    bool validate = true;

    if (CCharacterCreation::m_existingCharacterIndex >= 0) {
        display = CCharacterSelection::GetCharacterDisplay(CCharacterCreation::m_existingCharacterIndex);

        if (!display) {
            return;
        }

        guid = display->m_characterInfo.m_guid;

        if (name) {
            // Custom: case-sensitive, so a case-only change counts as a new name
            if (!SStrCmp(display->m_characterInfo.m_name, name, STORM_MAX_STR)) {
                validate = false;
            }
        } else {
            name = display->m_characterInfo.m_name;
        }
    }

    if (validate) {
        int32_t result = CHAR_NAME_NO_NAME;

        if (!name || (result = ClientServices::CharacterValidateName(name)) != CHAR_NAME_SUCCESS) {
            FrameScript_SignalEvent(GLUE_EVENT_OPEN_STATUS_DIALOG, "%s%s", "OKAY", FrameScript_GetText(ClientServices::GetErrorToken(result), -1, GENDER_NOT_APPLICABLE));
            return;
        }
    }

    CHARACTER_CREATE_INFO info;
    SStrCopy(info.m_name, name, sizeof(info.m_name));
    info.m_raceID = static_cast<uint8_t>(CCharacterCreation::m_character->m_data.m_preferences.m_raceID);
    info.m_classID = static_cast<uint8_t>(CCharacterCreation::m_selectedClassID);
    info.m_sexID = static_cast<uint8_t>(CCharacterCreation::m_character->m_data.m_preferences.m_sexID);
    info.m_skinID = static_cast<uint8_t>(CCharacterCreation::m_character->m_data.m_preferences.m_skinID);
    info.m_hairColorID = static_cast<uint8_t>(CCharacterCreation::m_character->m_data.m_preferences.m_hairColorId);
    info.m_hairStyleID = static_cast<uint8_t>(CCharacterCreation::m_character->m_data.m_preferences.m_hairStyleId);
    info.m_facialHairStyleID = static_cast<uint8_t>(CCharacterCreation::m_character->m_data.m_preferences.m_facialHairId);
    info.m_faceID = static_cast<uint8_t>(CCharacterCreation::m_character->m_data.m_preferences.m_faceId);
    info.m_outfitID = 0;

    if (guid && display) {
        uint32_t flags = display->m_characterInfo.m_customizeFlags;

        if (flags & 0x100000) {
            CGlueMgr::RaceChange(guid, &info);
        } else if (flags & 0x10000) {
            CGlueMgr::FactionChange(guid, &info);
        } else {
            CGlueMgr::CustomizeCharacter(guid, &info);
        }
    } else {
        CGlueMgr::CreateCharacter(&info);
    }
}

// VERIFY: a51378a3 auto
// OFFSET: 0x004E0500
int32_t CCharacterCreation::GetExistingRaceIndex() {
    CharacterSelectionDisplay* display = CCharacterSelection::GetCharacterDisplay(CCharacterCreation::m_existingCharacterIndex);

    if (!display) {
        return -1;
    }

    for (uint32_t i = 0; i < CCharacterCreation::m_races.Count(); i++) {
        if (CCharacterCreation::m_races.m_data[i] == display->m_characterInfo.m_raceID) {
            return i;
        }
    }

    return -1;
}

// VERIFY: 31f4168d auto
// OFFSET: 0x004E0550
int32_t CCharacterCreation::GetExistingClassIndex() {
    CharacterSelectionDisplay* display = CCharacterSelection::GetCharacterDisplay(CCharacterCreation::m_existingCharacterIndex);

    if (!display) {
        return -1;
    }

    for (int32_t i = 0; i < g_chrClassesDB.m_numRecords; i++) {
        ChrClassesRec* rec = i >= 0 && i < g_chrClassesDB.m_numRecords ? &g_chrClassesDB.m_records[i] : nullptr;

        if (rec->m_id == display->m_characterInfo.m_classID) {
            return i;
        }
    }

    return -1;
}

// VERIFY: 6c4329fd manual
// OFFSET: 0x004E0F50
int32_t CCharacterCreation::GetRandomClassID() {
    int32_t classes[10];
    int32_t count = 0;
    uint8_t expansion = ClientServices::Connection()->m_accountExpansion;

    for (uint32_t i = 0; i < CCharacterCreation::m_classes.Count(); i++) {
        ChrClassesRec* rec = CCharacterCreation::m_classes.m_data[i];

        if (expansion >= rec->m_requiredExpansion) {
            classes[count++] = rec->m_id;
        }
    }

    if (count <= 0) {
        return classes[0];
    }

    return classes[static_cast<uint32_t>((static_cast<uint64_t>(count) * CRandom::uint32_(g_rndSeed)) >> 32)];
}

// Custom: dresses the character and the altered form preview the same way (the body below is the original Dress,
// working on `component`; the starting outfit is still chosen by the character's own race)
void CCharacterCreation::Dress() {
    CCharacterCreation::Dress(CCharacterCreation::m_character); // Custom: through Dress(target) (retail races shown bare)

    if (CCharacterCreation::m_alteredCharacter) {
        CCharacterCreation::DressComponent(CCharacterCreation::m_alteredCharacter);
    }
}

// VERIFY: b15e5451 auto
// OFFSET: 0x004E0FD0
// Custom: retail races (RetailRace.cpp): takes every item off a preview (Dracthyr Dragon is shown bare)
void CCharacterCreation::Undress(CCharacterComponent* target) {
    if (!target || !target->m_data.m_model) {
        return;
    }

    for (int32_t slot = 0; slot < NUM_ITEM_SLOTS; slot++) {
        target->RemoveItem(static_cast<ITEM_SLOT>(slot));
    }

    CCharacterComponent::RemoveHandItem(target->m_data.m_model, INVSLOT_MAINHAND, SHEATHE_NONE, false);
    CCharacterComponent::RemoveHandItem(target->m_data.m_model, INVSLOT_OFFHAND, SHEATHE_NONE, false);
    CCharacterComponent::RemoveHandItem(target->m_data.m_model, INVSLOT_OFFHAND, SHEATHE_NONE, true);
    target->m_handItemDisplayID[0] = target->m_handItemDisplayID[1] = 0;
    target->m_flags |= 0x4;
}

// Custom: dresses one preview in the starting outfit, or undresses it where the retail race shows it bare
void CCharacterCreation::Dress(CCharacterComponent* target) {
    if (RetailRaceNaked(target)) {
        CCharacterCreation::Undress(target);
        return;
    }

    if (!target || !target->m_data.m_model) {
        return;
    }

    CCharacterCreation::DressComponent(target);
}

void CCharacterCreation::DressComponent(CCharacterComponent* component) {
    if (CCharacterCreation::m_existingCharacterIndex >= 0) {
        CharacterSelectionDisplay* display = CCharacterSelection::GetCharacterDisplay(CCharacterCreation::m_existingCharacterIndex);

        if (display) {
            for (uint32_t slot = 0; slot < 23; slot++) {
                int32_t displayID = display->m_characterInfo.m_inventoryItemDisplayID[slot];
                int32_t itemVisual = display->m_characterInfo.m_inventoryItemVisualID[slot];

                if (!displayID) {
                    continue;
                }

                if (display->m_characterInfo.m_classID == 3) {
                    if (slot == INVSLOT_MAINHAND || slot == INVSLOT_OFFHAND) {
                        continue;
                    }
                } else if (slot == INVSLOT_RANGED) {
                    continue;
                }

                if (slot == INVSLOT_HEAD || ((display->m_characterInfo.m_flags & 0x800) && slot == INVSLOT_BACK)) {
                    continue;
                }

                ItemDisplayInfoRec itemRec;
                int32_t found = g_itemDisplayInfoDB.GetRecord(displayID, &itemRec);
                int32_t visual = found && g_itemVisualsDB.GetRecord(itemRec.m_itemVisual) ? 0 : itemVisual;

                if (slot != INVSLOT_MAINHAND && slot != INVSLOT_OFFHAND && slot != INVSLOT_RANGED) {
                    component->AddItemBySlot(static_cast<INVENTORY_SLOTS>(slot), displayID, visual);
                } else if (found) {
                    CCharacterComponent::AddHandItem(component->m_data.m_model, &itemRec, static_cast<INVENTORY_SLOTS>(slot), SHEATHE_NONE, false, display->m_characterInfo.m_inventoryItemType[slot] == INVTYPE_SHIELD, false, visual);
                }

                if (slot == INVSLOT_TABARD && found && (itemRec.m_flags & 0x1)) {
                    int32_t emblemStyle;
                    int32_t emblemColor;
                    int32_t borderStyle;
                    int32_t borderColor;
                    int32_t background;

                    if (GuildGetGuildTabard(display->m_characterInfo.m_guid, display->m_characterInfo.m_guildID, nullptr, nullptr, emblemStyle, emblemColor, borderStyle, borderColor, background)) {
                        component->ApplyGuildColor(emblemStyle, emblemColor, borderStyle, borderColor, background);
                    }
                }
            }

            return;
        }
    }

    for (uint32_t i = 0; i < NUM_ITEM_SLOTS; i++) {
        component->RemoveItem(static_cast<ITEM_SLOT>(i));
    }

    CCharacterComponent::RemoveHandItem(component->m_data.m_model, INVSLOT_MAINHAND, SHEATHE_NONE, false);
    CCharacterComponent::RemoveHandItem(component->m_data.m_model, INVSLOT_OFFHAND, SHEATHE_NONE, false);
    CCharacterComponent::RemoveHandItem(component->m_data.m_model, INVSLOT_OFFHAND, SHEATHE_NONE, true);
    CCharacterComponent::RemoveHandItem(component->m_data.m_model, INVSLOT_RANGED, SHEATHE_NONE, false);

    for (int32_t i = 0; i < g_charStartOutfitDB.m_numRecords; i++) {
        CharStartOutfitRec* outfit = i >= 0 && i < g_charStartOutfitDB.m_numRecords ? &g_charStartOutfitDB.m_records[i] : nullptr;

        if (static_cast<uint8_t>(outfit->m_raceID) != CCharacterCreation::m_character->m_data.m_preferences.m_raceID || static_cast<uint8_t>(outfit->m_classID) != CCharacterCreation::m_selectedClassID || static_cast<uint8_t>(outfit->m_sexID) != CCharacterCreation::m_character->m_data.m_preferences.m_sexID) {
            continue;
        }

        for (int32_t j = 0; j < 24; j++) {
            int32_t displayID = outfit->m_displayItemID[j];
            int32_t type = outfit->m_inventoryType[j];

            if (displayID <= 0 || type == INVTYPE_HEAD) {
                continue;
            }

            if (CCharacterCreation::m_selectedClassID == 3) {
                switch (type) {
                    case INVTYPE_WEAPON:
                    case INVTYPE_2HWEAPON:
                    case INVTYPE_WEAPONMAINHAND:
                    case INVTYPE_WEAPONOFFHAND: {
                        continue;
                    }

                    case INVTYPE_RANGED:
                    case INVTYPE_RANGEDRIGHT: {
                        bool mainHand = type == INVTYPE_RANGEDRIGHT || type == INVTYPE_THROWN;
                        ItemDisplayInfoRec itemRec;

                        if (g_itemDisplayInfoDB.GetRecord(displayID, &itemRec)) {
                            GEOCOMPONENTLINKS link = CCharacterComponent::AddHandItem(component->m_data.m_model, &itemRec, INVSLOT_RANGED, SHEATHE_NONE, false, false, mainHand, 0);

                            if (type == INVTYPE_RANGED) {
                                for (CM2Model* attached = component->m_data.m_model->m_attachList; attached; attached = attached->m_attachNext) {
                                    if (attached->m_attachmentId == static_cast<uint32_t>(link)) {
                                        attached->SetLoadedCallback(&CCharacterSelection::ModelLoadedCallback, nullptr);
                                        break;
                                    }
                                }
                            }
                        }

                        break;
                    }

                    case INVTYPE_BAG: {
                        ItemDisplayInfoRec itemRec;

                        if (g_itemDisplayInfoDB.GetRecord(displayID, &itemRec) && *itemRec.m_modelName[0]) {
                            component->AddItem(ITEMSLOT_11, &itemRec, 0);
                        }

                        break;
                    }

                    default: {
                        break;
                    }
                }
            }

            component->AddItemByType(static_cast<INVENTORY_TYPES>(type), displayID);
        }

        return;
    }
}

// VERIFY: 6d456576 auto
// OFFSET: 0x004E13A0
void CCharacterCreation::InitCharacterComponents(ComponentData* data, bool randomize) {
    // Custom: retail races keep the creator's eye lightning choice across a rebuild of the same race
    const bool eyeLightning = m_character
        && m_character->m_data.m_preferences.m_raceID == data->m_preferences.m_raceID
        && RetailRaceEyeLightning(m_character);
    CreatureModelDataRec* modelRec = GetModelDataRec(data->m_preferences.m_raceID, data->m_preferences.m_sexID);

    if (!modelRec || !*modelRec->m_modelName) {
        return;
    }

    if (CCharacterCreation::m_character) {
        if (CCharacterCreation::m_character->m_data.m_model->m_attachParent) {
            CCharacterCreation::m_character->m_data.m_model->DetachFromParent();
        }

        RetailRaceFormStore(CCharacterCreation::m_character); // Custom: retail Two Forms selections survive the rebuild
        CCharacterComponent::FreeComponent(CCharacterCreation::m_character);
    }

    CCharacterCreation::m_character = CCharacterComponent::AllocComponent();
    // Custom: retail races are built from their retail model (for the form the creator last showed)
    const uint32_t form = RetailRaceFormRestore(CCharacterCreation::m_character, data->m_preferences);
    char retailPath[260];
    const bool retailModel = RetailRaceModelPath(data->m_preferences.m_raceID, data->m_preferences.m_sexID, retailPath, sizeof(retailPath), form);
    data->m_model = CCharacterCreation::m_charCustomizeFrame->GetScene()->CreateModel(retailModel ? retailPath : modelRec->m_modelName, 0);

    if (!data->m_model) {
        return;
    }

    CCharacterCreation::m_charCustomizeFrame->m_lightArray2[0].unk_01BC = 1;
    data->m_model->m_lightingCallback = &CCharacterSelection::GenericLightingCallback;
    data->m_model->m_lightingArg = CCharacterCreation::m_charCustomizeFrame->m_lightArray2;
    data->m_model->SetBoneSequence(0xFFFFFFFF, 0, 0, 0, 1.0f, 1, 1);
    data->m_flags |= 0x2;

    CCharacterCreation::m_character->Init(data, nullptr);
    RetailRaceEyeLightning(m_character, eyeLightning); // Custom

    if (randomize) {
        CCharacterCreation::m_character->SetRandomSkin(COMPONENT_CONTEXT_0);
        CCharacterCreation::m_character->SetRandomHairColor(COMPONENT_CONTEXT_0);
        CCharacterCreation::m_character->SetRandomHairStyle(COMPONENT_CONTEXT_0);
        CCharacterCreation::m_character->SetRandomFace(COMPONENT_CONTEXT_0);
        CCharacterCreation::m_character->SetRandomFacialFeature(COMPONENT_CONTEXT_0);
    }

    CCharacterCreation::m_prevSkinIndex = CCharacterCreation::m_character->m_data.m_preferences.m_skinID;
    CCharacterCreation::m_prevFaceIndex = CCharacterCreation::m_character->m_data.m_preferences.m_faceId;
    CCharacterCreation::m_prevHairColorIndex = CCharacterCreation::m_character->m_data.m_preferences.m_hairColorId;
    CCharacterCreation::m_prevHairStyleIndex = CCharacterCreation::m_character->m_data.m_preferences.m_hairStyleId;
    CCharacterCreation::m_prevFacialHairIndex = CCharacterCreation::m_character->m_data.m_preferences.m_facialHairId;

    // Custom: (re)build the altered form preview for this race and sex before dressing, so both get dressed
    CCharacterCreation::CreateAlteredCharacter();

    CCharacterCreation::SetCharacterFacing(CCharacterCreation::m_charFacing);
    CCharacterCreation::Dress();
    CCharacterCreation::m_character->RenderPrep(0);

    if (CCharacterCreation::m_charCustomizeFrame->m_model) {
        // Custom: attaches the altered form preview as well
        CCharacterCreation::AttachCharacters();
    }

    CCharacterCreation::SyncFormMirror(); // Custom: retail Two Forms side-by-side preview
}

// Custom: drops the retail Two Forms preview
void CCharacterCreation::ClearFormMirror() {
    CCharacterComponent* mirror = CCharacterCreation::m_formMirror;
    if (!mirror) return;
    CCharacterCreation::m_formMirror = nullptr;
    if (mirror->m_data.m_model && mirror->m_data.m_model->m_attachParent) mirror->m_data.m_model->DetachFromParent();
    CCharacterComponent::FreeComponent(mirror);
}

// Custom: rebuilds the second preview: the form the main character is not showing, with that form's own selections.
void CCharacterCreation::SyncFormMirror() {
    CCharacterCreation::ClearFormMirror();
    CCharacterComponent* main = CCharacterCreation::m_character;
    if (!main || !main->m_data.m_model || !CCharacterCreation::m_charCustomizeFrame || !RetailRaceFormDual(main)) return;

    ComponentData data;
    data.m_preferences.m_raceID = main->m_data.m_preferences.m_raceID;
    data.m_preferences.m_sexID = main->m_data.m_preferences.m_sexID;
    data.m_preferences.m_classId = main->m_data.m_preferences.m_classId;
    CCharacterComponent* mirror = CCharacterComponent::AllocComponent();
    if (!mirror) return;
    const uint32_t form = RetailRaceMirrorSetup(mirror, main, data.m_preferences);
    char path[260];
    if (!form || !RetailRaceModelPath(data.m_preferences.m_raceID, data.m_preferences.m_sexID, path, sizeof(path), form)) {
        CCharacterComponent::FreeComponent(mirror);
        return;
    }
    data.m_model = CCharacterCreation::m_charCustomizeFrame->GetScene()->CreateModel(path, 0);
    if (!data.m_model) {
        CCharacterComponent::FreeComponent(mirror);
        return;
    }

    CCharacterCreation::m_charCustomizeFrame->m_lightArray2[0].unk_01BC = 1;
    data.m_model->m_lightingCallback = &CCharacterSelection::GenericLightingCallback;
    data.m_model->m_lightingArg = CCharacterCreation::m_charCustomizeFrame->m_lightArray2;
    data.m_model->SetBoneSequence(0xFFFFFFFF, 0, 0, 0, 1.0f, 1, 1);
    data.m_flags |= 0x2;

    mirror->Init(&data, nullptr);
    CCharacterCreation::m_formMirror = mirror;
    CCharacterCreation::SetCharacterFacing(CCharacterCreation::m_charFacing);
    CCharacterCreation::Dress(mirror);
    mirror->RenderPrep(0);
    if (CCharacterCreation::m_charCustomizeFrame->m_model) {
        mirror->m_data.m_model->AttachToParent(CCharacterCreation::m_charCustomizeFrame->m_model, 0, nullptr, 0);
    }
    CGlueLoading::StartLoad(mirror, 1);
}

// VERIFY: 68a365e1 manual
// OFFSET: 0x004E1540
void CCharacterCreation::SetSelectedSex(uint32_t sexID) {
    if (sexID >= 3 || sexID == CCharacterCreation::m_character->m_data.m_preferences.m_sexID) {
        return;
    }

    CharacterPreferences* preferences = CCharacterCreation::m_charPreferences[CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex]][CCharacterCreation::m_character->m_data.m_preferences.m_sexID];

    if (!preferences) {
        preferences = static_cast<CharacterPreferences*>(SMemAlloc(sizeof(CharacterPreferences), __FILE__, __LINE__, 0));
        CCharacterCreation::m_charPreferences[CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex]][CCharacterCreation::m_character->m_data.m_preferences.m_sexID] = preferences;
    }

    CCharacterCreation::m_character->GetPreferences(preferences);

    ComponentData data;
    int32_t classID = CCharacterCreation::m_selectedClassID;
    data.m_preferences.m_raceID = CCharacterCreation::m_character->m_data.m_preferences.m_raceID;
    data.m_preferences.m_sexID = sexID;
    data.m_preferences.m_classId = classID;

    if (CCharacterCreation::m_existingCharacterIndex >= 0) {
        CharacterSelectionDisplay* display = CCharacterSelection::GetCharacterDisplay(CCharacterCreation::m_existingCharacterIndex);

        if (display && display->m_characterInfo.m_sexID == sexID && (display->m_characterInfo.m_customizeFlags & 0x1)) {
            data.m_preferences.m_raceID = display->m_characterInfo.m_raceID;
            data.m_preferences.m_sexID = display->m_characterInfo.m_sexID;
            data.m_preferences.m_classId = display->m_characterInfo.m_classID;
            data.m_preferences.m_skinID = display->m_characterInfo.m_skinColorID;
            data.m_preferences.m_hairStyleId = display->m_characterInfo.m_hairStyleID;
            data.m_preferences.m_hairColorId = display->m_characterInfo.m_hairColorID;
            data.m_preferences.m_facialHairId = display->m_characterInfo.m_facialHairStyleID;
            data.m_preferences.m_faceId = display->m_characterInfo.m_faceID;
            CCharacterCreation::InitCharacterComponents(&data, false);

            CCharacterCreation::m_nameGen.LoadNames(CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex], sexID);
            CCharacterCreation::SyncAlteredCharacter(); // Custom: the altered form preview follows the choices
            CGlueLoading::StartLoad(CCharacterCreation::m_character, 1);
            return;
        }

        classID = CCharacterCreation::m_selectedClassID;
    }

    CharacterPreferences* saved = CCharacterCreation::m_charPreferences[CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex]][sexID];

    if (saved) {
        data.SetPreferences(saved);
        data.m_preferences.m_classId = classID;
        CCharacterComponent::ValidateComponentData(&data, COMPONENT_CONTEXT_0);
        CCharacterCreation::InitCharacterComponents(&data, false);
    } else {
        data.m_preferences.m_raceID = CCharacterCreation::m_character->m_data.m_preferences.m_raceID;
        data.m_preferences.m_sexID = sexID;
        data.m_preferences.m_classId = classID;
        CCharacterCreation::InitCharacterComponents(&data, true);
    }

    CCharacterCreation::m_nameGen.LoadNames(CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex], sexID);
    CCharacterCreation::SyncAlteredCharacter(); // Custom: the altered form preview follows the choices
    CGlueLoading::StartLoad(CCharacterCreation::m_character, 1);
}

// VERIFY: 60798dcb auto
// OFFSET: 0x004E1740
void CCharacterCreation::SetSelectedClass(int32_t classID) {
    if (!CCharacterCreation::IsClassAvailable(classID)) {
        return;
    }

    CCharacterCreation::m_selectedClassID = classID;

    // Custom: a race whose (retail) model failed to load leaves no character to rebuild
    if (!CCharacterCreation::m_character || !CCharacterCreation::m_character->m_data.m_model) {
        return;
    }

    ComponentData data;
    data.m_preferences.m_raceID = CCharacterCreation::m_character->m_data.m_preferences.m_raceID;
    data.m_preferences.m_sexID = CCharacterCreation::m_character->m_data.m_preferences.m_sexID;
    data.m_preferences.m_classId = classID;
    data.m_preferences.m_skinID = CCharacterCreation::m_character->m_data.m_preferences.m_skinID;
    data.m_preferences.m_hairStyleId = CCharacterCreation::m_character->m_data.m_preferences.m_hairStyleId;
    data.m_preferences.m_hairColorId = CCharacterCreation::m_character->m_data.m_preferences.m_hairColorId;
    data.m_preferences.m_facialHairId = CCharacterCreation::m_character->m_data.m_preferences.m_facialHairId;
    data.m_preferences.m_faceId = CCharacterCreation::m_character->m_data.m_preferences.m_faceId;
    CCharacterComponent::ValidateComponentData(&data, COMPONENT_CONTEXT_0);
    CCharacterCreation::InitCharacterComponents(&data, false);
    CCharacterCreation::Dress();
    CCharacterCreation::SyncAlteredCharacter(); // Custom: the altered form preview follows the choices
    CGlueLoading::StartLoad(CCharacterCreation::m_character, 1);
}

// VERIFY: ca66e480 auto
// OFFSET: 0x004E17F0
void CCharacterCreation::RandomizeCharCustomization() {
    CCharacterCreation::m_character->SetRandomSkin(COMPONENT_CONTEXT_0);
    CCharacterCreation::m_character->SetRandomFace(COMPONENT_CONTEXT_0);
    CCharacterCreation::m_character->SetRandomHairColor(COMPONENT_CONTEXT_0);
    CCharacterCreation::m_character->SetRandomHairStyle(COMPONENT_CONTEXT_0);
    CCharacterCreation::m_character->SetRandomFacialFeature(COMPONENT_CONTEXT_0);

    CCharacterCreation::m_prevSkinIndex = CCharacterCreation::m_character->m_data.m_preferences.m_skinID;
    CCharacterCreation::m_prevFaceIndex = CCharacterCreation::m_character->m_data.m_preferences.m_faceId;
    CCharacterCreation::m_prevHairColorIndex = CCharacterCreation::m_character->m_data.m_preferences.m_hairColorId;
    CCharacterCreation::m_prevHairStyleIndex = CCharacterCreation::m_character->m_data.m_preferences.m_hairStyleId;
    CCharacterCreation::m_prevFacialHairIndex = CCharacterCreation::m_character->m_data.m_preferences.m_facialHairId;

    CCharacterCreation::Dress();
    CCharacterCreation::SyncAlteredCharacter(); // Custom: the altered form preview follows the choices
    CGlueLoading::StartLoad(CCharacterCreation::m_character, 1);
}

// VERIFY: f5f53eff manual
// OFFSET: 0x004E1ED0
void CCharacterCreation::CalcClasses(int32_t raceID) {
    uint32_t count = 0;

    for (int32_t i = 0; static_cast<uint32_t>(i) < static_cast<uint32_t>(g_charBaseInfoDB.m_numRecords); i++) {
        CharBaseInfoRec* rec = i >= 0 && i < g_charBaseInfoDB.m_numRecords ? &g_charBaseInfoDB.m_records[i] : nullptr;

        if (rec && static_cast<uint8_t>(rec->m_raceID) == raceID) {
            count++;
        }
    }

    CCharacterCreation::m_classes.SetCount(count);

    uint32_t index = 0;

    for (int32_t i = 0; i < g_charBaseInfoDB.m_numRecords; i++) {
        CharBaseInfoRec* rec = i >= 0 && i < g_charBaseInfoDB.m_numRecords ? &g_charBaseInfoDB.m_records[i] : nullptr;

        if (rec && static_cast<uint8_t>(rec->m_raceID) == raceID) {
            CCharacterCreation::m_classes.m_data[index++] = g_chrClassesDB.GetRecord(static_cast<uint8_t>(rec->m_classID));
        }
    }
}

// VERIFY: 32675449 auto
// OFFSET: 0x004E1FD0
void CCharacterCreation::ResetCharCustomizeInfo() {
    if (!CCharacterCreation::m_charCustomizeFrame) {
        return;
    }

    CCharacterCreation::m_existingCharacterIndex = -1;
    RetailRaceEyeLightning(m_character, false); // Custom

    if (CCharacterCreation::m_charCustomizeFrame->m_model) {
        CCharacterCreation::m_charCustomizeFrame->m_model->DetachAllChildrenById(0);
    }

    // Custom: some listed races may have no loadable model (missing records, or retail data that could not be read).
    // Retry with another random race until a character exists, so the rest of the glue never sees a null component.
    ComponentData data;
    for (int32_t attempt = 0; attempt < 64; attempt++) {
        data = ComponentData();
        CCharacterCreation::GetRandomRaceAndSex(&data);
        CCharacterCreation::CalcClasses(data.m_preferences.m_raceID);
        CCharacterCreation::InitCharacterComponents(&data, true);

        if (CCharacterCreation::m_character && CCharacterCreation::m_character->m_data.m_model) {
            break;
        }
    }

    if (!CCharacterCreation::m_character) {
        return;
    }

    CCharacterCreation::SetSelectedClass(CCharacterCreation::GetRandomClassID());
    data.m_preferences.m_classId = CCharacterCreation::m_selectedClassID;

    CCharacterCreation::m_raceIndex = -1;

    for (uint32_t i = 0; i < CCharacterCreation::m_races.Count(); i++) {
        if (CCharacterCreation::m_races.m_data[i] == static_cast<int32_t>(CCharacterCreation::m_character->m_data.m_preferences.m_raceID)) {
            CCharacterCreation::m_raceIndex = i;
            break;
        }
    }

    CCharacterCreation::m_nameGen.LoadNames(CCharacterCreation::m_character->m_data.m_preferences.m_raceID, CCharacterCreation::m_character->m_data.m_preferences.m_sexID);
    CCharacterCreation::SyncAlteredCharacter(); // Custom: the altered form preview follows the choices
    CGlueLoading::StartLoad(CCharacterCreation::m_character, 1);
}

// VERIFY: 06f4239a manual
// OFFSET: 0x004E20B0
void CCharacterCreation::SetSelectedRace(uint32_t raceIndex) {
    if (raceIndex >= CCharacterCreation::m_races.Count() || raceIndex == CCharacterCreation::m_raceIndex) {
        return;
    }

    CharacterPreferences* preferences = CCharacterCreation::m_charPreferences[CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex]][CCharacterCreation::m_character->m_data.m_preferences.m_sexID];

    if (!preferences) {
        preferences = static_cast<CharacterPreferences*>(SMemAlloc(sizeof(CharacterPreferences), __FILE__, __LINE__, 0));
        CCharacterCreation::m_charPreferences[CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex]][CCharacterCreation::m_character->m_data.m_preferences.m_sexID] = preferences;
    }

    CCharacterCreation::m_character->GetPreferences(preferences);
    CCharacterCreation::m_raceIndex = raceIndex;

    ComponentData data;
    CharacterSelectionDisplay* display = nullptr;

    if (CCharacterCreation::m_existingCharacterIndex >= 0) {
        display = CCharacterSelection::GetCharacterDisplay(CCharacterCreation::m_existingCharacterIndex);
    }

    if (display && display->m_characterInfo.m_raceID == CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex] && (display->m_characterInfo.m_customizeFlags & 0x110000)) {
        data.m_preferences.m_raceID = display->m_characterInfo.m_raceID;
        data.m_preferences.m_sexID = display->m_characterInfo.m_sexID;
        data.m_preferences.m_classId = display->m_characterInfo.m_classID;
        data.m_preferences.m_skinID = display->m_characterInfo.m_skinColorID;
        data.m_preferences.m_hairStyleId = display->m_characterInfo.m_hairStyleID;
        data.m_preferences.m_hairColorId = display->m_characterInfo.m_hairColorID;
        data.m_preferences.m_facialHairId = display->m_characterInfo.m_facialHairStyleID;
        data.m_preferences.m_faceId = display->m_characterInfo.m_faceID;
        CCharacterCreation::InitCharacterComponents(&data, false);
        CCharacterCreation::SetSelectedSex(display->m_characterInfo.m_sexID);
    } else {
        CharacterPreferences* saved = CCharacterCreation::m_charPreferences[CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex]][CCharacterCreation::m_character->m_data.m_preferences.m_sexID];

        if (saved) {
            data.SetPreferences(saved);
            CCharacterCreation::CalcClasses(data.m_preferences.m_raceID);

            int32_t classID = CCharacterCreation::m_selectedClassID;

            if (!IsRaceClassValid(data.m_preferences.m_raceID, CCharacterCreation::m_selectedClassID)) {
                classID = CCharacterCreation::GetRandomClassID();
                CCharacterCreation::m_selectedClassID = classID;
            }

            data.m_preferences.m_classId = classID;
            CCharacterComponent::ValidateComponentData(&data, COMPONENT_CONTEXT_0);
            CCharacterCreation::InitCharacterComponents(&data, false);
        } else {
            data.m_preferences.m_sexID = CCharacterCreation::m_character->m_data.m_preferences.m_sexID;
            data.m_preferences.m_raceID = CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex];
            CCharacterCreation::CalcClasses(data.m_preferences.m_raceID);

            if (!IsRaceClassValid(data.m_preferences.m_raceID, CCharacterCreation::m_selectedClassID)) {
                CCharacterCreation::SetSelectedClass(CCharacterCreation::GetRandomClassID());
            }

            data.m_preferences.m_classId = CCharacterCreation::m_selectedClassID;
            CCharacterCreation::InitCharacterComponents(&data, true);
        }
    }

    CCharacterCreation::m_nameGen.LoadNames(CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex], CCharacterCreation::m_character->m_data.m_preferences.m_sexID);
    CCharacterCreation::Dress();
    CCharacterCreation::SyncAlteredCharacter(); // Custom: the altered form preview follows the choices
    CGlueLoading::StartLoad(CCharacterCreation::m_character, 1);
}

// VERIFY: abfffae0 auto
// OFFSET: 0x004E2330
void CCharacterCreation::SetToExistingCharacter(uint32_t index) {
    CharacterSelectionDisplay* display = CCharacterSelection::GetCharacterDisplay(index);

    if (!display) {
        return;
    }

    CCharacterCreation::m_existingCharacterIndex = index;

    ComponentData data;
    data.m_preferences.m_raceID = display->m_characterInfo.m_raceID;
    data.m_preferences.m_sexID = display->m_characterInfo.m_sexID;
    data.m_preferences.m_classId = display->m_characterInfo.m_classID;
    data.m_preferences.m_skinID = display->m_characterInfo.m_skinColorID;
    data.m_preferences.m_hairStyleId = display->m_characterInfo.m_hairStyleID;
    data.m_preferences.m_hairColorId = display->m_characterInfo.m_hairColorID;
    data.m_preferences.m_facialHairId = display->m_characterInfo.m_facialHairStyleID;
    data.m_preferences.m_faceId = display->m_characterInfo.m_faceID;
    CCharacterCreation::InitCharacterComponents(&data, false);
    CCharacterCreation::CalcClasses(data.m_preferences.m_raceID);
    CCharacterCreation::InitCharacterComponents(&data, false);

    CCharacterCreation::m_selectedClassID = display->m_characterInfo.m_classID;

    for (uint32_t i = 0; i < CCharacterCreation::m_races.Count(); i++) {
        if (CCharacterCreation::m_races.m_data[i] == display->m_characterInfo.m_raceID) {
            CCharacterCreation::m_raceIndex = i;
            break;
        }
    }

    CCharacterCreation::m_nameGen.LoadNames(CCharacterCreation::m_character->m_data.m_preferences.m_raceID, CCharacterCreation::m_character->m_data.m_preferences.m_sexID);
    CCharacterCreation::SyncAlteredCharacter(); // Custom: the altered form preview follows the choices
    CGlueLoading::StartLoad(CCharacterCreation::m_character, 1);
}
