#include "Engine/Source/Retail/RetailLookWire.h"
#include "Engine/Source/Retail/BodyShape.h"
#include "WoW/Common/ComponentCore/RetailRace.h"
#include "Engine/Source/Base/CDataStore.h"
#include <cstring>

#if defined(WOWDECOMP_RETAIL_ASSETS)

#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Common/TextureCache.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/HelmetGeosetVisDataRec.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/ChrRacesRec.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Gx/CGxTexFlags.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Retail/Appearance.h"
#include "Engine/Source/Retail/BusyNotice.h"
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/Compose.h"
#include "Engine/Source/Retail/Equipment.h"
#include "Engine/Source/Retail/RaceGeometry.h"
#include "Engine/Source/Retail/EyeEffects.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Retail/LegacyAppearance.h"
#include "Engine/Source/Retail/RacePolicy.h"
#include "Engine/Source/Retail/SlimeGiant.h"
#include "Engine/Source/Retail/Centaur.h"
#include "Engine/Source/Retail/CentaurAppearance.h"
#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Glue/CGlueLoading.h"
#include "WoW/Source/Glue/CharacterCreation.h"
#include "WoW/Source/Client.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include <cstdarg>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <unordered_map>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace app = Retail::Db2::appearance;

namespace {

// Diagnostics: Logs\RetailRace.log, one line per notable step with a millisecond tick.
void Log(const char* format, ...) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    FILE* file = std::fopen("Logs/RetailRace.log", "a");
    if (!file) return;
    std::fprintf(file, "%08lu ", static_cast<unsigned long>(GetTickCount()));
    va_list args;
    va_start(args, format);
    std::vfprintf(file, format, args);
    va_end(args);
    std::fputc(10, file);
    std::fclose(file);
}

// Forever uses separate IDs, tables, storage and cache; never add its IDs to Retail's source.
const std::map<uint32_t, uint32_t>& ForeverRaceMap() {
    static const auto map = [] {
        std::map<uint32_t, uint32_t> result;
        if (const char* env = std::getenv("WOW_FOREVER_RACES")) {
            std::istringstream input(env);
            std::string pair;
            while (std::getline(input, pair, ',')) {
                unsigned target = 0, source = 0;
                if (std::sscanf(pair.c_str(), "%u:%u", &target, &source) == 2 && target && source)
                    result[target] = source;
            }
            return result;
        }
        // Only opt in when an explicit Forever configuration is present.
        const char* config = std::getenv("WOW_FOREVER_CONFIG");
        bool enabled = config && *config;
        char exe[MAX_PATH];
        if (GetModuleFileNameA(nullptr, exe, sizeof(exe))) {
            const auto data = std::filesystem::path(exe).parent_path() / "Data";
            enabled = enabled || std::filesystem::is_regular_file(data / "Retail" / "ForeverCasc.ini")
                || std::filesystem::is_regular_file(data / "Forever" / "ForeverCasc.ini");
        }
        // Custom: no default Forever races (Kalibro maps his races 52/53 here; ours are Alliance Dracthyr and Gnoll)
        (void)enabled;
        return result;
    }();
    return map;
}

Retail::SourceKind RaceSource(uint32_t race) {
    return ForeverRaceMap().count(race) ? Retail::SourceKind::Forever : Retail::SourceKind::Retail;
}

// Custom: the races mapped to retail (or to Kalibro's Centaur / Slime Giant profiles) come from the retailRaces CVar,
// instead of WOW_RETAIL_RACES / Data\Retail\RetailRaces.csv and fixed race IDs
static_assert(Retail::Centaur::Profile == CLIENT_RETAIL_CENTAUR && Retail::SlimeGiant::Profile == CLIENT_RETAIL_SLIME_GIANT);

bool CentaurRace(uint32_t race) { return ClientGetRetailRace(race) == Retail::Centaur::Profile; }
bool SlimeGiantRace(uint32_t race) { return ClientGetRetailRace(race) == Retail::SlimeGiant::Profile; }

uint32_t RetailRaceId(uint32_t race) {
    const auto forever = ForeverRaceMap().find(race);
    if (forever != ForeverRaceMap().end()) return forever->second;
    return ClientGetRetailRace(race);
}

// Custom: Kalibro's packed appearance codec (EsteriaAppearance, for his Vulpera/Highmountain/Earthen/Haranir/Mechagnome
// IDs) is not used: every mapped race keeps its whole look in the retail look payload (RetailLookWire.h)
bool PackedRace(uint32_t) { return false; }
bool EsteriaRetailChoice(CCharacterComponent*, uint32_t, uint32_t&) { return false; }
bool EsteriaRetailStoreChoice(CCharacterComponent*, uint32_t, uint32_t) { return false; }
std::array<uint8_t, 13> EsteriaRetailFields(CCharacterComponent*) { return {}; }
void EsteriaRetailFields(CCharacterComponent*, const std::array<uint8_t, 13>&) {}

// The option of a race/sex model whose display name is `name` (0 if none).
uint32_t OptionByName(uint32_t retailRace, uint32_t sex, const char* name) {
    const uint32_t model = app::ChrModelForRace(retailRace, sex);
    for (uint32_t i = 0, n = app::OptionCount(model); i < n; ++i) {
        app::ChrOption option{};
        if (app::OptionAt(model, i, option) && app::OptionName(option.id) == name) return option.id;
    }
    return 0;
}

// The stock selectors (skin, face, hair style, hair colour, facial hair) mapped onto Retail options by name.
const char* FirstOption(uint32_t retailRace, uint32_t sex, std::initializer_list<const char*> names) {
    for (const char* name : names) if (OptionByName(retailRace, sex, name)) return name;
    return nullptr;
}

const char* StockOptionName(uint32_t variation, uint32_t retailRace, uint32_t sex, bool colors) {
    if (retailRace == Retail::Centaur::Profile) {
        if (variation == VARIATION_SKIN) return "Skin Color";
        if (variation == VARIATION_HAIR && !colors) return "Head Style";
        if (variation == VARIATION_FACIAL_HAIR && sex == 0) return "Facial Hair";
        return nullptr;
    }
    if (retailRace == Retail::SlimeGiant::Profile) {
        if (variation == VARIATION_SKIN) return "Slime Color";
        if (variation == VARIATION_FACE) return "Slime Drip";
        return nullptr;
    }
    switch (variation) {
        case VARIATION_SKIN: return FirstOption(retailRace, sex, {"Skin Color", "Fur Color", "Primary Color"});
        case VARIATION_FACE: return "Face";
        case VARIATION_HAIR: return colors ? FirstOption(retailRace, sex, {"Hair Color", "Horn Color"})
            : FirstOption(retailRace, sex, {"Hair Style", "Horn Style", "Hair"});
        case VARIATION_FACIAL_HAIR: return sex
            ? FirstOption(retailRace, sex, {"Makeup", "Earrings", "Piercings", "Horns", "Face Features", "Ears"})
            : FirstOption(retailRace, sex, {"Beard", "Facial Hair", "Beard Style", "Jaw Features", "Tusks", "Chin Decoration"});
        default: return nullptr;
    }
}

// ---- option model --------------------------------------------------------------------------------------------

enum class Stock { None, Skin, Face, HairStyle, HairColor, Facial };

struct OptionInfo {
    uint32_t id = 0;
    std::string name;
    uint32_t count = 0;
    uint32_t extra = 0; // trailing client-only choices past the DB2 ones
    Stock stock = Stock::None;
    std::vector<uint32_t> creationIndices; // UI positions map to the unchanged raw choice indexes.
};

// Blood Elf ears staged as Human-rigged collections (EsteriaWoW/tools/human_elf_ears.py), offered after the
// Human "Ears" choices. The DB2 recipe keeps its default ears; the geometry pass swaps them for the collection.
constexpr uint16_t ElfEarGeosets[] = {702, 703, 704};
constexpr const char* ElfEarNames[] = {"Elven (Long)", "Elven (Swept)", "Elven (Short)"};

Stock StockFor(const std::string& name, uint32_t retailRace, uint32_t sex) {
    const auto matches = [&](uint32_t variation, bool colors) {
        const char* selected = StockOptionName(variation, retailRace, sex, colors);
        return selected && name == selected;
    };
    if (matches(VARIATION_SKIN, true)) return Stock::Skin;
    if (matches(VARIATION_FACE, false)) return Stock::Face;
    if (matches(VARIATION_HAIR, false)) return Stock::HairStyle;
    if (matches(VARIATION_HAIR, true)) return Stock::HairColor;
    if (matches(VARIATION_FACIAL_HAIR, false)) return Stock::Facial;
    return Stock::None;
}

// The options of a race/sex model in display order, with their choice counts.
const std::vector<OptionInfo>& OptionsFor(uint32_t retailRace, uint32_t sex, uint32_t form = app::FormDefault) {
    static std::map<uint64_t, std::vector<OptionInfo>> cache;
    std::vector<OptionInfo>& options = cache[(uint64_t(Retail::currentSource == Retail::SourceKind::Forever) << 63) | (uint64_t(form) << 48) | (uint64_t(retailRace) << 8) | sex];
    if (!options.empty()) return options;
    const uint32_t model = app::ChrModelForRace(retailRace, sex, form);
    for (uint32_t i = 0, n = app::OptionCount(model); i < n; ++i) {
        app::ChrOption option{};
        if (!app::OptionAt(model, i, option)) continue;
        OptionInfo info;
        info.id = option.id;
        info.name = app::OptionName(option.id);
        info.count = app::ChoiceCount(option.id);
        info.stock = StockFor(info.name, retailRace, sex);
        for (uint32_t index = 0; index < info.count; ++index) {
            app::ChrChoice choice{};
            if (app::ChoiceAt(info.id, index, choice) && app::CreationChoiceAllowed(model, choice.id))
                info.creationIndices.push_back(index);
        }
        if (retailRace == 1 && info.name == "Ears" && Retail::currentSource == Retail::SourceKind::Retail) {
            info.extra = uint32_t(std::size(ElfEarGeosets));
            for (uint32_t extra = 0; extra < info.extra; ++extra) info.creationIndices.push_back(info.count + extra);
            info.count += info.extra;
        }
        options.push_back(std::move(info));
    }
    return options;
}

uint32_t* PreferenceFor(CharacterPreferences& prefs, Stock stock) {
    switch (stock) {
        case Stock::Skin: return &prefs.m_skinID;
        case Stock::Face: return &prefs.m_faceId;
        case Stock::HairStyle: return &prefs.m_hairStyleId;
        case Stock::HairColor: return &prefs.m_hairColorId;
        case Stock::Facial: return &prefs.m_facialHairId;
        default: return nullptr;
    }
}

// Selected choice index per option for components whose options go beyond the five stock selectors.
std::unordered_map<CCharacterComponent*, std::vector<uint32_t>> g_state;
std::unordered_map<CCharacterComponent*, uint64_t> g_stateIdentity;
std::unordered_map<CCharacterComponent*, CGUnit_C*> g_owners;
std::unordered_map<CCharacterComponent*, bool> g_eyeLightning;
std::unordered_map<CCharacterComponent*, bool> g_hiddenLegArmor;
std::unordered_map<CCharacterComponent*, uint32_t> g_lookApplied; // look stamp each component last applied
std::map<uint64_t, CharacterRetailLook::Look> g_looks;     // by guid: the character list and the players in view
std::map<uint64_t, CharacterRetailLook::Look> g_parked;   // by guid: the Two Forms look a character is not showing (character select)
std::map<uint64_t, uint32_t> g_lookStamp;                  // bumped whenever a guid's look is replaced

// Two Forms preview (Worgen Wolf/Human, Dracthyr Dragon/Visage): the form a component shows (absent = the race's
// default model) and what each form had selected when it was last left. Nothing here is saved or sent to the server.
struct FormSlot { std::vector<uint32_t> state; uint32_t prefs[5] = {}; };
std::unordered_map<CCharacterComponent*, uint32_t> g_form;
std::unordered_map<CCharacterComponent*, std::map<uint32_t, FormSlot>> g_formSlots;

uint32_t ActiveForm(CCharacterComponent* character) {
    const auto found = g_form.find(character);
    return found == g_form.end() ? uint32_t(app::FormDefault) : found->second;
}

bool FormsRace(uint32_t retail) { return retail == 22 || retail == 52 || retail == 70; }
uint32_t FormByIndex(uint32_t retail, uint32_t index) {
    return retail == 22 ? (index ? app::FormHuman : app::FormWolf) : (index ? app::FormVisage : app::FormDragon);
}
// The race's native form, the one a component shows until a look names another: Wolf, Dragon.
uint32_t DefaultForm(uint32_t retail) { return retail == 22 ? app::FormWolf : app::FormDragon; }
uint32_t EffectiveForm(CCharacterComponent* character, uint32_t retail) {
    return ActiveForm(character) ? ActiveForm(character) : DefaultForm(retail);
}

// Each form's recipe overwrites the component's geoset groups (the Dragon's hide the 13xx legs and 18xx belt); a form
// starts from the defaults, as a new component would. The cape group belongs to equipment and stays.
void ResetGeosets(CCharacterComponent* character) {
    const ComponentData fresh;
    const uint32_t cape = character->m_data.m_geosets[15];
    std::copy(std::begin(fresh.m_geosets), std::end(fresh.m_geosets), character->m_data.m_geosets);
    character->m_data.m_geosets[15] = cape;
}

void CheckStateIdentity(CCharacterComponent* character) {
    const auto& prefs = character->m_data.m_preferences;
    const uint64_t identity = (uint64_t(prefs.m_raceID) << 32) | prefs.m_sexID;
    auto& previous = g_stateIdentity[character];
    if (previous != identity) {
        g_state.erase(character); g_eyeLightning.erase(character); g_form.erase(character); g_formSlots.erase(character);
        previous = identity;
    }
}

