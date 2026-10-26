#include "CharacterSelectionDisplay.h"
#include "Engine/Source/Model2/M2Model.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Common/ComponentCore/RetailRace.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/ComponentData.h"
#include "WoW/Source/Glue/CGlueLoading.h"
#include "WoW/Source/Glue/CSimpleModelFFX.h"
#include "Engine/Source/Model2/M2Scene.h"
#include <cstdlib>

// Custom (Kalibro's retail race layer): the Two Forms mirror, the form a Worgen or Dracthyr is not showing, standing
// beside the selected character
CCharacterComponent* CharacterSelectionDisplay::m_formMirror;

// Sideways offset of the second form from the selected one, along the model frame's horizontal axis.
// WOW_FORM_SELECT_SPACING and WOW_FORM_SELECT_AXIS ("x" or "y") tune it without a rebuild.
static void FormMirrorOffset(float& x, float& y) {
    x = y = 0.0f;
    const char* spacing = std::getenv("WOW_FORM_SELECT_SPACING");
    const char* axis = std::getenv("WOW_FORM_SELECT_AXIS");
    (axis && (axis[0] == 'x' || axis[0] == 'X') ? x : y) = spacing ? static_cast<float>(std::atof(spacing)) : 1.2f;
}

void CharacterSelectionDisplay::ClearFormMirror() {
    CCharacterComponent* mirror = m_formMirror;
    if (!mirror) {
        return;
    }

    m_formMirror = nullptr;

    if (mirror->m_data.m_model && mirror->m_data.m_model->m_attachParent) {
        mirror->m_data.m_model->DetachFromParent();
    }

    CCharacterComponent::FreeComponent(mirror);
}

void CharacterSelectionDisplay::ShowFormMirror(CharacterSelectionDisplay& display) {
    ClearFormMirror();

    const CHARACTER_INFO& info = display.m_characterInfo;
    uint32_t form = 0;

    if (!CCharacterSelection::m_modelFrame || !CCharacterSelection::m_modelFrame->m_model || !display.m_component || !display.m_component->m_data.m_model
        || !RetailRaceParkedForm(info.m_guid.m_guid, form)) {
        return;
    }

    char path[260];

    if (!RetailRaceModelPath(info.m_raceID, info.m_sexID, path, sizeof(path), form)) {
        return;
    }

    ComponentData data;
    data.m_preferences.m_raceID = info.m_raceID;
    data.m_preferences.m_sexID = info.m_sexID;
    data.m_preferences.m_classId = info.m_classID;
    data.m_model = CCharacterSelection::m_modelFrame->GetScene()->CreateModel(path, 0);

    if (!data.m_model) {
        return;
    }

    data.m_flags |= 0x2;
    CCharacterComponent* mirror = CCharacterComponent::AllocComponent();
    mirror->Init(&data, nullptr);

    if (!RetailRaceApplyParkedTo(mirror, info.m_guid.m_guid)) {
        CCharacterComponent::FreeComponent(mirror);
        return;
    }

    data.m_model->m_lightingCallback = &CCharacterSelection::GenericLightingCallback;
    data.m_model->m_lightingArg = CCharacterSelection::m_modelFrame->m_lightArray2;
    data.m_model->SetBoneSequence(0xFFFFFFFF, 0, 0xFFFFFFFF, 0, 1.0f, 1, 1);

    CCharacterSelection::DressComponent(info, mirror);
    data.m_model->AttachToParent(CCharacterSelection::m_modelFrame->m_model, 0, nullptr, 0);
    m_formMirror = mirror;
    CGlueLoading::StartLoad(mirror, 1);
}

// INLINED
CharacterSelectionDisplay::CharacterSelectionDisplay() {
    this->m_component = nullptr;
    this->m_petAlpha = 1.0f;
    this->m_petModel = nullptr;
}

// INLINED
CharacterSelectionDisplay::~CharacterSelectionDisplay() {
    // Custom: drop the Two Forms mirror with the display
    ClearFormMirror();

    if (this->m_petModel) {
        this->m_petModel->Release();
    }

    if (this->m_component) {
        CCharacterComponent::FreeComponent(this->m_component);
        this->m_component = nullptr;
    }
}

// VERIFY: 4e7bb4fe auto
// OFFSET: 0x004D9160
int32_t CharacterSelectionDisplay::SetSelectedCharacterInfo(CHARACTER_INFO* info) {
    CGlueMgr::m_characterInfo = info;

    if (!info) {
        return 0;
    }

    CGlueMgr::s_displayedCharacterIndex = CCharacterSelection::s_selectionIndex;
    return 1;
}

// VERIFY: 96adbffe manual
// OFFSET: 0x004E2E70
void CharacterSelectionDisplay::SetCharacterFacing(float rotation) {
    if (!CCharacterSelection::s_characterCount) {
        return;
    }

    CCharacterSelection::s_charFacing = rotation;

    if (CCharacterSelection::s_characterList.Count() <= 0) {
        return;
    }

    CharacterSelectionDisplay& display = CCharacterSelection::s_characterList[CCharacterSelection::s_selectionIndex];

    if (display.m_component->m_data.m_model) {
        C3Vector position(0.0f, 0.0f, 0.0f);
        // Custom: retail races are previewed at their own scale, and the Two Forms mirror stands beside them
        display.m_component->m_data.m_model->SetWorldTransform(position, rotation,
            RetailRacePreviewScale(display.m_characterInfo.m_raceID));
    }

    if (m_formMirror && m_formMirror->m_data.m_model) {
        C3Vector position(0.0f, 0.0f, 0.0f);
        FormMirrorOffset(position.x, position.y);
        m_formMirror->m_data.m_model->SetWorldTransform(position, rotation,
            RetailRacePreviewScale(display.m_characterInfo.m_raceID));
    }
}
