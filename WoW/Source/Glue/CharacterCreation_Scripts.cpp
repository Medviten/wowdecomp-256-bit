#include "CharacterCreation.h"
#include <cstdint>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleModel.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ChrClassesRec.h"
#include "WoW/Source/DB/Rec/ChrRacesRec.h"
#include "WoW/Source/DB/Rec/FactionGroupRec.h"
#include "WoW/Source/DB/Rec/FactionTemplateRec.h"
#include "WoW/Source/DB/Rec/SpellShapeshiftFormRec.h"
#include "WoW/Source/Client.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Services/Camera.h"
#include "Engine/Source/Tempest/CAaSphere.h"
#include <cstdlib>
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/CSimpleModelFFX.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Ui/BarberShop.h"
#include "WoW/Source/Ui/LFGInfo.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/Types.h"


// VERIFY: a2e71192 auto
// OFFSET: 0x004E05B0
static int32_t Script_SetCharCustomizeFrame(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: SetCharCustomizeFrame(\"frameName\")");
        return 0;
    }

    CSimpleModelFFX* frame = static_cast<CSimpleModelFFX*>(CScriptObject::GetScriptObjectByName(lua_tolstring(L, 1, nullptr), CSimpleModel::GetObjectType()));

    if (frame) {
        CCharacterCreation::m_charCustomizeFrame = frame;
    }

    return 0;
}

// VERIFY: 3c9ef8ef manual
// OFFSET: 0x004DFA70
static int32_t Script_IsRaceClassRestricted(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: Script_IsRaceClassRestricted(raceID, classID)");
        return 0;
    }

    int32_t raceID = static_cast<int32_t>(lua_tonumber(L, 1));
    int32_t classID = static_cast<int32_t>(lua_tonumber(L, 2));
    bool restricted;

    switch (raceID) {
        case 1: {
            restricted = (CCharacterSelection::s_raceRestrictions[0] & (1 << classID)) != 0;
            break;
        }

        case 2: {
            restricted = (CCharacterSelection::s_raceRestrictions[5] & (1 << classID)) != 0;
            break;
        }

        case 3: {
            restricted = (CCharacterSelection::s_raceRestrictions[1] & (1 << classID)) != 0;
            break;
        }

        case 4: {
            restricted = (CCharacterSelection::s_raceRestrictions[3] & (1 << classID)) != 0;
            break;
        }

        case 5: {
            restricted = (CCharacterSelection::s_raceRestrictions[8] & (1 << classID)) != 0;
            break;
        }

        case 6: {
            restricted = (CCharacterSelection::s_raceRestrictions[7] & (1 << classID)) != 0;
            break;
        }

        case 7: {
            restricted = (CCharacterSelection::s_raceRestrictions[2] & (1 << classID)) != 0;
            break;
        }

        case 8: {
            restricted = (CCharacterSelection::s_raceRestrictions[6] & (1 << classID)) != 0;
            break;
        }

        case 10: {
            restricted = (CCharacterSelection::s_raceRestrictions[9] & (1 << classID)) != 0;
            break;
        }

        case 11: {
            restricted = (CCharacterSelection::s_raceRestrictions[4] & (1 << classID)) != 0;
            break;
        }

        default: {
            restricted = false;
            luaL_error(L, "Script_IsRaceClassRestricted: unsupported race ID(%d)", raceID);
            break;
        }
    }

    lua_pushnumber(L, restricted);
    return 1;
}

// VERIFY: 8e30f704 auto
// OFFSET: 0x004DFC90
static int32_t Script_UpdateCustomizationScene(lua_State* L) {
    CCharacterCreation::m_character->RenderPrep(0);

    // Custom: altered form preview
    if (CCharacterCreation::m_alteredCharacter) {
        CCharacterCreation::m_alteredCharacter->RenderPrep(0);
    }

    return 0;
}

// Custom: AzerothCore's world.player_shapeshift_model, shipped client-side as DBFilesClient\PlayerShapeshiftModel.csv
// (lines "ShapeshiftID,RaceID,CustomizationID,GenderID,ModelID"; anything not starting with a digit is skipped), so
// character creation can preview druid forms the way the server will show them
struct ShapeshiftModelRow {
    int32_t m_form;
    int32_t m_race;
    int32_t m_customization;
    int32_t m_gender;
    int32_t m_model;
};