// Parks the current form's selections and loads the other's; the caller refreshes the body.
void SwitchForm(CCharacterComponent* character, uint32_t retail, uint32_t next) {
    CheckStateIdentity(character);
    CharacterPreferences& prefs = character->m_data.m_preferences;
    uint32_t current = ActiveForm(character);
    if (!current) current = DefaultForm(retail);
    if (current != next) {
        auto& slots = g_formSlots[character];
        FormSlot& parked = slots[current];
        parked.state = g_state[character];
        parked.prefs[0] = prefs.m_skinID; parked.prefs[1] = prefs.m_faceId; parked.prefs[2] = prefs.m_hairStyleId;
        parked.prefs[3] = prefs.m_hairColorId; parked.prefs[4] = prefs.m_facialHairId;
        const FormSlot loaded = slots[next];
        ResetGeosets(character);
        g_state[character] = loaded.state;
        prefs.m_skinID = loaded.prefs[0]; prefs.m_faceId = loaded.prefs[1]; prefs.m_hairStyleId = loaded.prefs[2];
        prefs.m_hairColorId = loaded.prefs[3]; prefs.m_facialHairId = loaded.prefs[4];
    }
    g_form[character] = next;
}

// Creator option: show both forms of a Two Forms race at once, the first form left and the second right.
bool g_dual = false;

// What the creator remembers per race and sex while the component is rebuilt (class, sex or race change): the form
// on show, its live selections, and the parked selections of the other form.
struct FormMemory {
    uint32_t form = 0;
    std::vector<uint32_t> state;
    uint32_t prefs[5] = {};
    std::map<uint32_t, FormSlot> slots;
};
std::map<uint64_t, FormMemory> g_formMemory;
uint64_t FormMemoryKey(uint32_t race, uint32_t sex) { return (uint64_t(race) << 8) | sex; }

// Retail components seen by the geometry hook; ticked each frame in the world.
std::set<CCharacterComponent*> g_active;

uint32_t IndexOf(CCharacterComponent* character, const std::vector<OptionInfo>& options, size_t i) {
    CheckStateIdentity(character);
    if (!options[i].count) return 0;
    if (PackedRace(character->m_data.m_preferences.m_raceID)) {
        const auto state = g_state.find(character);
        if (state != g_state.end() && i < state->second.size() && state->second[i] != UINT32_MAX)
            return state->second[i] % options[i].count;
        uint32_t saved = 0;
        if (EsteriaRetailChoice(character, options[i].id, saved)) {
            for (uint32_t index = 0; index < options[i].count; ++index) {
                app::ChrChoice choice{};
                if (app::ChoiceAt(options[i].id, index, choice) && choice.id == saved) return index;
            }
        }
        return 0;
    }
    uint32_t value = 0;
    if (options[i].stock != Stock::None) {
        value = *PreferenceFor(character->m_data.m_preferences, options[i].stock);
    } else {
        const auto found = g_state.find(character);
        if (found != g_state.end() && i < found->second.size() && found->second[i] != UINT32_MAX) value = found->second[i];
    }
    return value % options[i].count;
}

void StoreIndex(CCharacterComponent* character, const std::vector<OptionInfo>& options, size_t i, uint32_t value) {
    CheckStateIdentity(character);
    if (!options[i].count) return;
    value %= options[i].count;
    if (PackedRace(character->m_data.m_preferences.m_raceID)) {
        app::ChrChoice choice{};
        if (!app::ChoiceAt(options[i].id, value, choice)) return;
        auto& state = g_state[character];
        state.resize(options.size(), UINT32_MAX);
        // Old IDs retain their installed packed codec. New choices remain preview-only until a
        // wider server look transport exists; they must not overwrite other packed selectors.
        state[i] = EsteriaRetailStoreChoice(character, options[i].id, choice.id) ? UINT32_MAX : value;
        return;
    }
    if (options[i].stock != Stock::None) {
        *PreferenceFor(character->m_data.m_preferences, options[i].stock) = value;
        return;
    }
    std::vector<uint32_t>& state = g_state[character];
    state.resize(options.size(), UINT32_MAX);
    state[i] = value;
}

// Choice IDs for a character, one per option. WOW_RETAIL_CHOICES ("id,id,...") overrides everything, for testing.
std::vector<uint32_t> Choices(uint32_t retailRace, CCharacterComponent* character) {
    std::vector<uint32_t> choices;
    if (const char* env = std::getenv("WOW_RETAIL_CHOICES")) {
        for (const char* c = env; *c;) {
            char* end = nullptr;
            choices.push_back(uint32_t(std::strtoul(c, &end, 10)));
            if (end == c) break;
            c = *end == ',' ? end + 1 : end;
        }
        return choices;
    }
    const std::vector<OptionInfo>& options = OptionsFor(retailRace, character->m_data.m_preferences.m_sexID, ActiveForm(character));
    for (size_t i = 0; i < options.size(); ++i) {
        app::ChrChoice choice{};
        uint32_t index = IndexOf(character, options, i);
        if (character == CCharacterCreation::m_character) {
            const auto& eligible = options[i].creationIndices;
            if (eligible.empty()) continue;
            if (std::find(eligible.begin(), eligible.end(), index) == eligible.end()) {
                index = eligible.front();
                StoreIndex(character, options, i, index);
            }
        }
        if (index < options[i].count - options[i].extra && app::ChoiceAt(options[i].id, index, choice))
            choices.push_back(choice.id);
    }
    return choices;
}

// The collection geoset of a selected client-only elf ear choice, or 0.
uint16_t ElfEarGeoset(CCharacterComponent* character, uint32_t retailRace) {
    const std::vector<OptionInfo>& options = OptionsFor(retailRace, character->m_data.m_preferences.m_sexID, ActiveForm(character));
    for (size_t i = 0; i < options.size(); ++i) {
        if (!options[i].extra) continue;
        const uint32_t index = IndexOf(character, options, i), first = options[i].count - options[i].extra;
        return index >= first ? ElfEarGeosets[index - first] : 0;
    }
    return 0;
}

// The shared model remembers the name it was created from; Retail models are created from "retail/<id>.m2".
bool IsRetailModel(CM2Model* model) {
    if (!model || !model->m_shared) return false;
    const char* name = model->m_shared->m_fileName;
    return Retail::IsCdnPath(name);
}

constexpr uint32_t CollectionSocket = UINT32_MAX - 1;
struct CollectionState {
    CM2Model* parent = nullptr;
    std::map<std::string, std::vector<uint16_t>> geosets;
};
std::unordered_map<CCharacterComponent*, CollectionState> g_collections;

std::string CollectionName(CM2Model* model) {
    std::string name = model->m_shared->m_fileName;
    std::replace(name.begin(), name.end(), '\\', '/');
    return name;
}

void BindCollectionTextures(CM2Model* parent) {
    if (!(parent->m_fFlags & 1) || !parent->m_textures) return;
    const auto& textures = parent->m_shared->m_data->m_textures;
    for (CM2Model* child = parent->m_attachList; child; child = child->m_attachNext) {
        if (child->m_attachmentId != CollectionSocket) continue;
        for (uint32_t i = 0; i < textures.m_count; ++i)
            if (textures[i].m_type && parent->m_textures[i]) child->ReplaceTexture(textures[i].m_type, parent->m_textures[i]);
    }
}

void FinishCollections(CCharacterComponent* character, bool refresh = false) {
    const auto state = g_collections.find(character);
    CM2Model* parent = character->m_data.m_model;
    if (state == g_collections.end() || state->second.parent != parent || !(parent->m_fFlags & 1)) return;
    for (CM2Model* child = parent->m_attachList; child; child = child->m_attachNext) {
        if (child->m_attachmentId != CollectionSocket || !(child->m_fFlags & 1)) continue;
        const auto selected = state->second.geosets.find(CollectionName(child));
        if (selected == state->second.geosets.end()) continue;
        const bool ready = child->HasSkinParent(parent);
        if (!ready && !child->SetSkinParent(parent)) {
            static std::set<std::string> reported;
            const auto key = std::string(parent->m_shared->m_fileName) + ":" + child->m_shared->m_fileName;
            if (reported.insert(key).second) Log("collection rig mismatch: %s", key.c_str());
            continue;
        }
        if (!ready || refresh) {
            child->SetGeometryVisible(0, 65535, 0);
            for (const auto geoset : selected->second) child->SetGeometryVisible(geoset, geoset, 1);
            if (SlimeGiantRace(character->m_data.m_preferences.m_raceID) ||
                CentaurRace(character->m_data.m_preferences.m_raceID))
                for (const auto geoset : character->m_data.m_geosets) child->SetGeometryVisible(geoset, geoset, 1);
            if (CentaurRace(character->m_data.m_preferences.m_raceID)) {
                child->SetGeometryVisible(2002, 2002, character->m_itemDisplayID[ITEMSLOT_BOOTS] ? 1 : 0);
                child->SetGeometryVisible(1800, 1899, 0);
                child->SetGeometryVisible(1805, 1805, character->m_itemDisplayID[ITEMSLOT_BELT] ? 1 : 0);
                child->SetGeometryVisible(1500, 1599, 0);
                ItemDisplayInfoRec cape;
                if (character->m_itemDisplayID[ITEMSLOT_CAPE] &&
                    g_itemDisplayInfoDB.GetRecord(character->m_itemDisplayID[ITEMSLOT_CAPE], &cape)) {
                    const uint32_t geoset = 1501 + cape.m_geosetGroup[0];
                    child->SetGeometryVisible(geoset, geoset, 1);
                }
            }
            BindCollectionTextures(parent);
        }
    }
}

// Whether the equipped helmet hides HelmetGeosetVisData slot `slot` (0 hair, 1..3 facial, 4 ears, 5..6 other) for
// the race's helmet-mask donor.
bool HelmetHides(CCharacterComponent* character, uint32_t slot) {
    const uint32_t sex = character->m_data.m_preferences.m_sexID;
    const uint32_t maskRace = Retail::HelmetMaskRace(RetailRaceId(character->m_data.m_preferences.m_raceID));
    ItemDisplayInfoRec display;
    if (!maskRace || maskRace >= 32 || sex > 1 || slot >= 7 || !character->m_itemDisplayID[ITEMSLOT_HEAD]
        || !g_itemDisplayInfoDB.GetRecord(character->m_itemDisplayID[ITEMSLOT_HEAD], &display)) return false;
    const auto* visibility = g_helmetGeosetVisDataDB.GetRecord(display.m_helmetGeosetVisID[sex]);
    // Custom: 256-bit hide masks, indexed by race id itself (RaceMask::HasBit)
    return visibility && visibility->m_hideGeoset[slot].HasBit(int32_t(maskRace));
}

void SyncCollections(CCharacterComponent* character, const app::Recipe& recipe) {
    CM2Model* parent = character->m_data.m_model;
    auto& state = g_collections[character];
    state.parent = parent;
    state.geosets.clear();
    if (SlimeGiantRace(character->m_data.m_preferences.m_raceID))
        state.geosets["custom/slimegiant/" + std::to_string(recipe.modelFileDataId) + "-clothing.m2"] = {1};
    if (CentaurRace(character->m_data.m_preferences.m_raceID))
        state.geosets["custom/centaur/" + std::to_string(recipe.modelFileDataId) + "-clothing.m2"] = {1};
    const uint16_t ears = ElfEarGeoset(character, RetailRaceId(character->m_data.m_preferences.m_raceID));
    if (ears && !HelmetHides(character, 4)) // geometry and equipment share one refresh, so this follows the helm
        state.geosets["custom/humanears/" + std::to_string(recipe.modelFileDataId) + "-ears.m2"] = {ears};
    for (const auto& part : recipe.attached)
        if (part.fileDataId && part.geoset % 100) { // group*100 is an explicitly hidden collection.
            Retail::Source().RegisterCharacterModel(part.fileDataId);
            state.geosets[Retail::FileName(part.fileDataId, ".m2")].push_back(part.geoset);
        }
    for (CM2Model* child = parent->m_attachList; child;) {
        CM2Model* next = child->m_attachNext;
        if (child->m_attachmentId == CollectionSocket && !state.geosets.count(CollectionName(child)))
            child->DetachFromParent();
        child = next;
    }
    for (const auto& selected : state.geosets) {
        bool exists = false;
        for (CM2Model* child = parent->m_attachList; child; child = child->m_attachNext)
            exists = exists || (child->m_attachmentId == CollectionSocket && selected.first == CollectionName(child));
        if (exists) continue;
        CM2Model* child = parent->m_scene->CreateModel(selected.first.c_str(), 0);
        if (!child) { Log("collection create failed: %s", selected.first.c_str()); continue; }
        child->SetGeometryVisible(0, 65535, 0); // Wait for a verified shared pose before exposing geometry.
        child->AttachToParent(parent, CollectionSocket, nullptr, 1);
        child->Release(); // The parent owns the attachment reference.
    }
    FinishCollections(character, true);
}

// The creator and character select run without an active player. In the world the stock validation applies.
bool InWorld() {
    return ClntObjMgrGetActivePlayer().m_guid != 0;
}

// Conditional character models belong to an appearance instance, never to a shared race skeleton.
bool SyncBodyModel(CCharacterComponent* character) {
    Retail::SourceScope sourceScope(RaceSource(character->m_data.m_preferences.m_raceID));
    const auto& prefs = character->m_data.m_preferences;
    const uint32_t retail = RetailRaceId(prefs.m_raceID);
    CM2Model* old = character->m_data.m_model;
    const uint32_t form = ActiveForm(character);
    const bool formBody = form && FormsRace(retail);
    if ((!formBody && (prefs.m_sexID || (retail != 2 && retail != 36))) || !IsRetailModel(old) ||
        !(old->m_fFlags & 1) || !old->m_scene) return false;

    const auto choices = Choices(retail, character);
    app::Recipe recipe;
    if (!app::BuildForCharacter(retail, prefs.m_sexID, choices.data(), uint32_t(choices.size()), recipe, form) ||
        !recipe.modelFileDataId) return false;
    const std::string path = retail == Retail::SlimeGiant::Profile ? Retail::SlimeGiant::Path(recipe.modelFileDataId)
        : retail == Retail::Centaur::Profile ? Retail::Centaur::Path(recipe.modelFileDataId)
        : Retail::FileName(recipe.modelFileDataId, ".m2");
    if (!_stricmp(CollectionName(old).c_str(), path.c_str())) return false;

    CGUnit_C* owner = nullptr;
    const auto found = g_owners.find(character);
    if (found != g_owners.end()) owner = found->second;
    // A world body is shared by the component and unit. Never update just one of those references.
    if (owner && (owner->m_characterComponent != character || owner->m_worldModel != old)) return false;
    if (InWorld() && !owner) return false;

    Retail::Source().RegisterCharacterModel(recipe.modelFileDataId);
    Retail::BusyNotice busy("Loading character posture");
    CM2Model* replacement = old->m_scene->CreateModel(path.c_str(), old->m_flags);
    if (!replacement) return false;
    replacement->WaitForLoad("Retail conditional body");
    if (!(replacement->m_fFlags & 1) || !replacement->m_shared || !replacement->m_shared->m_data ||
        !replacement->m_shared->m_skinData || _stricmp(CollectionName(replacement).c_str(), path.c_str())) {
        Log("conditional body load failed component %p model %s", static_cast<void*>(character), path.c_str());
        replacement->Release();
        return false;
    }
    // Reparenting detaches first, so reject unsupported equipment sockets before changing any links.
    for (CM2Model* child = old->m_attachList; child; child = child->m_attachNext) {
        if (child->m_attachmentId != CollectionSocket && !(child->m_fFlags & 0x40000) &&
            !replacement->HasAttachment(child->m_attachmentId)) {
            Log("conditional body missing socket component %p model %s socket %u",
                static_cast<void*>(character), path.c_str(), child->m_attachmentId);
            replacement->Release();
            return false;
        }
    }

    replacement->m_worldTransform = old->m_worldTransform;
    replacement->m_matrixF4 = old->m_matrixF4;
    replacement->m_scaleSq = old->m_scaleSq;
    replacement->m_frameOfReferenceMatrix = old->m_frameOfReferenceMatrix;
    replacement->m_frameOfReference = old->m_frameOfReference == &old->m_frameOfReferenceMatrix
        ? &replacement->m_frameOfReferenceMatrix : old->m_frameOfReference;
    replacement->m_fFlags = (replacement->m_fFlags & ~0x8004u) | (old->m_fFlags & 0x8004u);
    replacement->m_lighting = old->m_lighting;
    replacement->m_lightingCallback = old->m_lightingCallback;
    replacement->m_lightingArg = old->m_lightingArg;
    replacement->m_drawCallback = old->m_drawCallback;
    replacement->m_drawArg = old->m_drawArg;
    replacement->m_eventCallback = old->m_eventCallback;
    replacement->m_eventCallbackParam = old->m_eventCallbackParam;
    replacement->m_sequenceCallback = old->m_sequenceCallback;
    replacement->m_sequenceCallbackParam = old->m_sequenceCallbackParam;
    replacement->m_loadedCallback = old->m_loadedCallback;
    replacement->m_loadedArg = old->m_loadedArg;
    replacement->m_fadeAlpha = old->m_fadeAlpha;
    replacement->m_baseAlpha = old->m_baseAlpha;
    replacement->m_diffuseTintR = old->m_diffuseTintR;
    replacement->m_diffuseTintG = old->m_diffuseTintG;
    replacement->m_diffuseTintB = old->m_diffuseTintB;
    replacement->m_emissiveTintR = old->m_emissiveTintR;
    replacement->m_emissiveTintG = old->m_emissiveTintG;
    replacement->m_emissiveTintB = old->m_emissiveTintB;
    replacement->m_effectiveAlpha = old->m_effectiveAlpha;
    replacement->m_alpha = old->m_alpha;
    replacement->m_diffuseColor = old->m_diffuseColor;
    replacement->m_emissiveColor = old->m_emissiveColor;

    M2BoneSequenceInfo sequence{};
    if (old->GetBoneSequenceInfo(UINT32_MAX, sequence))
        replacement->SetBoneSequence(UINT32_MAX, sequence.m_sequenceId, sequence.m_variationIndex,
            sequence.m_time, sequence.m_speed, 0, 1);
    else replacement->SetBoneSequence(UINT32_MAX, 0, UINT32_MAX, 0, 1.0f, 0, 1);

    if (old->m_attachParent)
        replacement->AttachToParent(old->m_attachParent, old->m_attachmentId, nullptr,
            (old->m_fFlags & 0x40000) != 0);
    // The component reference protects old while its attachment and world-owner references change.
    if (old->m_attachParent) old->DetachFromParent();
    for (CM2Model* child = old->m_attachList; child;) {
        CM2Model* next = child->m_attachNext;
        if (child->m_attachmentId == CollectionSocket) child->DetachFromParent();
        else {
            const uint32_t socket = child->m_attachmentId;
            const bool root = (child->m_fFlags & 0x40000) != 0;
            ++child->m_refCount; // AttachToParent releases the old parent's attachment reference.
            child->AttachToParent(replacement, socket, nullptr, root);
            child->Release(); // Only the replacement parent's attachment reference remains.
        }
        child = next;
    }
    g_collections.erase(character);
    character->m_data.m_model = replacement; // CreateModel's reference is now component-owned.
    if (owner) {
        owner->m_torsoKeyBone = -1; // The loaded callback resolves the new rig's torso key.
        owner->SetCharacterModel(replacement); // Adds a separate unit reference; releases old's.
    }
    character->m_flags |= 0x4; // Dirty geometry; bit 1 belongs to the normal texture request lifecycle.
    character->m_dirtySections = UINT32_MAX;
    Log("conditional body swap component %p race %u model %s -> %s", static_cast<void*>(character),
        prefs.m_raceID, old->m_shared->m_fileName, path.c_str());
    old->Release(); // Release the former component reference last.
    return true;
}

// Faces added over the retail tables ship in the client archives: Esteria\RetailExtras.txt defines them and their
// textures carry file ids in this range, read from custom\nightborne\faces.
constexpr uint32_t kExtraTextureFirst = 8800000, kExtraTextureLimit = 8900000;

bool ExtrasFromArchive(std::string& text) {
    void* data = nullptr;
    uint32_t size = 0;
    if (!SFileLoadFileEx(nullptr, "Esteria\\RetailExtras.txt", &data, &size, 0, 0, nullptr) || !data) return false;
    text.assign(static_cast<const char*>(data), size);
    SFileUnloadFile(data);
    return true;
}

const bool g_extrasRegistered = (app::SetExtrasProvider(&ExtrasFromArchive), true);

bool FetchBlp(uint32_t fileDataId, std::vector<uint8_t>& bytes, void*) {
    const auto local = fileDataId >= kExtraTextureFirst && fileDataId < kExtraTextureLimit
        ? "custom\\nightborne\\faces\\" + std::to_string(fileDataId) + ".blp"
        : Retail::SlimeGiant::TexturePath(fileDataId);
    if (!local.empty()) {
        void* data = nullptr;
        uint32_t size = 0;
        bytes.clear();
        if (!SFileLoadFileEx(nullptr, local.c_str(), &data, &size, 0, 0, nullptr)) return false;
        const bool valid = data && size >= 148 && size <= 4 * 1024 * 1024;
        if (valid) bytes.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
        SFileUnloadFile(data);
        return valid;
    }
    std::string error;
    return Retail::Source().Read(Retail::FileName(fileDataId, ".blp").c_str(), bytes, error);
}

// Everything derived from (race, sex, choices): the recipe and its composed images, kept in memory.
struct Look {
    app::Recipe recipe;
    std::vector<Retail::ComposedTexture> composed;
    std::map<uint32_t, CTexture*> textures; // finished GPU textures for types that carry no equipment
};

// Paints the Blood Elf ear texture for the elf ear collections into an area of the Human body sheet that no Human
// mesh samples. The .ear file (EsteriaWoW/tools/human_elf_ears.py) is 'HEAR', u16 x, y, w, h, u16 n, n * (u16 x, y)
// tone samples, then w*h RGBA texels: rgb is detail relative to skin (128 = 1.0), alpha is coverage. The samples are
// the head skin around the ear opening, so the ears match the skin they join for every face and skin colour.
void PaintElfEars(uint32_t modelFileDataId, Retail::Image& sheet) {
    const std::string name = "custom/humanears/" + std::to_string(modelFileDataId) + "-ears.ear";
    void* data = nullptr;
    uint32_t size = 0;
    if (!SFile::LoadFile(name.c_str(), &data, &size, 0, nullptr)) return;
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint16_t header[5] = {};
    if (size >= 14 && !std::memcmp(bytes, "HEAR", 4)) std::memcpy(header, bytes + 4, sizeof(header));
    const uint32_t x = header[0], y = header[1], w = header[2], h = header[3], n = header[4];
    const auto* points = bytes + 14;
    bool valid = w && h && n && size == 14 + size_t(n) * 4 + size_t(w) * h * 4 && x + w <= sheet.width
        && y + h <= sheet.height && sheet.rgba.size() == size_t(sheet.width) * sheet.height * 4;
    for (uint32_t i = 0; valid && i < n; ++i)
        valid = uint32_t(points[i * 4] | points[i * 4 + 1] << 8) < sheet.width
            && uint32_t(points[i * 4 + 2] | points[i * 4 + 3] << 8) < sheet.height;
    if (!valid) {
        Log("elf ears: invalid %s", name.c_str());
        SFile::Unload(data);
        return;
    }
    uint32_t tone[3] = {};
    std::vector<uint8_t> samples(n);
    for (uint32_t channel = 0; channel < 3; ++channel) {
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t column = points[i * 4] | points[i * 4 + 1] << 8, row = points[i * 4 + 2] | points[i * 4 + 3] << 8;
            samples[i] = sheet.rgba[(size_t(row) * sheet.width + column) * 4 + channel];
        }
        std::nth_element(samples.begin(), samples.begin() + n / 2, samples.end());
        tone[channel] = samples[n / 2];
    }
    const uint8_t* texel = points + size_t(n) * 4;
    for (uint32_t row = 0; row < h; ++row) {
        for (uint32_t column = 0; column < w; ++column, texel += 4) {
            if (!texel[3]) continue;
            uint8_t* pixel = &sheet.rgba[(size_t(y + row) * sheet.width + x + column) * 4];
            for (uint32_t channel = 0; channel < 3; ++channel) {
                const uint32_t colour = std::min<uint32_t>(255, texel[channel] * tone[channel] / 128);
                pixel[channel] = uint8_t((pixel[channel] * (255 - texel[3]) + colour * texel[3]) / 255);
            }
        }
    }
    SFile::Unload(data);
}

Look& LookFor(uint32_t retail, uint32_t sex, const std::vector<uint32_t>& choices, std::string& key, uint32_t form) {
    static std::map<std::string, Look> looks;
    static std::deque<std::string> order;
    key = std::string(Retail::SourcePrefix()) + std::to_string(retail) + ":" + std::to_string(sex) + ":f" + std::to_string(form);
    for (const uint32_t choice : choices) key += ":" + std::to_string(choice);
    auto found = looks.find(key);
    if (found != looks.end()) return found->second;

    Retail::BusyNotice busy("Preparing character appearance");
    const unsigned long start = GetTickCount();
    // Bound memory while the player cycles through options: keep a few recent looks.
    while (order.size() >= 6) {
        const auto old = looks.find(order.front());
        if (old != looks.end()) {
            for (auto& texture : old->second.textures) HandleClose(reinterpret_cast<HOBJECT>(texture.second));
            looks.erase(old);
        }
        order.pop_front();
    }
    order.push_back(key);
    Look& look = looks[key];
    if (app::BuildForCharacter(retail, sex, choices.data(), uint32_t(choices.size()), look.recipe, form)) {
        std::string warnings;
        Retail::ComposeTextures(look.recipe, FetchBlp, nullptr, look.composed, warnings);
        if (retail == 1 && Retail::currentSource == Retail::SourceKind::Retail)
            for (auto& texture : look.composed)
                if (texture.textureType == 1) PaintElfEars(look.recipe.modelFileDataId, texture.image);
        if (retail == Retail::SlimeGiant::Profile || retail == Retail::Centaur::Profile) {
            uint32_t width = 0, height = 0;
            if (app::LayoutSize(look.recipe.textureLayoutId, width, height))
                look.composed.push_back({14, {width, height, std::vector<uint8_t>(size_t(width) * height * 4, 0)}});
        }
        if (!warnings.empty()) Log("compose warnings: %s", warnings.c_str());
    }
    Log("look %s: model %u geosets %zu layers %zu composed %zu in %lu ms", key.c_str(), look.recipe.modelFileDataId,
        look.recipe.geosets.size(), look.recipe.layers.size(), look.composed.size(), GetTickCount() - start);
    return look;
}

CTexture* MakeTexture(const std::string& name, const Retail::Image& image) {
    std::vector<uint8_t> blp;
    Retail::BuildBlp(image, blp);
    CGxTexFlags flags(GxTex_LinearMipNearest, 0, 0, 0, 0, 0, 1);
    // Texture names hold 260 bytes and a 45-option look key is longer: keep the head and add a hash of the whole name.
    std::string bounded = name;
    if (bounded.size() > 200) {
        char tail[24];
        std::snprintf(tail, sizeof(tail), "#%016llx", static_cast<unsigned long long>(std::hash<std::string>{}(name)));
        bounded.resize(200);
        bounded += tail;
    }
    return TextureCreateFromBlpData(bounded.c_str(), flags, blp.data());
}