static TSGrowableArray<ShapeshiftModelRow> s_shapeshiftModels;
static bool s_shapeshiftModelsLoaded;

static void LoadShapeshiftModels() {
    if (s_shapeshiftModelsLoaded) {
        return;
    }

    s_shapeshiftModelsLoaded = true;

    char* data;
    uint32_t size;

    if (!SFile::LoadFile("DBFilesClient\\PlayerShapeshiftModel.csv", reinterpret_cast<void**>(&data), &size, 1, nullptr) || !data) {
        return;
    }

    const char* line = data;

    while (*line) {
        const char* end = line;

        while (*end && *end != '\n') {
            end++;
        }

        // Values may be quoted ("1","4","0","2","29407", as MySQL tools export them); the header line isn't numeric
        const char* first = line;

        while (first < end && (*first == '"' || *first == '\'' || *first == ' ' || *first == '\t')) {
            first++;
        }

        if (first < end && *first >= '0' && *first <= '9') {
            int32_t values[5];
            uint32_t count = 0;
            const char* cursor = first;

            while (count < 5 && cursor < end) {
                char* next;
                long value = strtol(cursor, &next, 10);

                if (next == cursor) {
                    cursor++;
                    continue;
                }

                values[count++] = static_cast<int32_t>(value);
                cursor = next;
            }

            if (count == 5) {
                ShapeshiftModelRow* row = s_shapeshiftModels.New();
                row->m_form = values[0];
                row->m_race = values[1];
                row->m_customization = values[2];
                row->m_gender = values[3];
                row->m_model = values[4];
            }
        }

        line = *end ? end + 1 : end;
    }

    SFile::Unload(data);
}

static int32_t FindShapeshiftModel(int32_t form, int32_t race, int32_t customization, int32_t gender) {
    for (uint32_t i = 0; i < s_shapeshiftModels.Count(); i++) {
        const ShapeshiftModelRow& row = s_shapeshiftModels[i];

        // A row whose display the client doesn't have (e.g. one only in the server's DBC tables) can't be shown, so
        // it is skipped and the lookup falls through to the next, more general row or the DBC default
        if (row.m_form == form && row.m_race == race && row.m_customization == customization && row.m_gender == gender && g_creatureDisplayInfoDB.GetRecord(row.m_model)) {
            return row.m_model;
        }
    }

    return 0;
}

static bool IsHordeRace(int32_t race) {
    ChrRacesRec* raceRec = g_chrRacesDB.GetRecord(race);
    FactionTemplateRec* factionTemplate = raceRec ? g_factionTemplateDB.GetRecord(raceRec->m_factionID) : nullptr;

    if (!factionTemplate) {
        return false;
    }

    for (int32_t i = 0; i < g_factionGroupDB.m_numRecords; i++) {
        FactionGroupRec* group = &g_factionGroupDB.m_records[i];

        if (group->m_maskID && ((1 << group->m_maskID) & factionTemplate->m_factionGroup) && !SStrCmpI(group->m_internalName, "Horde", STORM_MAX_STR)) {
            return true;
        }
    }

    return false;
}

// Mirrors AzerothCore's Unit::GetModelForForm / ObjectMgr::GetModelForShapeshift (as patched on this server): the table
// keyed by skin (fur) color for the races in shapeshiftSkinColorRaces and hair color for everyone else, with 255 = any
// customization and gender 2 = either, then SpellShapeshiftForm.dbc's Alliance/Horde model by faction
static int32_t GetShapeshiftDisplay(int32_t form, const CharacterPreferences& prefs) {
    LoadShapeshiftModels();

    bool horde = IsHordeRace(prefs.m_raceID);
    int32_t customization = ClientShapeshiftUsesSkinColor(prefs.m_raceID) ? prefs.m_skinID : prefs.m_hairColorId;
    int32_t race = prefs.m_raceID;
    int32_t gender = prefs.m_sexID;
    int32_t model;

    if ((model = FindShapeshiftModel(form, race, customization, gender)) || (model = FindShapeshiftModel(form, race, customization, 2)) || (model = FindShapeshiftModel(form, race, 255, gender)) || (model = FindShapeshiftModel(form, race, 255, 2))) {
        return model;
    }

    SpellShapeshiftFormRec* formRec = g_spellShapeshiftFormDB.GetRecord(form);

    if (!formRec) {
        return 0;
    }

    model = horde ? formRec->m_creatureDisplayID[1] : formRec->m_creatureDisplayID[0];
    return model ? model : formRec->m_creatureDisplayID[0];
}