// Body texture: the retail skin sheet with the equipped items' section layers pasted over it. The stock item
// section grid (512x512) is the left half of the retail sheet at 2x, in the same section order.
void BindBody(CCharacterComponent* character, CM2Model* model, const Look& look, const std::string& key,
    bool belt = false) {
    const bool clothing = SlimeGiantRace(character->m_data.m_preferences.m_raceID) ||
        CentaurRace(character->m_data.m_preferences.m_raceID);
    const Retail::ComposedTexture* body = nullptr;
    for (const auto& texture : look.composed)
        if (texture.textureType == (clothing ? 14u : 1u)) body = &texture;
    if (!body) { Log("no body sheet for %s", key.c_str()); return; }

    std::string itemKey;
    // Dragon form wears its armor as customization choices, not as the equipped items: those layers do not fit its sheet.
    auto layers = Retail::CollectEquipmentLayers(character->m_section, !belt && (character->m_flags & 0x20) != 0);
    if (RetailRaceNaked(character)) layers.clear();
    const uint32_t boots = character->m_itemDisplayID[ITEMSLOT_BOOTS];
    const uint32_t beltDisplay = character->m_itemDisplayID[ITEMSLOT_BELT];
    const bool hoofBoots = CentaurRace(character->m_data.m_preferences.m_raceID);
    const bool hasFoot = std::any_of(layers.begin(), layers.end(), [&](const Retail::EquipmentLayer& layer) {
        return layer.displayId == boots && layer.section == 7;
    });
    const bool beltUpper = std::any_of(layers.begin(), layers.end(), [&](const Retail::EquipmentLayer& layer) {
        return Retail::Centaur::BeltLayer(layer.displayId, beltDisplay) && layer.section == 5;
    });
    for (const auto& layer : layers) {
        itemKey += "/" + std::to_string(layer.section) + "." + std::to_string(layer.layer) + "." + std::to_string(layer.displayId);
        // Guild tabard layers all have display ID zero; their filenames identify the emblem/colors.
        if (!layer.displayId && layer.texture) {
            itemKey += ":" + std::string(static_cast<const CACHEENTRY*>(layer.texture)->m_fileName);
        }
    }

    static std::map<std::string, CTexture*> bodies;
    static std::deque<std::string> order;
    const bool hiddenLegArmor = RetailRaceLegArmorHidden(character);
    const std::string name = "Retail\\Race\\" + key + (belt ? "\\belt" : "\\body") + itemKey
        + (hiddenLegArmor ? "/bare-legs" : "");
    auto found = bodies.find(name);
    if (found == bodies.end()) {
        Retail::BusyNotice busy("Preparing character appearance");
        const unsigned long start = GetTickCount();
        Retail::Image sheet = body->image;
        for (const auto& layer : layers) {
            if ((hoofBoots && Retail::Centaur::BeltLayer(layer.displayId, beltDisplay)) != belt) continue;
            int32_t destination = hoofBoots
                ? Retail::Centaur::EquipmentSection(layer.section, layer.displayId, boots, hasFoot)
                : int32_t(layer.section);
            if (belt) destination = layer.section == 5 || (!beltUpper && layer.section == 4) ? 5 : -1;
            if (destination < 0) continue;
            if (hiddenLegArmor && (destination == 5 || destination == 6)) continue;
            void* handle = layer.texture;
            // Only dirty sections reload their item textures. A full retail sheet also needs the
            // unchanged sections, whose temporary handles were released by the previous render.
            const bool reload = !handle;
            if (reload) {
                ItemDisplayInfoRec display;
                if (g_itemDisplayInfoDB.GetRecord(layer.displayId, &display)) {
                    handle = character->LoadItemComponentTexture(&display, static_cast<COMPONENT_SECTIONS>(layer.section));
                }
            }
            TCTEXTUREINFO info{};
            TextureCacheGetInfo(handle, info, 1);
            const CACHEENTRY* entry = static_cast<const CACHEENTRY*>(handle);
            std::vector<uint8_t> bytes;
            if (entry && !entry->m_loadFailed && entry->m_data && info.m_width) {
                const auto* begin = reinterpret_cast<const uint8_t*>(entry->m_data);
                bytes.assign(begin, begin + entry->m_size);
            }
            if (reload) TextureCacheDestroyTexture(handle);
            Retail::Image item;
            std::string error;
            if (!Retail::DecodeBlp(bytes, item, error)) {
                // Keep the previous complete sheet rather than caching an incomplete equipment state.
                Log("equipment decode failed: section %u layer %u display %u: %s", layer.section, layer.layer, layer.displayId, error.c_str());
                return;
            }
            if (belt && !Retail::Centaur::CropBeltRows(item)) return;
            for (uint32_t i = 0, n = app::SectionCount(look.recipe.textureLayoutId); i < n; ++i) {
                app::TextureSection section{};
                if (app::SectionAt(look.recipe.textureLayoutId, i, section) && section.sectionType == uint32_t(destination))
                    Retail::PaintImage(sheet, item, section.x, section.y, section.width, section.height, 2);
            }
        }
        CTexture* texture = MakeTexture(name, sheet);
        Log("body %s: %zu item layers, texture %p, %lu ms, model flags %u", name.c_str(), layers.size(),
            static_cast<void*>(texture), GetTickCount() - start, unsigned(model->m_fFlags));
        if (!texture) return;
        found = bodies.emplace(name, texture).first;
        order.push_back(name);
        // Keep a handful of equipment states; each is a full-size sheet.
        while (order.size() > 8) {
            const auto old = bodies.find(order.front());
            if (old != bodies.end()) { HandleClose(reinterpret_cast<HOBJECT>(old->second)); bodies.erase(old); }
            order.pop_front();
        }
    }
    model->ReplaceTexture(belt ? 15 : clothing ? 14 : 1, found->second);
    BindCollectionTextures(model);
    if (hoofBoots && !belt) BindBody(character, model, look, key, true);
}

void FitLegacyHelmet(CCharacterComponent* character) {
    const auto& prefs = character->m_data.m_preferences;
    if (prefs.m_sexID > 1
        || !character->m_itemDisplayID[ITEMSLOT_HEAD] || !RetailRaceActive(character)) return;
    CM2Model* parent = character->m_data.m_model;
    if (!(parent->m_fFlags & 1)) return; // Tick retries once the asynchronous parent load completes.
    CM2Model* helmet = parent->m_attachList;
    while (helmet && helmet->m_attachmentId != ATTACH_HELM) helmet = helmet->m_attachNext;
    if (!helmet || IsRetailModel(helmet)) return; // This correction is for installed legacy helmets.

    const M2Data* data = parent->m_shared->m_data;
    if (data->m_boneIndicesById.m_count <= 6 || data->m_attachmentIndicesById.m_count <= ATTACH_HELM) return;
    const uint16_t head = data->m_boneIndicesById[6];
    const uint16_t socket = data->m_attachmentIndicesById[ATTACH_HELM];
    if (head >= data->m_bones.m_count || socket >= data->m_attachments.m_count) return;
    const auto& attachment = data->m_attachments[socket];
    if (attachment.m_bone >= data->m_bones.m_count) return;

    const auto* race = g_chrRacesDB.GetRecord(prefs.m_raceID);
    const auto* display = race ? g_creatureDisplayInfoDB.GetRecord(prefs.m_sexID ? race->m_femaleDisplayID : race->m_maleDisplayID) : nullptr;
    const auto* legacy = display ? g_creatureModelDataDB.GetRecord(display->m_modelID) : nullptr;
    if (!legacy || !legacy->m_modelName || !*legacy->m_modelName) return;
    std::string donorName = legacy->m_modelName;
    if (CentaurRace(prefs.m_raceID))
        donorName = prefs.m_sexID ? "Character\\Draenei\\Female\\DraeneiFemale.m2"
            : "Character\\Draenei\\Male\\DraeneiMale.m2";
    // Stock helmet meshes retain the original race's crown convention even when the effective
    // player display points at its HD "Race2" body. Keep the accepted stock donor paths.
    const char* stockNames[] = {nullptr,"Human","Orc","Dwarf","NightElf","Scourge","Tauren","Gnome","Troll",nullptr,"BloodElf","Draenei"};
    if (prefs.m_raceID < 12 && stockNames[prefs.m_raceID]) {
        const std::string family = stockNames[prefs.m_raceID];
        const std::string sex = prefs.m_sexID ? "Female" : "Male";
        donorName = "Character\\" + family + "\\" + sex + "\\" + family + sex + ".m2";
    }
    const auto dot = donorName.find_last_of('.');
    if (dot != std::string::npos) donorName.replace(dot, std::string::npos, ".m2");
    const char* donorPath = donorName.c_str();
    struct Donor { bool attempted = false, valid = false; Retail::HelmetAnchor anchor; };
    static std::map<std::string, Donor> donors;
    Donor& donor = donors[donorPath];
    if (!donor.attempted) {
        donor.attempted = true;
        void* bytes = nullptr;
        uint32_t size = 0;
        std::string error;
        if (SFile::LoadFile(donorPath, &bytes, &size, 0, nullptr)) {
            const auto* begin = static_cast<const uint8_t*>(bytes);
            donor.valid = Retail::ReadHelmetAnchor(std::vector<uint8_t>(begin, begin + size), donor.anchor, error);
            SFile::Unload(bytes);
        } else error = "could not read the installed helmet donor model";
        if (!donor.valid) Log("helmet donor %s: %s", donorPath, error.c_str());
    }
    if (!donor.valid) return;

    const auto& headPivot = data->m_bones[head].m_pivot;
    Retail::HelmetAnchor target{{headPivot.x, headPivot.y, headPivot.z},
        {attachment.m_position.x, attachment.m_position.y, attachment.m_position.z}};
    C3Vector crown(target.head[0] + donor.anchor.socket[0] - donor.anchor.head[0],
        target.head[1] + donor.anchor.socket[1] - donor.anchor.head[1],
        target.head[2] + donor.anchor.socket[2] - donor.anchor.head[2]);
    // Legacy helmets use the donor crown convention. Bind this instance directly to the animated
    // retail head, avoiding extra scale/rotation on authored Retail-helmet helper bones. For an
    // identity helper this is exactly the accepted Human/Dwarf socket translation, in head space.
    float scale = 1.0f;
    if (CentaurRace(prefs.m_raceID)) {
        crown.x += .045f;
        crown.z += .12f;
        scale = prefs.m_sexID ? 1.22f : 1.30f;
    }
    if (!helmet->SetAttachmentFrame(head, crown, scale)) return;
    Log("helmet fit race %u sex %u display %u model %s donor %s head frame %.6f %.6f %.6f",
        prefs.m_raceID, prefs.m_sexID, character->m_itemDisplayID[ITEMSLOT_HEAD], helmet->m_shared->m_fileName,
        donorPath, crown.x, crown.y, crown.z);
}

void FitCentaurShoulders(CCharacterComponent* character) {
    if (!CentaurRace(character->m_data.m_preferences.m_raceID)) return;
    CM2Model* parent = character->m_data.m_model;
    if (!parent || !(parent->m_fFlags & 1)) return;
    const auto* data = parent->m_shared->m_data;
    if (data->m_boneIndicesById.m_count < 2) return;
    for (CM2Model* child = parent->m_attachList; child; child = child->m_attachNext) {
        if (child->m_attachmentId != 5 && child->m_attachmentId != 6) continue;
        const uint16_t arm = data->m_boneIndicesById[child->m_attachmentId == 5 ? 1 : 0];
        if (arm >= data->m_bones.m_count) continue;
        const uint16_t bone = data->m_bones[arm].m_parentIndex;
        if (bone >= data->m_bones.m_count) continue;
        C3Vector position = data->m_bones[arm].m_pivot;
        position.y += child->m_attachmentId == 5 ? -.11f : .11f;
        position.z += .12f;
        child->SetAttachmentFrame(bone, position, 1.20f);
    }
}

}

bool RetailRaceMapped(uint32_t race) {
    return RetailRaceId(race) != 0;
}

float RetailRacePreviewScale(uint32_t race) {
    return CentaurRace(race) ? .75f : 1.0f;
}

void RetailRaceDuplicateCollections(CM2Model* source, CM2Model* target) {
    if (!source || !target || source == target) return;
    target->WaitForLoad("Retail character preview");
    for (CM2Model* original = source->m_attachList; original; original = original->m_attachNext) {
        for (CM2Model* copy = target->m_attachList; copy; copy = copy->m_attachNext) {
            if (copy->m_attachmentId != original->m_attachmentId || copy->m_shared != original->m_shared) continue;
            copy->WaitForLoad("Retail clothing preview");
            if (original->m_attachmentId == CollectionSocket && original->HasSkinParent(source)) {
                if (!copy->SetSkinParent(target)) Log("preview collection bind failed: %s", copy->m_shared->m_fileName);
            } else copy->CopyAttachmentFrameFrom(original);
        }
    }
}

uint32_t RetailRaceHelmetMask(uint32_t race) {
    const uint32_t retail = RetailRaceId(race);
    const uint32_t donor = retail ? Retail::HelmetMaskRace(retail) : race;
    return donor && donor < 32 ? uint32_t{1} << donor : 0;
}

uint32_t RetailRaceHelmetRace(uint32_t race) {
    const uint32_t retail = RetailRaceId(race);
    const uint32_t donor = retail ? Retail::HelmetMaskRace(retail) : 0;
    return donor ? donor : race;
}

bool RetailRaceActive(CCharacterComponent* character) {
    return character && RetailRaceId(character->m_data.m_preferences.m_raceID) && IsRetailModel(character->m_data.m_model);
}

bool RetailRaceOwns(uint32_t race, uint32_t sex) {
    Retail::SourceScope sourceScope(RaceSource(race));
    const uint32_t retail = RetailRaceId(race);
    return retail && app::ChrModelForRace(retail, sex);
}

// Called before the creator frees its component: keeps a Two Forms race's selections for the rebuilt one.
void RetailRaceFormStore(CCharacterComponent* character) {
    if (!character) return;
    const CharacterPreferences& prefs = character->m_data.m_preferences;
    Retail::SourceScope sourceScope(RaceSource(prefs.m_raceID));
    if (!FormsRace(RetailRaceId(prefs.m_raceID))) return;
    FormMemory& memory = g_formMemory[FormMemoryKey(prefs.m_raceID, prefs.m_sexID)];
    memory.form = ActiveForm(character);
    const auto state = g_state.find(character);
    memory.state = state == g_state.end() ? std::vector<uint32_t>() : state->second;
    memory.prefs[0] = prefs.m_skinID; memory.prefs[1] = prefs.m_faceId; memory.prefs[2] = prefs.m_hairStyleId;
    memory.prefs[3] = prefs.m_hairColorId; memory.prefs[4] = prefs.m_facialHairId;
    const auto slots = g_formSlots.find(character);
    memory.slots = slots == g_formSlots.end() ? std::map<uint32_t, FormSlot>() : slots->second;
}

// Called once a new creator component exists, before its model is made: gives it the remembered selections of that race
// and sex (stock selectors go into `prefs`) and returns the form to build its model for (0 = the race's default).
uint32_t RetailRaceFormRestore(CCharacterComponent* character, CharacterPreferences& prefs) {
    const auto found = g_formMemory.find(FormMemoryKey(prefs.m_raceID, prefs.m_sexID));
    if (!character || found == g_formMemory.end()) return 0;
    const FormMemory& memory = found->second;
    g_stateIdentity[character] = (uint64_t(prefs.m_raceID) << 32) | prefs.m_sexID;
    g_state[character] = memory.state;
    g_formSlots[character] = memory.slots;
    if (memory.form) g_form[character] = memory.form;
    prefs.m_skinID = memory.prefs[0]; prefs.m_faceId = memory.prefs[1]; prefs.m_hairStyleId = memory.prefs[2];
    prefs.m_hairColorId = memory.prefs[3]; prefs.m_facialHairId = memory.prefs[4];
    return memory.form;
}

void RetailRaceFormClearMemory() { g_formMemory.clear(); }

// ---- Two Forms commit ---------------------------------------------------------------------------------------------
// After a Worgen or Dracthyr is created the client commits both forms with CMSG_ESTERIA_FORM_LOOK (docs/two-forms-server.md).
// The new character's GUID is not in SMSG_CHAR_CREATE, so the commit waits for the next character list and finds the
// character by name, race and sex. The server answers with SMSG_ESTERIA_FORM_LOOK_ACK; without one the commit is resent
// a few times and then dropped (the character keeps the active form's look and defaults for the other).

namespace {

constexpr uint32_t kFormLookRequest = 0x525;
constexpr uint32_t kCommitResendMs = 3000, kCommitAttempts = 3;

struct PendingCommit {
    enum State { None, AwaitCreate, AwaitList, Sent } state = None;
    std::string name;
    uint32_t race = 0, sex = 0, revision = 0, attempts = 0, lastSend = 0;
    uint8_t activeForm = 0;
    uint64_t guid = 0;
    std::array<uint8_t, CharacterRetailLook::PayloadSize> active{}, parked{};
};
PendingCommit g_commit;

uint32_t SlotPref(const FormSlot& slot, Stock stock) {
    switch (stock) {
        case Stock::Skin: return slot.prefs[0];
        case Stock::Face: return slot.prefs[1];
        case Stock::HairStyle: return slot.prefs[2];
        case Stock::HairColor: return slot.prefs[3];
        case Stock::Facial: return slot.prefs[4];
        default: return 0;
    }
}

// One form's look as the creator holds it. The form on show is read from the component; the other from its parked slot.
bool BuildFormLook(CCharacterComponent* character, uint32_t retail, uint32_t form, const FormSlot* parked,
    CharacterRetailLook::Look& look) {
    const CharacterPreferences& prefs = character->m_data.m_preferences;
    const std::vector<OptionInfo>& options = OptionsFor(retail, prefs.m_sexID, form);
    if (options.empty() || options.size() > CharacterRetailLook::MaxOptions) return false;
    look = {};
    look.race = uint8_t(prefs.m_raceID);
    look.sex = uint8_t(prefs.m_sexID);
    look.form = uint8_t(form);
    look.count = uint8_t(options.size());
    for (size_t i = 0; i < options.size(); ++i) {
        uint32_t index = 0;
        if (!parked) index = IndexOf(character, options, i);
        else if (options[i].count) {
            if (options[i].stock != Stock::None) index = SlotPref(*parked, options[i].stock);
            else if (i < parked->state.size() && parked->state[i] != UINT32_MAX) index = parked->state[i];
            index %= options[i].count;
        }
        const auto& eligible = options[i].creationIndices;
        if (!eligible.empty() && std::find(eligible.begin(), eligible.end(), index) == eligible.end()) index = eligible.front();
        look.index[i] = uint16_t(std::min<uint32_t>(index, 65535));
    }
    return true;
}

void SendCommit() {
    // Plain bytes (little endian); SendClientMessage puts the opcode in front. Its CDataStore overload crashed on this size.
    std::vector<uint8_t> payload;
    const auto put = [&](uint64_t value, unsigned bytes) { for (unsigned i = 0; i < bytes; ++i) payload.push_back(uint8_t(value >> (8 * i))); };
    put(g_commit.guid, 8);
    put(g_commit.revision, 4);
    put(g_commit.activeForm, 1);
    payload.insert(payload.end(), g_commit.active.begin(), g_commit.active.end());
    payload.insert(payload.end(), g_commit.parked.begin(), g_commit.parked.end());
    // Custom: sent directly on the realm connection (Kalibro's ExtensionMgr::SendClientMessage)
    if (!ClientServices::s_currentConnection) return;
    CDataStore msg;
    msg.Put(kFormLookRequest);
    msg.PutData(payload.data(), uint32_t(payload.size()));
    msg.Finalize();
    ClientServices::s_currentConnection->Send(&msg);
    g_commit.lastSend = GetTickCount();
    ++g_commit.attempts;
    Log("form commit sent: guid %llx revision %u attempt %u", static_cast<unsigned long long>(g_commit.guid), g_commit.revision, g_commit.attempts);
}

}  // namespace

// Called as the creator submits a character: keeps both forms' looks for the commit. Nothing is kept for other races.
void RetailFormCommitPrepare(const char* name, uint32_t race, uint32_t sex) {
    g_commit = PendingCommit();
    CCharacterComponent* character = CCharacterCreation::m_character;
    if (!character || !name || !*name) return;
    const CharacterPreferences& prefs = character->m_data.m_preferences;
    if (prefs.m_raceID != race || prefs.m_sexID != sex) return;
    Retail::SourceScope sourceScope(RaceSource(race));
    const uint32_t retail = RetailRaceId(race);
    if (!FormsRace(retail)) return;

    const uint32_t active = ActiveForm(character) ? ActiveForm(character) : DefaultForm(retail);
    const uint32_t other = active == FormByIndex(retail, 0) ? FormByIndex(retail, 1) : FormByIndex(retail, 0);
    FormSlot parked;
    const auto slots = g_formSlots.find(character);
    if (slots != g_formSlots.end()) {
        const auto found = slots->second.find(other);
        if (found != slots->second.end()) parked = found->second;
    }
    CharacterRetailLook::Look activeLook, parkedLook;
    if (!BuildFormLook(character, retail, active, nullptr, activeLook) || !BuildFormLook(character, retail, other, &parked, parkedLook)) return;
    CharacterRetailLook::WritePayload(g_commit.active.data(), activeLook);
    CharacterRetailLook::WritePayload(g_commit.parked.data(), parkedLook);
    g_commit.name = name;
    g_commit.race = race;
    g_commit.sex = sex;
    g_commit.activeForm = uint8_t(active);
    g_commit.revision = GetTickCount();
    g_commit.state = PendingCommit::AwaitCreate;
}

void RetailFormCommitCreateResult(bool success) {
    if (g_commit.state != PendingCommit::AwaitCreate) return;
    g_commit.state = success ? PendingCommit::AwaitList : PendingCommit::None;
}

// Called for every character of a received list. Returns true when this one is the pending commit's character.
bool RetailFormCommitOffer(const char* name, uint32_t race, uint32_t sex, uint64_t guid) {
    if (g_commit.state != PendingCommit::AwaitList || !name || _stricmp(name, g_commit.name.c_str()) ||
        race != g_commit.race || sex != g_commit.sex) return false;
    // Custom: a server without the retail look opcodes disconnects on CMSG_RETAIL_FORM_LOOK
    if (!RetailRaceServerLooks()) {
        Log("form commit dropped: the server sent no RLE1 block, so it has no retail look support");
        g_commit = PendingCommit();
        return true;
    }
    g_commit.guid = guid;
    g_commit.state = PendingCommit::Sent;
    g_commit.attempts = 0;
    SendCommit();
    return true;
}

// The list that follows a create names the character, or the commit is dropped.
void RetailFormCommitListDone() {
    if (g_commit.state == PendingCommit::AwaitList) {
        Log("form commit dropped: %s not in the character list", g_commit.name.c_str());
        g_commit = PendingCommit();
    }
}

void RetailFormCommitAck(uint64_t guid, uint32_t revision, uint32_t result) {
    if (g_commit.state != PendingCommit::Sent || guid != g_commit.guid || revision != g_commit.revision) return;
    Log("form commit acknowledged: guid %llx result %u", static_cast<unsigned long long>(guid), result);
    g_commit = PendingCommit();
}

// Resends an unanswered commit; called every frame on the character screen.
void RetailFormCommitTick() {
    if (g_commit.state != PendingCommit::Sent || GetTickCount() - g_commit.lastSend < kCommitResendMs) return;
    if (g_commit.attempts >= kCommitAttempts) {
        Log("form commit dropped: no acknowledgement for guid %llx", static_cast<unsigned long long>(g_commit.guid));
        g_commit = PendingCommit();
        return;
    }
    SendCommit();
}


// Dracthyr Dragon is shown bare in the creator: the stock starting outfit's item layers do not fit its sheet.
bool RetailRaceNaked(CCharacterComponent* character) {
    if (!character) return false;
    Retail::SourceScope sourceScope(RaceSource(character->m_data.m_preferences.m_raceID));
    const uint32_t retail = RetailRaceId(character->m_data.m_preferences.m_raceID);
    return FormsRace(retail) && EffectiveForm(character, retail) == app::FormDragon;
}

bool RetailRaceFormDual(CCharacterComponent* main) {
    if (!g_dual || !main) return false;
    Retail::SourceScope sourceScope(RaceSource(main->m_data.m_preferences.m_raceID));
    return FormsRace(RetailRaceId(main->m_data.m_preferences.m_raceID));
}

// Offset of a creator preview along the screen's horizontal axis while both forms show. WOW_FORM_DUAL_SPACING and
// WOW_FORM_DUAL_AXIS ("x" or "y") tune it without a rebuild.
bool RetailRaceFormOffset(CCharacterComponent* character, float& x, float& y) {
    x = y = 0.0f;
    if (!RetailRaceFormDual(character)) return false;
    Retail::SourceScope sourceScope(RaceSource(character->m_data.m_preferences.m_raceID));
    const uint32_t retail = RetailRaceId(character->m_data.m_preferences.m_raceID);
    const uint32_t form = ActiveForm(character) ? ActiveForm(character) : DefaultForm(retail);
    const char* spacing = std::getenv("WOW_FORM_DUAL_SPACING");
    const float distance = spacing ? float(std::atof(spacing)) : 0.9f;
    const char* axis = std::getenv("WOW_FORM_DUAL_AXIS");
    float& along = axis && (axis[0] == 'x' || axis[0] == 'X') ? x : y;
    along = form == FormByIndex(retail, 1) ? distance : -distance;
    return true;
}

// Fills `prefs` and the new mirror component's form state from the form `main` is not showing, and returns that form.
uint32_t RetailRaceMirrorSetup(CCharacterComponent* mirror, CCharacterComponent* main, CharacterPreferences& prefs) {
    if (!RetailRaceFormDual(main)) return 0;
    Retail::SourceScope sourceScope(RaceSource(main->m_data.m_preferences.m_raceID));
    const uint32_t retail = RetailRaceId(main->m_data.m_preferences.m_raceID);
    const uint32_t current = ActiveForm(main) ? ActiveForm(main) : DefaultForm(retail);
    const uint32_t other = current == FormByIndex(retail, 0) ? FormByIndex(retail, 1) : FormByIndex(retail, 0);
    FormSlot slot;
    const auto slots = g_formSlots.find(main);
    if (slots != g_formSlots.end()) {
        const auto found = slots->second.find(other);
        if (found != slots->second.end()) slot = found->second;
    }
    prefs.m_skinID = slot.prefs[0]; prefs.m_faceId = slot.prefs[1]; prefs.m_hairStyleId = slot.prefs[2];
    prefs.m_hairColorId = slot.prefs[3]; prefs.m_facialHairId = slot.prefs[4];
    g_stateIdentity[mirror] = (uint64_t(main->m_data.m_preferences.m_raceID) << 32) | main->m_data.m_preferences.m_sexID;
    g_state[mirror] = slot.state;
    g_form[mirror] = other;
    return other;
}