// Custom: SetModelCreatureDisplay("frameName", displayID) shows a creature display (model plus its skins) in a Model
// frame, lit by the frame's own light and framed whole by a camera aimed at its bounding sphere; 0 clears the frame.
// Used by the druid form preview. (A plain Model frame draws only in its own rectangle; a ModelFFX would clear the
// whole screen.)
static int32_t Script_SetModelCreatureDisplay(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: SetModelCreatureDisplay(\"frameName\", displayID)");
        return 0;
    }

    CSimpleModel* frame = static_cast<CSimpleModel*>(CScriptObject::GetScriptObjectByName(lua_tolstring(L, 1, nullptr), CSimpleModel::GetObjectType()));

    if (!frame) {
        luaL_error(L, "SetModelCreatureDisplay: '%s' is not a Model frame", lua_tolstring(L, 1, nullptr));
        return 0;
    }

    int32_t displayId = lua_tointeger(L, 2);
    CreatureDisplayInfoRec* displayRec = displayId > 0 ? g_creatureDisplayInfoDB.GetRecord(displayId) : nullptr;
    CreatureModelDataRec* modelRec = displayRec ? g_creatureModelDataDB.GetRecord(displayRec->m_modelID) : nullptr;

    if (!modelRec || !modelRec->m_modelName || !*modelRec->m_modelName) {
        frame->SetModel(static_cast<CM2Model*>(nullptr));
        return 0;
    }

    CM2Model* model = frame->GetScene()->CreateModel(modelRec->m_modelName, 0);

    if (!model) {
        return 0;
    }

    if (!(model->m_fFlags & 0x1)) {
        model->WaitForLoad(nullptr);
    }

    CCharacterComponent::ReplaceMonsterSkin(model, displayRec, modelRec);

    frame->m_light.SetLightType(M2LIGHT_DIRECT);
    frame->m_light.m_dirColor = C3Vector(0.8f, 0.8f, 0.64f);
    frame->m_light.m_ambColor = C3Vector(0.7f, 0.7f, 0.7f);
    frame->m_light.SetDirection(C3Vector(0.0f, 1.0f, 0.0f));
    frame->m_light.SetVisible(1);

    frame->SetModel(model);
    model->SetBoneSequence(0xFFFFFFFF, 0, 0xFFFFFFFF, 0, 1.0f, 1, 1);

    // Like CGCharacterModelBase::ConfigureCamera's fallback, scaled to the model: about 3.4 radii in front, a little above (closer than the paperdoll framing, so the form fills the preview)
    CAaSphere sphere = model->GetBoundingSphere();
    float radius = sphere.r > 0.1f ? sphere.r : 1.0f;
    CCamera* camera = CameraCreate();
    CameraSetFloat(camera, 4, 0.5f);
    CameraSetCoord(camera, 8, sphere.c, 0);
    CameraSetCoord(camera, 7, C3Vector(sphere.c.x + radius * 3.4f, sphere.c.y, sphere.c.z + radius * 0.9f), 0);
    CameraSetFloat(camera, 3, 0.027777778f);
    frame->SetCamera(camera);
    HandleClose(reinterpret_cast<HOBJECT>(camera));

    model->Release();
    return 0;
}

// Custom: displayID, formName = GetCharCustomizeShapeshiftDisplay(form) for the character being created
static int32_t Script_GetCharCustomizeShapeshiftDisplay(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: GetCharCustomizeShapeshiftDisplay(form)");
        return 0;
    }

    int32_t form = lua_tointeger(L, 1);

    if (!CCharacterCreation::m_character) {
        lua_pushnumber(L, 0.0);
        lua_pushnil(L);
        return 2;
    }

    SpellShapeshiftFormRec* formRec = g_spellShapeshiftFormDB.GetRecord(form);
    lua_pushnumber(L, GetShapeshiftDisplay(form, CCharacterCreation::m_character->m_data.m_preferences));

    if (formRec && formRec->m_name) {
        lua_pushstring(L, formRec->m_name);
    } else {
        lua_pushnil(L);
    }

    return 2;
}