bool RetailRaceModelPath(uint32_t race, uint32_t sex, char* path, size_t size, uint32_t form) {
    Retail::SourceScope sourceScope(RaceSource(race));
    const uint32_t retail = RetailRaceId(race);
    if (!retail) return false;
    Retail::BusyNotice busy("Loading character data");
    app::Recipe recipe;
    const bool ok = app::BuildForCharacter(retail, sex, nullptr, 0, recipe, form) && recipe.modelFileDataId;
    Log("model path race %u sex %u -> %s (%u)", race, sex, ok ? "ok" : "unavailable", recipe.modelFileDataId);
    if (!ok) return false;
    Retail::Source().RegisterCharacterModel(recipe.modelFileDataId);
    const auto name = retail == Retail::SlimeGiant::Profile ? Retail::SlimeGiant::Path(recipe.modelFileDataId)
        : retail == Retail::Centaur::Profile ? Retail::Centaur::Path(recipe.modelFileDataId)
        : Retail::FileName(recipe.modelFileDataId, ".m2");
    std::snprintf(path, size, "%s", name.c_str());
    return true;
}

bool RetailRaceBody(CCharacterComponent* character) {
    Retail::SourceScope sourceScope(RaceSource(character->m_data.m_preferences.m_raceID));
    CharacterPreferences& prefs = character->m_data.m_preferences;
    CM2Model* model = character->m_data.m_model;
    const uint32_t retail = RetailRaceId(prefs.m_raceID);
    if (!retail || !IsRetailModel(model)) return false;
    const std::vector<uint32_t> choices = Choices(retail, character);
    std::string key;
    Look& look = LookFor(retail, prefs.m_sexID, choices, key, ActiveForm(character));
    Log("RetailRaceBody component %p key %s", static_cast<void*>(character), key.c_str());
    BindBody(character, model, look, key);
    return true;
}

bool RetailRaceGeometry(CCharacterComponent* character) {
    Retail::SourceScope sourceScope(RaceSource(character->m_data.m_preferences.m_raceID));
    CharacterPreferences& prefs = character->m_data.m_preferences;
    CM2Model* model = character->m_data.m_model;
    const uint32_t retail = RetailRaceId(prefs.m_raceID);
    if (!retail || !IsRetailModel(model)) return false;

    const std::vector<uint32_t> choices = Choices(retail, character);
    std::string key;
    Look& look = LookFor(retail, prefs.m_sexID, choices, key, ActiveForm(character));
    if (!look.recipe.modelFileDataId) return false;
    Log("RetailRaceGeometry component %p key %s", static_cast<void*>(character), key.c_str());
    g_active.insert(character);
    SyncCollections(character, look.recipe);

    // Stock only hides geosets up to id 2000. Retail models carry more groups above it (feet 20xx, torso 22xx,
    // customization 3xxx...), all of which would otherwise draw at once. Show just the defaults here; the
    // recipe and equipment add the rest.
    model->SetGeometryVisible(2001, 9999, 0);
    // Restore anatomy that no customization choice selects, including Dwarves' full head (32).
    for (const uint16_t base : Retail::DefaultRaceGeosets(look.recipe.geosets))
        model->SetGeometryVisible(base, base, 1);
    // No Dracthyr Dragon choice selects its wing membranes (group 46, the wing texture sheet), so they are always shown.
    if (FormsRace(retail) && EffectiveForm(character, retail) == app::FormDragon) {
        bool chosen = false;
        for (const uint16_t geoset : look.recipe.geosets) chosen = chosen || geoset / 100 == 46;
        if (!chosen) model->SetGeometryVisible(4601, 4601, 1);
    }

    // Stock geoset slots are indexed by group; recipe geosets replace the Wrath hair/beard defaults.
    for (const uint16_t geoset : look.recipe.geosets) {
        model->SetGeometryVisible(geoset, geoset, 1);
        if (geoset / 100 < 19) character->m_data.m_geosets[geoset / 100] = geoset;
    }
    if (ElfEarGeoset(character, retail)) {
        // The collection's ear root seals the head opening itself; no Human ear geometry may show.
        model->SetGeometryVisible(700, 799, 0);
        character->m_data.m_geosets[7] = 700;
    }

    // Hair, eyes and other single-layer types are cached per look; the body also carries equipment.
    for (const auto& texture : look.composed) {
        if (texture.textureType == 1 || ((retail == Retail::SlimeGiant::Profile ||
            retail == Retail::Centaur::Profile) && texture.textureType == 14)) continue;
        CTexture*& bound = look.textures[texture.textureType];
        if (!bound) bound = MakeTexture("Retail\\Race\\" + key + "\\" + std::to_string(texture.textureType), texture.image);
        if (bound) {
            model->ReplaceTexture(texture.textureType, bound);
            // Some collection-only targets do not appear in the body's texture table, so a
            // later copy of the body's slots cannot bind those jewelry/fur/hair materials.
            for (CM2Model* child = model->m_attachList; child; child = child->m_attachNext)
                if (child->m_attachmentId == CollectionSocket) child->ReplaceTexture(texture.textureType, bound);
        }
    }
    BindCollectionTextures(model);
    // The body is composed after item loading/section prep. Geometry-only refreshes happen after
    // temporary equipment textures are freed and must never replace the equipped sheet with bare skin.
    return true;
}

void RetailRaceEquipment(CCharacterComponent* character) {
    Retail::SourceScope sourceScope(RaceSource(character->m_data.m_preferences.m_raceID));
    CM2Model* model = character->m_data.m_model;
    if (!RetailRaceActive(character)) return;
    Retail::ApplyRaceEyeGeometry(*model, RetailRaceEyeLightning(character));
    FitLegacyHelmet(character);
    FitCentaurShoulders(character);
    if (SlimeGiantRace(character->m_data.m_preferences.m_raceID) ||
        CentaurRace(character->m_data.m_preferences.m_raceID)) FinishCollections(character, true);

    // Bare feet and closed shoes are separate Retail meshes. Wrath boot style zero is still a
    // shoe; shin style and robe coverage must not select bare toes under its painted texture.
    bool hasClosedFoot = true;
    if ((model->m_fFlags & 1) && model->m_shared->m_skinData) {
        hasClosedFoot = false;
        const auto& sections = model->m_shared->m_skinData->m_skinSections;
        for (uint32_t i = 0; i < sections.m_count; ++i) {
            hasClosedFoot = hasClosedFoot || (sections[i].m_skinSectionId & 0xFFFF) == 2002;
        }
    }
    if (Retail::UsesLegacyClosedFeet(RetailRaceId(character->m_data.m_preferences.m_raceID)))
        Retail::ApplyLegacyFootGeometry(*model, character->m_itemDisplayID[ITEMSLOT_BOOTS], hasClosedFoot);

    // The Gilnean body has ten shin variants (501-510) that the Wrath boot styles do not index; the style that is picked
    // runs up to the knee. Keep the plain shin and let the shoe mesh and the boot texture show the boots.
    {
        const uint32_t retail = RetailRaceId(character->m_data.m_preferences.m_raceID);
        if (retail == 22 && EffectiveForm(character, retail) == app::FormHuman) {
            model->SetGeometryVisible(501, 599, 0);
            model->SetGeometryVisible(501, 501, 1);
            character->m_data.m_geosets[5] = 501;
            Retail::ApplyLegacyFootGeometry(*model, character->m_itemDisplayID[ITEMSLOT_BOOTS], hasClosedFoot);
        }
    }

    // AddHelm sets stock selector values before the recipe runs. Apply the effective target's
    // helmet hides last; otherwise retail hair and facial hair reappear underneath the helm.
    const uint32_t race = character->m_data.m_preferences.m_raceID;
    const uint32_t sex = character->m_data.m_preferences.m_sexID;
    constexpr uint32_t groups[] = {0, 1, 2, 3, 7, 16, 17};
    for (uint32_t i = 0; i < 7; ++i) {
        if (!HelmetHides(character, i)) continue;
        const uint32_t group = groups[i];
        if (CentaurRace(race)) {
            if (group == 0) model->SetGeometryVisible(7001, 7099, 0);
            if ((group == 1 || group == 2) && sex == 0) model->SetGeometryVisible(6101, 6199, 0);
            continue; // Female 6101 is required arm anatomy, never a beard hide.
        }
        // Geoset zero contains unconditional anatomy, while 1..99 are hairstyles.
        model->SetGeometryVisible(group ? group * 100 : 1, group * 100 + 99, 0);
    }
}

void RetailRaceForget(CCharacterComponent* character) {
    g_state.erase(character);
    g_form.erase(character);
    g_formSlots.erase(character);
    g_stateIdentity.erase(character);
    g_active.erase(character);
    g_collections.erase(character);
    g_owners.erase(character);
    g_eyeLightning.erase(character);
    g_hiddenLegArmor.erase(character);
    g_lookApplied.erase(character);
}

bool RetailRaceLegArmorHidden(CCharacterComponent* character) {
    if (!character || !CentaurRace(character->m_data.m_preferences.m_raceID)) return false;
    const auto owner = g_owners.find(character);
    if (owner == g_owners.end() || !owner->second || !owner->second->m_obj ||
        !(owner->second->m_obj->m_type & TYPEMASK_PLAYER)) return false;
    const auto* player = static_cast<CGPlayer_C*>(owner->second);
    return (player->m_player->m_playerFlags & CentaurAppearance::HideLegArmorFlag) != 0; // Custom: race checked above
}

bool RetailRaceEyeLightning(CCharacterComponent* character) {
    if (!character) return false;
    CheckStateIdentity(character);
    const auto owner = g_owners.find(character);
    if (owner != g_owners.end() && owner->second && owner->second->m_obj)
        return (owner->second->m_obj->m_pad & CharacterEyeEffects::LightningWordMask) != 0;
    const auto found = g_eyeLightning.find(character);
    return found != g_eyeLightning.end() && found->second;
}

void RetailRaceEyeLightning(CCharacterComponent* character, bool enabled) {
    if (!character) return;
    CheckStateIdentity(character);
    g_eyeLightning[character] = enabled;
    character->m_flags |= 0x4;
}

void RetailRaceCreateEyeEffects(CDataStore* packet) {
    auto* character = CCharacterCreation::m_character;
    if (!packet || !character || !RetailRaceMapped(character->m_data.m_preferences.m_raceID)) return;
    for (uint8_t byte : CharacterEyeEffects::Trailer(RetailRaceEyeLightning(character))) packet->Put(byte);
}

void BodyShapeCreateFooter(CDataStore* packet, bool always) {
    auto* character = CCharacterCreation::m_character;
    if (!packet || !character || !RetailRaceMapped(character->m_data.m_preferences.m_raceID)) return;
    CharacterBodyShape::Values wire = BodyShape::ToWire(BodyShape::Local());
    if (!always && CharacterBodyShape::AllDefault(wire)) return;
    uint8_t payload[CharacterBodyShape::PayloadSize];
    CharacterBodyShape::WritePayload(payload, wire);
    for (uint8_t byte : payload) packet->Put(byte);
    for (uint32_t shift = 0; shift < 32; shift += 8) packet->Put(static_cast<uint8_t>(CharacterBodyShape::FooterMagic >> shift));
}

void RetailRaceBindOwner(CCharacterComponent* character, CGUnit_C* owner) {
    if (!character) return;
    if (owner && owner->m_obj) {
        char line[240];
        std::snprintf(line, sizeof(line), "bind: component=%p guid=%llx race=%u sex=%u retailModel=%d look=%d",
            static_cast<void*>(character), static_cast<unsigned long long>(owner->m_obj->m_guid.m_guid),
            unsigned(character->m_data.m_preferences.m_raceID), unsigned(character->m_data.m_preferences.m_sexID),
            int(IsRetailModel(character->m_data.m_model)), int(g_looks.count(owner->m_obj->m_guid.m_guid)));
        BodyShape::Log(line);
    }
    if (owner) g_owners[character] = owner;
    else g_owners.erase(character);
}

namespace {

// Same setter sequence the stock initializer uses, then rebuild geosets and textures.
void Refresh(CCharacterComponent* character) {
    SyncBodyModel(character);
    CharacterPreferences& prefs = character->m_data.m_preferences;
    character->SetHairColor(prefs.m_hairColorId, false, "");
    character->SetSkinColor(prefs.m_skinID, false, true, "");
    character->SetFace(prefs.m_faceId, false, "");
    character->SetHairStyle(prefs.m_hairStyleId, "");
    character->SetBeardStyle(prefs.m_facialHairId, false, "");
    character->m_flags |= 0x4;
    character->m_dirtySections = 0xFFFFFFFF;
    CGlueLoading::StartLoad(character, 1);
}

struct Snapshot {
    CCharacterComponent* owner = nullptr;
    uint32_t prefs[5] = {};
    std::vector<uint32_t> state;
    std::array<uint8_t, 13> fields{};
    uint32_t race = 0, sex = 0, form = 0;
    bool eyeLightning = false;
};

Snapshot g_preview;

}

int RetailRaceCycleCommand(lua_State* L, CCharacterComponent* character, const char* command) {
    if (!character) return -1;
    Retail::SourceScope sourceScope(RaceSource(character->m_data.m_preferences.m_raceID));
    CharacterPreferences& prefs = character->m_data.m_preferences;
    const uint32_t retail = RetailRaceId(prefs.m_raceID);
    const bool owned = retail && app::ChrModelForRace(retail, prefs.m_sexID) && IsRetailModel(character->m_data.m_model);

    if (!std::strcmp(command, "EA_RETAIL")) {
        lua_pushnumber(L, owned ? 1 : 0);
        return 1;
    }
    if (!owned) return -1;

    // Two Forms preview. EA_FORM_COUNT is 0 for races without one; EA_FORM_GET/SET take index 0 (Wolf/Dragon) or 1 (Human/Visage).
    if (!std::strncmp(command, "EA_FORM_", 8)) {
        static const char* const names[] = {"", "Wolf", "Human", "Dragon", "Visage"};
        const uint32_t current = ActiveForm(character) ? ActiveForm(character) : DefaultForm(retail);
        if (!std::strcmp(command, "EA_FORM_COUNT")) { lua_pushnumber(L, FormsRace(retail) ? 2 : 0); return 1; }
        if (!FormsRace(retail)) return -1;
        if (!std::strcmp(command, "EA_FORM_DUAL")) {
            // 0 off, 1 on, anything else queries.
            const double wanted = lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 2;
            if ((wanted == 0 || wanted == 1) && g_dual != (wanted == 1) && character == CCharacterCreation::m_character) {
                g_dual = wanted == 1;
                CCharacterCreation::SyncFormMirror();
                CCharacterCreation::SetCharacterFacing(CCharacterCreation::m_charFacing);
            }
            lua_pushnumber(L, g_dual ? 1 : 0);
            return 1;
        }
        if (!std::strcmp(command, "EA_FORM_GET")) { lua_pushnumber(L, current == FormByIndex(retail, 1) ? 1 : 0); return 1; }
        const double index = lua_isnumber(L, 2) ? lua_tonumber(L, 2) : -1;
        if (index != 0 && index != 1) { if (!std::strcmp(command, "EA_FORM_NAME")) { lua_pushstring(L, ""); return 1; } return 0; }
        if (!std::strcmp(command, "EA_FORM_NAME")) { lua_pushstring(L, names[FormByIndex(retail, uint32_t(index))]); return 1; }
        if (!std::strcmp(command, "EA_FORM_SET")) {
            const uint32_t next = FormByIndex(retail, uint32_t(index));
            if (next != current) {
                const bool creator = character == CCharacterCreation::m_character;
                SwitchForm(character, retail, next);
                if (creator && RetailRaceNaked(character)) CCharacterCreation::Undress(character); // before the body swap carries attachments over
                Refresh(character);
                if (creator && !RetailRaceNaked(character)) {
                    CCharacterCreation::Dress(character);
                    CGlueLoading::StartLoad(character, 1);
                }
                if (creator) {
                    CCharacterCreation::SyncFormMirror();
                    CCharacterCreation::SetCharacterFacing(CCharacterCreation::m_charFacing);
                }
            }
            return 0;
        }
        return -1;
    }

    const std::vector<OptionInfo>& options = OptionsFor(retail, prefs.m_sexID, ActiveForm(character));
    std::vector<size_t> visibleOptions;
    for (size_t i = 0; i < options.size(); ++i)
        if (!options[i].creationIndices.empty()) visibleOptions.push_back(i);
    // A synthetic final option does not enter DB2 choice lists or the saved legacy codec.
    // Models without an authored lightning mesh do not offer an ineffective toggle.
    const auto* body = character->m_data.m_model;
    bool eyeOption = false;
    if ((body->m_fFlags & 1) && body->m_shared->m_skinData) {
        const auto& sections = body->m_shared->m_skinData->m_skinSections;
        for (uint32_t i = 0; i < sections.m_count; ++i)
            eyeOption = eyeOption || (sections[i].m_skinSectionId & 0xFFFF) == 1701;
    }
    const size_t visibleCount = visibleOptions.size() + (eyeOption ? 1 : 0);

    if (!std::strcmp(command, "EA_COUNT")) {
        lua_pushnumber(L, visibleCount);
        return 1;
    }

    if (!std::strcmp(command, "EA_PREVIEW_SAVE") || !std::strcmp(command, "EA_PREVIEW_RESTORE") || !std::strcmp(command, "EA_PREVIEW_END")) {
        if (!std::strcmp(command, "EA_PREVIEW_SAVE")) {
            g_preview.owner = character;
            g_preview.race = prefs.m_raceID; g_preview.sex = prefs.m_sexID; g_preview.form = ActiveForm(character);
            g_preview.fields = EsteriaRetailFields(character);
            g_preview.eyeLightning = RetailRaceEyeLightning(character);
            g_preview.prefs[0] = prefs.m_skinID; g_preview.prefs[1] = prefs.m_faceId; g_preview.prefs[2] = prefs.m_hairStyleId;
            g_preview.prefs[3] = prefs.m_hairColorId; g_preview.prefs[4] = prefs.m_facialHairId;
            const auto found = g_state.find(character);
            g_preview.state = found == g_state.end() ? std::vector<uint32_t>() : found->second;
            lua_pushnumber(L, 1);
        } else if (!std::strcmp(command, "EA_PREVIEW_END")) {
            g_preview.owner = nullptr;
            lua_pushnumber(L, 1);
        } else if (g_preview.owner == character && g_preview.race == prefs.m_raceID && g_preview.sex == prefs.m_sexID) {
            if (ActiveForm(character) != g_preview.form)
                SwitchForm(character, retail, g_preview.form ? g_preview.form : DefaultForm(retail));
            prefs.m_skinID = g_preview.prefs[0]; prefs.m_faceId = g_preview.prefs[1]; prefs.m_hairStyleId = g_preview.prefs[2];
            prefs.m_hairColorId = g_preview.prefs[3]; prefs.m_facialHairId = g_preview.prefs[4];
            g_state[character] = g_preview.state;
            RetailRaceEyeLightning(character, g_preview.eyeLightning);
            if (PackedRace(prefs.m_raceID)) EsteriaRetailFields(character, g_preview.fields);
            Refresh(character);
            lua_pushnumber(L, 1);
        } else {
            lua_pushnumber(L, 0);
        }
        return 1;
    }

    if (!std::strcmp(command, "EA_RANDOM")) {
        static std::mt19937 random(GetTickCount());
        for (const size_t i : visibleOptions) {
            const auto& eligible = options[i].creationIndices;
            if (eligible.size() > 1) StoreIndex(character, options, i,
                eligible[std::uniform_int_distribution<size_t>(0, eligible.size() - 1)(random)]);
        }
        Refresh(character);
        return 0;
    }

    if (!lua_isnumber(L, 2)) return -1;
    const double requested = lua_tonumber(L, 2);
    const bool isGet = !std::strcmp(command, "EA_GET");
    const bool inRange = requested >= 1 && requested <= double(visibleCount) && requested == std::floor(requested);
    if (!inRange) {
        if (isGet) { lua_pushnumber(L, 0); lua_pushnumber(L, 1); lua_pushstring(L, ""); return 3; }
        if (!std::strcmp(command, "EA_CHOICES") || !std::strcmp(command, "EA_CHOICE_NAME")) { lua_pushstring(L, ""); return 1; }
        return 0;
    }
    if (eyeOption && requested == double(visibleCount)) {
        const uint32_t value = RetailRaceEyeLightning(character) ? 1 : 0;
        if (isGet) {
            lua_pushnumber(L, value); lua_pushnumber(L, 2); lua_pushstring(L, "Eye Lightning"); return 3;
        }
        if (!std::strcmp(command, "EA_CHOICES")) { lua_pushstring(L, "0,1,"); return 1; }
        if (!std::strcmp(command, "EA_CHOICE_NAME")) {
            const double which = lua_isnumber(L, 3) ? lua_tonumber(L, 3) : -1;
            lua_pushstring(L, which == 0 ? "Off" : which == 1 ? "On" : ""); return 1;
        }
        if (!std::strcmp(command, "EA_SET") || !std::strcmp(command, "EA_CYCLE")) {
            if (!lua_isnumber(L, 3)) return 0;
            const double argument = lua_tonumber(L, 3);
            if (!std::isfinite(argument)) return 0;
            const bool direct = !std::strcmp(command, "EA_SET");
            if (direct && argument != 0 && argument != 1) return 0;
            RetailRaceEyeLightning(character, direct ? argument == 1 : !value);
            Refresh(character);
        }
        return 0;
    }
    const size_t index = visibleOptions[size_t(requested) - 1];
    const OptionInfo& option = options[index];
    const auto& eligible = option.creationIndices;
    const auto selected = std::find(eligible.begin(), eligible.end(), IndexOf(character, options, index));
    const uint32_t value = selected == eligible.end() ? 0 : uint32_t(selected - eligible.begin());
    const uint32_t count = uint32_t(eligible.size());

    if (isGet) {
        lua_pushnumber(L, value);
        lua_pushnumber(L, count);
        lua_pushstring(L, option.name.c_str());
        return 3;
    }
    if (!std::strcmp(command, "EA_CHOICES")) {
        std::string list;
        for (uint32_t i = 0; i < count; ++i) list += std::to_string(i) + ",";
        lua_pushstring(L, list.c_str());
        return 1;
    }
    if (!std::strcmp(command, "EA_CHOICE_NAME")) {
        app::ChrChoice choice{};
        const double which = lua_isnumber(L, 3) ? lua_tonumber(L, 3) : -1;
        const bool valid = which >= 0 && which < count && which == std::floor(which);
        const uint32_t raw = valid ? eligible[size_t(which)] : 0, authored = option.count - option.extra;
        const std::string name = !valid ? std::string() : raw >= authored ? ElfEarNames[raw - authored]
            : app::ChoiceAt(option.id, raw, choice) ? app::ChoiceName(choice.id) : std::string();
        lua_pushstring(L, name.c_str());
        return 1;
    }
    if (!std::strcmp(command, "EA_SET") || !std::strcmp(command, "EA_CYCLE")) {
        const double argument = lua_tonumber(L, 3);
        if (!std::isfinite(argument) || !count) return 0;
        const bool direct = !std::strcmp(command, "EA_SET");
        if (direct && (argument < 0 || argument >= count || argument != std::floor(argument))) return 0;
        const uint32_t next = direct
            ? uint32_t(argument)
            : uint32_t((int64_t(value) + (argument < 0 ? -1 : 1) + count) % count);
        if (next >= count) return 0;
        StoreIndex(character, options, index, eligible[next]);
        Refresh(character);
        return 0;
    }
    return -1;
}

// ---- saved looks ---------------------------------------------------------------------------------------------

namespace {


bool LookApplicable(CCharacterComponent* character, const CharacterRetailLook::Look& look, uint32_t& retail) {
    if (!character || !character->m_data.m_model) return false;
    const CharacterPreferences& prefs = character->m_data.m_preferences;
    // A look of the default model must not land on a component the creator has switched to another form.
    if (!look.form && ActiveForm(character)) return false;
    if (look.race != prefs.m_raceID || look.sex != prefs.m_sexID) return false;
    if (PackedRace(prefs.m_raceID)) return false;
    retail = RetailRaceId(prefs.m_raceID);
    if (!retail) return false;
    // A Two Forms look names its form's options: only one of the race's two forms is meaningful.
    return !look.form || (FormsRace(retail) && (look.form == FormByIndex(retail, 0) || look.form == FormByIndex(retail, 1)));
}

// The creator's / component's selection: one choice index per option, as the options panel shows it.
bool CaptureLook(CCharacterComponent* character, CharacterRetailLook::Look& look) {
    // shortcut: a non-default form's look is not sent, since the server transport has no form field yet; Phase 3.
    if (!character || ActiveForm(character) || !IsRetailModel(character->m_data.m_model)) return false;
    CharacterPreferences& prefs = character->m_data.m_preferences;
    if (PackedRace(prefs.m_raceID)) return false;
    Retail::SourceScope sourceScope(RaceSource(prefs.m_raceID));
    const uint32_t retail = RetailRaceId(prefs.m_raceID);
    if (!retail || !app::ChrModelForRace(retail, prefs.m_sexID)) return false;
    const std::vector<OptionInfo>& options = OptionsFor(retail, prefs.m_sexID);
    if (options.empty() || options.size() > CharacterRetailLook::MaxOptions) return false;
    look = {};
    look.race = uint8_t(prefs.m_raceID);
    look.sex = uint8_t(prefs.m_sexID);
    look.count = uint8_t(options.size());
    for (size_t i = 0; i < options.size(); ++i) {
        uint32_t index = IndexOf(character, options, i);
        if (character == CCharacterCreation::m_character) {
            const auto& eligible = options[i].creationIndices;
            if (!eligible.empty() && std::find(eligible.begin(), eligible.end(), index) == eligible.end()) index = eligible.front();
        }
        look.index[i] = uint16_t(std::min<uint32_t>(index, 65535));
    }
    return true;
}

bool ApplyLook(CCharacterComponent* character, const CharacterRetailLook::Look& look) {
    uint32_t retail = 0;
    if (!LookApplicable(character, look, retail)) return false;
    CharacterPreferences& prefs = character->m_data.m_preferences;
    Retail::SourceScope sourceScope(RaceSource(prefs.m_raceID));
    CheckStateIdentity(character);
    // The look's form becomes the component's form (the body is swapped to it by SyncBodyModel); the options it indexes
    // are that form's, and what the component held for another form no longer applies.
    if (look.form && ActiveForm(character) != look.form) {
        g_state.erase(character);
        g_form[character] = look.form;
        ResetGeosets(character);
    }
    const std::vector<OptionInfo>& options = OptionsFor(retail, prefs.m_sexID, look.form);
    std::vector<uint32_t>& state = g_state[character];
    state.resize(options.size(), UINT32_MAX);
    for (size_t i = 0; i < options.size() && i < look.count; ++i) {
        if (!options[i].count) continue;
        const uint32_t value = look.index[i] % options[i].count;
        if (options[i].stock != Stock::None) *PreferenceFor(prefs, options[i].stock) = value;
        else state[i] = value;
    }
    return true;
}

}  // namespace