// Custom: Cataclysm-style altered form preview (alteredFormRaces): HasAlteredForm(), IsViewingAlteredForm(),
// SetViewingAlteredForm(bool)
// Custom: name, fileString = GetAlteredFormRace() for the selected race's altered form (nil when it has none), like
// GetNameForRace for the race itself
static int32_t Script_GetAlteredFormRace(lua_State* L) {
    uint32_t altered = CCharacterCreation::m_character ? ClientGetAlteredFormRace(CCharacterCreation::m_character->m_data.m_preferences.m_raceID) : 0;
    ChrRacesRec* raceRec = altered ? g_chrRacesDB.GetRecord(altered) : nullptr;
    const char* name = raceRec ? CGUnit_C::GetDisplayRaceNameFromRecord(raceRec, CCharacterCreation::m_character->m_data.m_preferences.m_sexID, nullptr) : nullptr;

    if (raceRec && name) {
        lua_pushstring(L, name);
        lua_pushstring(L, raceRec->m_clientFileString);
        return 2;
    }

    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
}

static int32_t Script_HasAlteredForm(lua_State* L) {
    lua_pushboolean(L, CCharacterCreation::m_alteredCharacter != nullptr);
    return 1;
}

static int32_t Script_IsViewingAlteredForm(lua_State* L) {
    lua_pushboolean(L, CCharacterCreation::m_alteredCharacter ? CCharacterCreation::m_alteredFormAtFront : 0);
    return 1;
}

static int32_t Script_SetViewingAlteredForm(lua_State* L) {
    CCharacterCreation::SetViewingAlteredForm(lua_toboolean(L, 1));
    return 0;
}

// Custom: IsMaleOnlyRace() for the selected race: true when its ChrRaces female display uses the male display's
// model, i.e. the race has no female model. The server (WHE core patch) creates such characters male, and the
// character creation UI hides the female button for them.
static int32_t Script_IsMaleOnlyRace(lua_State* L) {
    ChrRacesRec* raceRec = CCharacterCreation::m_character ? g_chrRacesDB.GetRecord(CCharacterCreation::m_character->m_data.m_preferences.m_raceID) : nullptr;
    CreatureDisplayInfoRec* maleDisplay = raceRec ? g_creatureDisplayInfoDB.GetRecord(raceRec->m_maleDisplayID) : nullptr;
    CreatureDisplayInfoRec* femaleDisplay = raceRec ? g_creatureDisplayInfoDB.GetRecord(raceRec->m_femaleDisplayID) : nullptr;

    lua_pushboolean(L, maleDisplay && femaleDisplay && maleDisplay->m_modelID == femaleDisplay->m_modelID);
    return 1;
}

// VERIFY: 262c3be3 manual
// OFFSET: 0x004DFCA0
static int32_t Script_GetRandomName(lua_State* L) {
    char name[16];
    uint32_t length;

    do {
        CCharacterCreation::m_nameGen.GenerateName(name, 14);
        length = SStrLen(name);
    } while (length && (ClientServices::CharacterValidateName(name) != CHAR_NAME_SUCCESS || length < 4 || length > 10));

    lua_pushstring(L, name);
    return 1;
}

// VERIFY: d0094aee auto
// OFFSET: 0x004E0620
static int32_t Script_SetCharCustomizeBackground(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: SetCharCustomizeBackground(\"filename\")");
        return 0;
    }

    CCharacterCreation::SetCharCustomizeModel(lua_tolstring(L, 1, nullptr));
    return 0;
}

// VERIFY: 63616981 auto
// OFFSET: 0x004E0670
static int32_t Script_GetNameForRace(lua_State* L) {
    ChrRacesRec* raceRec = g_chrRacesDB.GetRecord(CCharacterCreation::m_character->m_data.m_preferences.m_raceID);
    const char* name = CGUnit_C::GetDisplayRaceNameFromRecord(raceRec, CCharacterCreation::m_character->m_data.m_preferences.m_sexID, nullptr);

    if (raceRec && name) {
        lua_pushstring(L, name);
        lua_pushstring(L, raceRec->m_clientFileString);
        return 2;
    }

    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
}

// VERIFY: d4e5d7f0 manual
// OFFSET: 0x004E06F0
static int32_t Script_GetFactionForRace(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: GetFactionForRace(index)");
        return 0;
    }

    uint32_t index = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;
    int32_t raceID = index < CCharacterCreation::m_races.Count() ? CCharacterCreation::m_races.m_data[index] : 0;
    ChrRacesRec* raceRec = g_chrRacesDB.GetRecord(raceID);

    if (raceRec) {
        FactionTemplateRec* factionTemplate = g_factionTemplateDB.GetRecord(raceRec->m_factionID);

        if (factionTemplate) {
            for (int32_t i = 0; i < g_factionGroupDB.m_numRecords; i++) {
                FactionGroupRec* group = i >= 0 && i < g_factionGroupDB.m_numRecords ? &g_factionGroupDB.m_records[i] : nullptr;

                if (group && group->m_maskID && ((1 << group->m_maskID) & factionTemplate->m_factionGroup)) {
                    lua_pushstring(L, group->m_name);
                    lua_pushstring(L, group->m_internalName);
                    return 2;
                }
            }
        }
    }

    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
}

// VERIFY: 3d7dbaad manual
// OFFSET: 0x004E0830
static int32_t Script_GetAvailableClasses(lua_State* L) {
    int32_t count = g_chrClassesDB.m_numRecords;

    for (int32_t i = 0; i < count; i++) {
        ChrClassesRec* classRec = i >= 0 && i < g_chrClassesDB.m_numRecords ? &g_chrClassesDB.m_records[i] : nullptr;
        const char* name = CGUnit_C::GetDisplayClassNameFromRecord(classRec, CCharacterCreation::m_character->m_data.m_preferences.m_sexID, nullptr);
        bool available = ClientServices::Connection()->m_accountExpansion >= classRec->m_requiredExpansion;

        if (name) {
            lua_pushstring(L, name);
            lua_pushstring(L, classRec->m_filename);
            lua_pushnumber(L, available);
        } else {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushnil(L);
        }
    }

    return 3 * count;
}

// VERIFY: 3d392030 auto
// OFFSET: 0x004E0900
int32_t Script_GetHairCustomization(lua_State* L) {
    int32_t raceID = CCharacterCreation::m_raceIndex < CCharacterCreation::m_races.Count() ? CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex] : 0;
    ChrRacesRec* raceRec = g_chrRacesDB.GetRecord(raceID);

    if (raceRec) {
        lua_pushstring(L, raceRec->m_hairCustomization);
        return 1;
    }

    lua_pushstring(L, "NORMAL");
    return 1;
}

// VERIFY: 485ac57c auto
// OFFSET: 0x004E0970
int32_t Script_GetFacialHairCustomization(lua_State* L) {
    int32_t raceID = CCharacterCreation::m_raceIndex < CCharacterCreation::m_races.Count() ? CCharacterCreation::m_races.m_data[CCharacterCreation::m_raceIndex] : 0;
    ChrRacesRec* raceRec = g_chrRacesDB.GetRecord(raceID);

    if (raceRec) {
        lua_pushstring(L, raceRec->m_facialHairCustomization[CCharacterCreation::m_character->m_data.m_preferences.m_sexID]);
        return 1;
    }

    lua_pushstring(L, "NORMAL");
    return 1;
}

// VERIFY: 611d2416 auto
// OFFSET: 0x004E09F0
static int32_t Script_GetSelectedRace(lua_State* L) {
    lua_pushnumber(L, CCharacterCreation::m_raceIndex + 1);
    return 1;
}

// VERIFY: 72ee83a9 auto
// OFFSET: 0x004E0A30
static int32_t Script_GetSelectedSex(lua_State* L) {
    lua_pushnumber(L, g_scriptSexIndex[CCharacterCreation::m_character->m_data.m_preferences.m_sexID]);
    return 1;
}

// VERIFY: 0cf46c80 manual
// OFFSET: 0x004E0A60
static int32_t Script_GetSelectedClass(lua_State* L) {
    int32_t classID = CCharacterCreation::m_selectedClassID;
    ChrClassesRec* classRec = g_chrClassesDB.GetRecord(classID);

    if (!classRec) {
        return 0;
    }

    lua_pushstring(L, CGUnit_C::GetDisplayClassNameFromRecord(classRec, CCharacterCreation::m_character->m_data.m_preferences.m_sexID, nullptr));
    lua_pushstring(L, classRec->m_filename);

    int32_t index = 0;

    for (int32_t i = 0; i < g_chrClassesDB.m_numRecords; i++) {
        ChrClassesRec* rec = i >= 0 && i < g_chrClassesDB.m_numRecords ? &g_chrClassesDB.m_records[i] : nullptr;

        if (rec->m_id == classID) {
            index = i;
        }
    }

    lua_pushnumber(L, index + 1);

    uint8_t roles = CGLookingForGroup::GetClassRoles(CCharacterCreation::m_selectedClassID);
    lua_pushboolean(L, roles & 0x2);
    lua_pushboolean(L, roles & 0x4);
    lua_pushboolean(L, roles & 0x8);
    return 6;
}