void RetailLookCreateFooter(CDataStore* packet) {
    CharacterRetailLook::Look look;
    if (!packet || !CaptureLook(CCharacterCreation::m_character, look)) return;
    uint8_t payload[CharacterRetailLook::PayloadSize];
    CharacterRetailLook::WritePayload(payload, look);
    for (uint8_t byte : payload) packet->Put(byte);
    for (uint32_t shift = 0; shift < 32; shift += 8) packet->Put(static_cast<uint8_t>(CharacterRetailLook::FooterMagic >> shift));
}

// The form a character's saved look names (an app::Form), 0 when none is known or it is a default-model look.
// A Dracthyr player showing its dragon body: the form its look names, the race's native one when none does.
bool RetailRaceDragon(uint32_t race, uint64_t guid) {
    Retail::SourceScope sourceScope(RaceSource(race));
    const uint32_t retail = RetailRaceId(race);
    if (!FormsRace(retail) || retail == 22) return false;
    const uint32_t form = RetailRaceLookForm(guid);
    return (form ? form : DefaultForm(retail)) == app::FormDragon;
}

uint32_t RetailRaceLookForm(uint64_t guid) {
    const auto found = g_looks.find(guid);
    return found == g_looks.end() ? 0 : found->second.form;
}

void RetailRaceSetLook(uint64_t guid, const CharacterRetailLook::Look& look) {
    g_looks[guid] = look;
    ++g_lookStamp[guid];
}

void RetailRaceClearLooks() {
    g_looks.clear();
    g_parked.clear();
    g_lookStamp.clear();
}

bool RetailRaceApplyLookTo(CCharacterComponent* character, uint64_t guid, bool refresh) {
    const auto found = g_looks.find(guid);
    if (found == g_looks.end() || !ApplyLook(character, found->second)) return false;
    if (refresh) Refresh(character);
    return true;
}

void RetailRaceSetParkedLook(uint64_t guid, const CharacterRetailLook::Look& look) {
    g_parked[guid] = look;
}

bool RetailRaceParkedForm(uint64_t guid, uint32_t& form) {
    const auto found = g_parked.find(guid);
    if (found == g_parked.end()) return false;
    form = found->second.form;
    return true;
}

bool RetailRaceApplyParkedTo(CCharacterComponent* character, uint64_t guid) {
    const auto found = g_parked.find(guid);
    return found != g_parked.end() && ApplyLook(character, found->second);
}

void RetailRaceTick() {
    if (g_active.empty()) return;
    // Prep() queues a component whose sections are dirty (equipment changed, textures lost) and runs the geometry
    // prep when only geosets changed; both end in the retail hooks.
    const bool inWorld = InWorld();
    for (CCharacterComponent* character : std::set<CCharacterComponent*>(g_active)) {
        Retail::SourceScope sourceScope(RaceSource(character->m_data.m_preferences.m_raceID));
        // The extension high word is public unit state. Poll after normal object updates;
        // do not register a padding-field observer in the stock object callback tables.
        const auto owner = g_owners.find(character);
        if (owner != g_owners.end() && owner->second && owner->second->m_obj) {
            if (CentaurRace(character->m_data.m_preferences.m_raceID)) {
                const bool hidden = RetailRaceLegArmorHidden(character);
                const auto previousLegs = g_hiddenLegArmor.find(character);
                if (previousLegs == g_hiddenLegArmor.end() || previousLegs->second != hidden) {
                    g_hiddenLegArmor[character] = hidden;
                    character->m_flags |= 0x4;
                    character->m_dirtySections = UINT32_MAX;
                }
            }
            const bool enabled = RetailRaceEyeLightning(character);
            auto previous = g_eyeLightning.find(character);
            if (previous == g_eyeLightning.end() || previous->second != enabled) {
                g_eyeLightning[character] = enabled;
                character->m_flags |= 0x4;
            }
        }
        bool lookChanged = false;
        if (owner != g_owners.end() && owner->second && owner->second->m_obj) {
            const uint64_t guid = owner->second->m_obj->m_guid.m_guid;
            const auto look = g_looks.find(guid);
            if (look != g_looks.end()) {
                const uint32_t stamp = g_lookStamp[guid];
                uint32_t& applied = g_lookApplied[character];
                if (applied != stamp) {
                    applied = stamp;
                    lookChanged = ApplyLook(character, look->second);
                    char line[160];
                    std::snprintf(line, sizeof(line), "look applied: component=%p guid=%llx result=%d", static_cast<void*>(character),
                        static_cast<unsigned long long>(guid), int(lookChanged));
                    BodyShape::Log(line);
                    if (lookChanged) character->m_flags |= 0x4;
                }
            }
        }
        if (SyncBodyModel(character) || lookChanged) Refresh(character);
        FitLegacyHelmet(character);
        FitCentaurShoulders(character);
        FinishCollections(character);
        if (inWorld) character->Prep();
    }
}

// Counts the stock selectors report for a Retail race: the size of the mapped Retail option.
int32_t RetailRaceSectionCount(int32_t race, int32_t sex, int32_t variation, bool colors) {
    Retail::SourceScope sourceScope(RaceSource(uint32_t(race)));
    if (CentaurRace(uint32_t(race)) && sex >= 0 && sex < 2 && variation == VARIATION_HAIR && !colors)
        return int32_t(Retail::Centaur::VisibleHeads(uint32_t(sex)));
    if (InWorld()) return -1;
    const uint32_t retail = RetailRaceId(uint32_t(race));
    if (!retail || sex < 0 || sex > 1 || !app::ChrModelForRace(retail, uint32_t(sex))) return -1;
    if (variation == VARIATION_SKIN && !colors) return 1;   // one "variation" whose colours are the skins
    if (variation == VARIATION_UNDERWEAR) return -1;
    const bool colourCount = colors && (variation == VARIATION_SKIN || variation == VARIATION_HAIR);
    if (colors && !colourCount) return 1;                   // faces and facial hair have no colour dimension
    const char* name = StockOptionName(uint32_t(variation), retail, uint32_t(sex), colors);
    if (!name) return -1;
    const uint32_t option = OptionByName(retail, uint32_t(sex), name);
    return option ? int32_t(app::ChoiceCount(option)) : 0;
}

// Stock CharSections lookups index Wrath rows that do not describe a Retail look; point them at row 0 so the
// stock validation passes and the real choice is applied through the Retail tables.
void RetailRaceSectionArguments(int32_t race, int32_t sex, int32_t* style, int32_t* color) {
    if (RetailRaceSectionCount(race, sex, VARIATION_FACE, false) < 0) return;
    *style = 0;
    *color = 0;
}

#else

bool RetailRaceMapped(uint32_t) { return false; }
uint32_t RetailRaceHelmetMask(uint32_t race) { return race && race < 32 ? uint32_t{1} << race : 0; }
uint32_t RetailRaceHelmetRace(uint32_t race) { return race; }
bool RetailRaceActive(CCharacterComponent*) { return false; }
bool RetailRaceOwns(uint32_t, uint32_t) { return false; }
bool RetailRaceModelPath(uint32_t, uint32_t, char*, size_t, uint32_t) { return false; }
bool RetailRaceNaked(CCharacterComponent*) { return false; }
void RetailRaceFormStore(CCharacterComponent*) {}
uint32_t RetailRaceFormRestore(CCharacterComponent*, CharacterPreferences&) { return 0; }
void RetailRaceFormClearMemory() {}
void RetailFormCommitPrepare(const char*, uint32_t, uint32_t) {}
void RetailFormCommitCreateResult(bool) {}
bool RetailFormCommitOffer(const char*, uint32_t, uint32_t, uint64_t) { return false; }
void RetailFormCommitListDone() {}
void RetailFormCommitAck(uint64_t, uint32_t, uint32_t) {}
void RetailFormCommitTick() {}
bool RetailRaceFormDual(CCharacterComponent*) { return false; }
uint32_t RetailRaceLookForm(uint64_t) { return 0; }
bool RetailRaceDragon(uint32_t, uint64_t) { return false; }
bool RetailRaceFormOffset(CCharacterComponent*, float& x, float& y) { x = y = 0.0f; return false; }
uint32_t RetailRaceMirrorSetup(CCharacterComponent*, CCharacterComponent*, CharacterPreferences&) { return 0; }
bool RetailRaceGeometry(CCharacterComponent*) { return false; }
bool RetailRaceBody(CCharacterComponent*) { return false; }
void RetailRaceTick() {}
void RetailRaceEquipment(CCharacterComponent*) {}
int RetailRaceCycleCommand(lua_State*, CCharacterComponent*, const char*) { return -1; }
void RetailRaceForget(CCharacterComponent*) {}
bool RetailRaceLegArmorHidden(CCharacterComponent*) { return false; }
void RetailRaceDuplicateCollections(CM2Model*, CM2Model*) {}
float RetailRacePreviewScale(uint32_t) { return 1.0f; }
void RetailRaceBindOwner(CCharacterComponent*, CGUnit_C*) {}
bool RetailRaceEyeLightning(CCharacterComponent*) { return false; }
void RetailRaceEyeLightning(CCharacterComponent*, bool) {}
void RetailRaceCreateEyeEffects(CDataStore*) {}
void BodyShapeCreateFooter(CDataStore*, bool) {}
void RetailLookCreateFooter(CDataStore*) {}
void RetailRaceSetLook(uint64_t, const CharacterRetailLook::Look&) {}
void RetailRaceClearLooks() {}
void RetailRaceSetParkedLook(uint64_t, const CharacterRetailLook::Look&) {}
bool RetailRaceParkedForm(uint64_t, uint32_t&) { return false; }
bool RetailRaceApplyParkedTo(CCharacterComponent*, uint64_t) { return false; }
bool RetailRaceApplyLookTo(CCharacterComponent*, uint64_t, bool) { return false; }
int32_t RetailRaceSectionCount(int32_t, int32_t, int32_t, bool) { return -1; }
void RetailRaceSectionArguments(int32_t, int32_t, int32_t*, int32_t*) {}

#endif

// Custom: the server side of the retail looks. Kept outside the Retail build switch so a client without Retail
// support still strips the tail blocks from the character list instead of rejecting it as over-long.

// A server with the retail look opcodes always ends the list with an RLE1 block (count 0 when no character has a
// look); a server without them drops the connection on CMSG_RETAIL_FORM_LOOK, so nothing is sent to it.
static bool s_serverRetailLooks = false;

bool RetailRaceServerLooks() {
    return s_serverRetailLooks;
}

// Strips the RLE1 / RLP1 tail blocks (per character guid(8) + payload, then count(u32) + magic) that the server
// appends to SMSG_ENUM_CHARACTERS_RESULT, and keeps their looks by guid.
void RetailRaceEnumTail(CDataStore* packet) {
    RetailRaceClearLooks();
    s_serverRetailLooks = false;
    if (!packet || !packet->m_data || packet->m_size < packet->m_base) {
        return;
    }

    uint8_t* bytes = packet->m_data;
    uint32_t size = packet->m_size - packet->m_base;
    if (size > packet->m_alloc) {
        return;
    }

    for (uint32_t tail = 0; tail < 2 && size >= 8; tail++) {
        uint32_t magic;
        uint32_t count;
        std::memcpy(&magic, bytes + size - 4, sizeof(magic));
        std::memcpy(&count, bytes + size - 8, sizeof(count));
        if (magic != CharacterRetailLook::EnumMagic && magic != CharacterRetailLook::ParkedMagic) {
            break;
        }

        const uint32_t stride = static_cast<uint32_t>(CharacterRetailLook::EnumStride);
        if (count > 255 || count > (size - 8) / stride) {
            break;
        }

        if (magic == CharacterRetailLook::EnumMagic) {
            s_serverRetailLooks = true;
        }

        const uint32_t start = size - 8 - count * stride;
        for (uint32_t i = 0; i < count; i++) {
            const uint8_t* entry = bytes + start + i * stride;
            uint64_t guid;
            std::memcpy(&guid, entry, sizeof(guid));
            CharacterRetailLook::Look look;
            if (!CharacterRetailLook::ReadPayload(entry + 8, CharacterRetailLook::PayloadSize, look)) {
                continue;
            }
            if (magic == CharacterRetailLook::ParkedMagic) {
                RetailRaceSetParkedLook(guid, look);
            } else {
                RetailRaceSetLook(guid, look);
            }
        }

        size = start;
        packet->m_size = packet->m_base + size;
    }
}

// SMSG_RETAIL_LOOK: guid(8) + payload, the full Retail customization of a player in view.
bool RetailRaceReceiveLook(CDataStore* msg) {
    const uint32_t bytes = static_cast<uint32_t>(8 + CharacterRetailLook::PayloadSize);
    if (!msg || msg->m_read > msg->m_size || msg->m_size - msg->m_read != bytes) {
        return false;
    }

    uint8_t data[8 + CharacterRetailLook::PayloadSize];
    msg->GetData(data, bytes);
    uint64_t guid;
    std::memcpy(&guid, data, sizeof(guid));

    CharacterRetailLook::Look look;
    if (!CharacterRetailLook::ReadPayload(data + 8, CharacterRetailLook::PayloadSize, look)) {
        return false;
    }

    RetailRaceSetLook(guid, look);
    return true;
}

// SMSG_RETAIL_FORM_LOOK_ACK: guid(8) + revision(4) + result(1), the answer to a Two Forms commit.
bool RetailRaceReceiveFormAck(CDataStore* msg) {
    if (!msg || msg->m_read > msg->m_size || msg->m_size - msg->m_read != 8 + 4 + 1) {
        return false;
    }

    uint64_t guid;
    uint32_t revision;
    uint8_t result;
    msg->Get(guid);
    msg->Get(revision);
    msg->Get(result);
    RetailFormCommitAck(guid, revision, result);
    return true;
}