// VERIFY: 8c236267 manual
// OFFSET: 0x004E0B50
static int32_t Script_CycleCharCustomization(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: CycleCharCustomization(index, delta)");
        return 0;
    }

    CHAR_CUSTOMIZATION_TYPE type = static_cast<CHAR_CUSTOMIZATION_TYPE>(static_cast<int32_t>(lua_tonumber(L, 1)) - 1);
    int32_t delta = static_cast<int32_t>(lua_tonumber(L, 2));
    CCharacterCreation::CycleCharCustomization(type, delta);
    return 0;
}

// VERIFY: e5380494 auto
// OFFSET: 0x004E0BE0
static int32_t Script_GetCharacterCreateFacing(lua_State* L) {
    lua_pushnumber(L, CCharacterCreation::m_charFacing * 57.29578f);
    return 1;
}

// VERIFY: 2d8b49e6 auto
// OFFSET: 0x004E0C10
static int32_t Script_SetCharacterCreateFacing(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: SetCharacterCreateFacing(degrees)");
        return 0;
    }

    CCharacterCreation::SetCharacterFacing(lua_tonumber(L, 1) * 0.017453292f);
    return 1;
}

// VERIFY: 8699a7d2 auto
// OFFSET: 0x004E0C60
static int32_t Script_CreateCharacter(lua_State* L) {
    if (lua_isstring(L, 1)) {
        lua_tolstring(L, 1, nullptr);
    }

    CCharacterCreation::CreateCharacter(lua_tolstring(L, 1, nullptr));
    return 0;
}

// VERIFY: 0866ae33 auto
// OFFSET: 0x004E0CA0
static int32_t Script_PaidChange_GetCurrentRaceIndex(lua_State* L) {
    lua_pushnumber(L, CCharacterCreation::GetExistingRaceIndex() + 1);
    return 1;
}

// VERIFY: 6c0afe43 auto
// OFFSET: 0x004E0CD0
static int32_t Script_PaidChange_GetCurrentClassIndex(lua_State* L) {
    lua_pushnumber(L, CCharacterCreation::GetExistingClassIndex() + 1);
    return 1;
}

// VERIFY: c7dacc88 manual
// OFFSET: 0x004E0D00
static int32_t Script_IsRaceClassValid(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: IsRaceClassValid(raceIndex, classIndex)");
        return 0;
    }

    uint32_t raceIndex = static_cast<uint32_t>(lua_tointeger(L, 1) - 1);
    int32_t classIndex = static_cast<int32_t>(lua_tointeger(L, 2) - 1);
    ChrClassesRec* classRec = classIndex >= 0 && classIndex < g_chrClassesDB.m_numRecords ? &g_chrClassesDB.m_records[classIndex] : nullptr;

    if (!classRec) {
        return 0;
    }

    int32_t classID = classRec->m_id;
    int32_t raceID = raceIndex < CCharacterCreation::m_races.Count() ? CCharacterCreation::m_races.m_data[raceIndex] : 0;

    if (IsRaceClassValid(raceID, classID)) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: e8f619c7 auto
// OFFSET: 0x004E0DD0
static int32_t Script_GetCreateBackgroundModel(lua_State* L) {
    if (SFile::IsTrial()) {
        lua_pushstring(L, "CharacterSelect");
        return 1;
    }

    if (CCharacterCreation::m_selectedClassID == 6) {
        ChrClassesRec* classRec = g_chrClassesDB.GetRecord(6);

        if (classRec) {
            lua_pushstring(L, classRec->m_filename);
            return 1;
        }
    } else {
        uint32_t raceID = CCharacterCreation::m_character->m_data.m_preferences.m_raceID;

        if (raceID == 7) {
            raceID = 3;
        } else if (raceID == 8) {
            raceID = 2;
        }

        ChrRacesRec* raceRec = g_chrRacesDB.GetRecord(raceID);

        if (raceRec) {
            lua_pushstring(L, raceRec->m_clientFileString);
            return 1;
        }
    }

    lua_pushstring(L, "");
    return 1;
}

// VERIFY: 2c992d0a auto
// OFFSET: 0x004E1880
static int32_t Script_GetAvailableRaces(lua_State* L) {
    uint32_t count = CCharacterCreation::m_races.Count();

    for (uint32_t i = 0; i < count; i++) {
        lua_pushstring(L, CGUnit_C::GetDisplayRaceNameFromRecord(g_chrRacesDB.GetRecord(CCharacterCreation::m_races.m_data[i]), CCharacterCreation::m_character->m_data.m_preferences.m_sexID, nullptr));

        ChrRacesRec* raceRec = g_chrRacesDB.GetRecord(CCharacterCreation::m_races.m_data[i]);
        lua_pushstring(L, raceRec ? raceRec->m_clientFileString : nullptr);

        raceRec = g_chrRacesDB.GetRecord(CCharacterCreation::m_races.m_data[i]);
        bool available = raceRec ? ClientServices::Connection()->m_accountExpansion >= raceRec->m_requiredExpansion : false;
        lua_pushnumber(L, available);
    }

    return 3 * count;
}

// VERIFY: f1ab0bab manual
// OFFSET: 0x004E19A0
static int32_t Script_GetClassesForRace(lua_State* L) {
    uint32_t count = CCharacterCreation::m_classes.Count();

    for (uint32_t i = 0; i < count; i++) {
        ChrClassesRec* classRec = i < CCharacterCreation::m_classes.Count() ? CCharacterCreation::m_classes.m_data[i] : nullptr;
        const char* name = CGUnit_C::GetDisplayClassNameFromRecord(classRec, CCharacterCreation::m_character->m_data.m_preferences.m_sexID, nullptr);
        bool available = ClientServices::Connection()->m_accountExpansion >= classRec->m_requiredExpansion;

        if (name) {
            lua_pushstring(L, name);
            lua_pushstring(L, classRec->m_filename);
            lua_pushnumber(L, available);
        } else {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushnil(L);
        }
    }

    return 3 * count;
}

// VERIFY: 21f49f77 auto
// OFFSET: 0x004E1A60
static int32_t Script_SetSelectedSex(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: SetSelectedSex(index)");
        return 0;
    }

    lua_Integer sex = lua_tointeger(L, 1);

    for (uint32_t i = 0; i < 3; i++) {
        if (sex == g_scriptSexIndex[i]) {
            CCharacterCreation::SetSelectedSex(i);
        }
    }

    return 0;
}

// VERIFY: 2a9ba016 manual
// OFFSET: 0x004E1AC0
static int32_t Script_SetSelectedClass(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: SetSelectedClass(index)");
        return 0;
    }

    int32_t index = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;
    ChrClassesRec* classRec = index >= 0 && index < g_chrClassesDB.m_numRecords ? &g_chrClassesDB.m_records[index] : nullptr;
    CCharacterCreation::SetSelectedClass(classRec->m_id);
    return 0;
}

// VERIFY: 19e1ce07 auto
// OFFSET: 0x004E1B60
static int32_t Script_RandomizeCharCustomization(lua_State* L) {
    CCharacterCreation::RandomizeCharCustomization();
    return 0;
}

// VERIFY: d87303ff auto
// OFFSET: 0x004E1B70
static int32_t Script_PaidChange_GetName(lua_State* L) {
    CharacterSelectionDisplay* display = CCharacterCreation::m_existingCharacterIndex >= 0 && CCharacterCreation::m_existingCharacterIndex < static_cast<int32_t>(CCharacterSelection::s_characterList.Count()) ? &CCharacterSelection::s_characterList[CCharacterCreation::m_existingCharacterIndex] : nullptr;

    if (display) {
        lua_pushstring(L, display->m_characterInfo.m_name);
        return 1;
    }

    lua_pushstring(L, nullptr);
    return 1;
}

// VERIFY: 2ac1e3c9 auto
// OFFSET: 0x004E2460
static int32_t Script_ResetCharCustomize(lua_State* L) {
    CCharacterCreation::ResetCharCustomizeInfo();
    return 0;
}

// VERIFY: 9ab9fa1a manual
// OFFSET: 0x004E2470
static int32_t Script_SetSelectedRace(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: SetSelectedRace(index)");
        return 0;
    }

    CCharacterCreation::SetSelectedRace(static_cast<uint32_t>(lua_tonumber(L, 1)) - 1);
    return 0;
}

// VERIFY: 63830e9b auto
// OFFSET: 0x004E24E0
static int32_t Script_UpdateCustomizationBackground(lua_State* L) {
    CCharacterCreation::SetSelectedRace(CCharacterCreation::m_raceIndex);
    return 0;
}

// VERIFY: ca2799b8 manual
// OFFSET: 0x004E2500
static int32_t Script_CustomizeExistingCharacter(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: CustomizeExistingCharacter(index)");
        return 0;
    }

    CCharacterCreation::SetToExistingCharacter(static_cast<uint32_t>(lua_tonumber(L, 1)) - 1);
    return 0;
}

// VERIFY: a77b42fd auto
// OFFSET: 0x00608560
static int32_t Script_PaidChange_GetPreviousRaceIndex(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// STATIC: 0x00AC4228
static FrameScript_Method s_scriptFunctions[] = {
    { "SetCharCustomizeFrame", &Script_SetCharCustomizeFrame },
    { "SetCharCustomizeBackground", &Script_SetCharCustomizeBackground },
    { "ResetCharCustomize", &Script_ResetCharCustomize },
    { "GetNameForRace", &Script_GetNameForRace },
    { "GetFactionForRace", &Script_GetFactionForRace },
    { "GetAvailableRaces", &Script_GetAvailableRaces },
    { "GetAvailableClasses", &Script_GetAvailableClasses },
    { "GetClassesForRace", &Script_GetClassesForRace },
    { "GetHairCustomization", &Script_GetHairCustomization },
    { "GetFacialHairCustomization", &Script_GetFacialHairCustomization },
    { "GetSelectedRace", &Script_GetSelectedRace },
    { "GetSelectedSex", &Script_GetSelectedSex },
    { "GetSelectedClass", &Script_GetSelectedClass },
    { "SetSelectedRace", &Script_SetSelectedRace },
    { "SetSelectedSex", &Script_SetSelectedSex },
    { "SetSelectedClass", &Script_SetSelectedClass },
    { "UpdateCustomizationBackground", &Script_UpdateCustomizationBackground },
    { "UpdateCustomizationScene", &Script_UpdateCustomizationScene },
    { "HasAlteredForm", &Script_HasAlteredForm },
    { "GetAlteredFormRace", &Script_GetAlteredFormRace },
    { "GetCharCustomizeShapeshiftDisplay", &Script_GetCharCustomizeShapeshiftDisplay },
    { "SetModelCreatureDisplay", &Script_SetModelCreatureDisplay },
    { "IsViewingAlteredForm", &Script_IsViewingAlteredForm },
    { "SetViewingAlteredForm", &Script_SetViewingAlteredForm },
    { "IsMaleOnlyRace", &Script_IsMaleOnlyRace },
    { "CycleCharCustomization", &Script_CycleCharCustomization },
    { "RandomizeCharCustomization", &Script_RandomizeCharCustomization },
    { "GetCharacterCreateFacing", &Script_GetCharacterCreateFacing },
    { "SetCharacterCreateFacing", &Script_SetCharacterCreateFacing },
    { "GetRandomName", &Script_GetRandomName },
    { "CreateCharacter", &Script_CreateCharacter },
    { "CustomizeExistingCharacter", &Script_CustomizeExistingCharacter },
    { "PaidChange_GetPreviousRaceIndex", &Script_PaidChange_GetPreviousRaceIndex },
    { "PaidChange_GetCurrentRaceIndex", &Script_PaidChange_GetCurrentRaceIndex },
    { "PaidChange_GetCurrentClassIndex", &Script_PaidChange_GetCurrentClassIndex },
    { "PaidChange_GetName", &Script_PaidChange_GetName },
    { "IsRaceClassValid", &Script_IsRaceClassValid },
    { "IsRaceClassRestricted", &Script_IsRaceClassRestricted },
    { "GetCreateBackgroundModel", &Script_GetCreateBackgroundModel },
};

// VERIFY: 88b9bf9d auto
// OFFSET: 0x004DFD00
void CharacterCreateRegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_scriptFunctions) / sizeof(s_scriptFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_scriptFunctions[i].m_name, s_scriptFunctions[i].m_method);
    }
}

// VERIFY: 50e76597 auto
// OFFSET: 0x004DFD30
void CharacterCreateUnregisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_scriptFunctions) / sizeof(s_scriptFunctions[0]); i++) {
        FrameScript_UnregisterFunction(s_scriptFunctions[i].m_name);
    }
}
