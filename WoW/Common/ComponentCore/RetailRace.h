#ifndef RETAIL_RACE_H
#define RETAIL_RACE_H

#include <cstddef>
#include <cstdint>

class CCharacterComponent;
struct CharacterPreferences;
class CGUnit_C;
class CM2Model;
struct lua_State;
class CDataStore;

// Custom (Kalibro's retail race layer, from his Esteria client): races taken 1:1 from the Retail client data (DB2
// customization tables, model and textures from the shipped files, local CASC or the CDN). A race is mapped to a Retail
// ChrRaces ID, or to Kalibro's Centaur / Slime Giant profiles, by the retailRaces CVar ("52=52,70=70,49=centaur").
// All functions return false/no-ops when the build has no Retail support or the race is not mapped.

// True if the Wrath race id is mapped to a Retail race (no table access).
bool RetailRaceMapped(uint32_t race);

// Bounded visual donor mask for stock helmet hides, including mapped custom race IDs >= 32.
uint32_t RetailRaceHelmetMask(uint32_t race);
// Custom: the race whose (256-bit) HelmetGeosetVisData bits a character uses: the retail race's helmet donor, or the
// race itself when it is not mapped
uint32_t RetailRaceHelmetRace(uint32_t race);

// True if the component's model is a Retail model of a mapped race. Retail geoset/texture logic must only run
// on such components: NPCs and other humanoids of the same race keep their stock models.
bool RetailRaceActive(CCharacterComponent* character);
void RetailRaceDuplicateCollections(CM2Model* source, CM2Model* target);
float RetailRacePreviewScale(uint32_t race);

// True if the race/sex is mapped and its Retail tables loaded.
bool RetailRaceOwns(uint32_t race, uint32_t sex);

// Writes the virtual model path ("retail/<FileDataID>.m2") that replaces the stock CreatureModelData path.
// `form` is an app::Form for Two Forms races; 0 keeps the race's default model.
bool RetailRaceModelPath(uint32_t race, uint32_t sex, char* path, size_t size, uint32_t form = 0);

// Two Forms side-by-side preview in the creator. FormDual: both forms are being shown. FormOffset: where `character`'s
// preview model stands. MirrorSetup: prepares the second component for the form `main` is not showing.
bool RetailRaceNaked(CCharacterComponent* character);
// The creator rebuilds its component on class, sex and race changes: Store before freeing the old one, Restore on the
// new one before its model is made (returns the form to build it for), ClearMemory when the creator closes.
void RetailRaceFormStore(CCharacterComponent* character);
uint32_t RetailRaceFormRestore(CCharacterComponent* character, CharacterPreferences& prefs);
void RetailRaceFormClearMemory();

// Two Forms commit to the server after a character is created (see docs/two-forms-server.md). Prepare as the creator
// submits; CreateResult with the create reply; Offer for each character of the next list (GUID discovery) and ListDone
// after it; Ack with SMSG_ESTERIA_FORM_LOOK_ACK; Tick each frame on the character screen to resend.
void RetailFormCommitPrepare(const char* name, uint32_t race, uint32_t sex);
void RetailFormCommitCreateResult(bool success);
bool RetailFormCommitOffer(const char* name, uint32_t race, uint32_t sex, uint64_t guid);
void RetailFormCommitListDone();
void RetailFormCommitAck(uint64_t guid, uint32_t revision, uint32_t result);
void RetailFormCommitTick();
bool RetailRaceFormDual(CCharacterComponent* main);
bool RetailRaceFormOffset(CCharacterComponent* character, float& x, float& y);
uint32_t RetailRaceMirrorSetup(CCharacterComponent* mirror, CCharacterComponent* main, CharacterPreferences& prefs);

// Shows customization geosets and binds hair/eye textures. Called from GeosRenderPrep.
bool RetailRaceGeometry(CCharacterComponent* character);

// Binds the body texture (retail skin sheet plus equipped item layers). Called after the stock section prep,
// which would otherwise leave its own composite on the model.
bool RetailRaceBody(CCharacterComponent* character);

// Capacity the stock selectors report for a mapped race: the size of the matching Retail option (variation is
// COMPONENT_VARIATIONS; `colors` asks for the colour count of that variation). -1 when the race is not mapped.
int32_t RetailRaceSectionCount(int32_t race, int32_t sex, int32_t variation, bool colors);

// Points stock CharSections lookups of a mapped race at row 0 so stock validation passes.
void RetailRaceSectionArguments(int32_t race, int32_t sex, int32_t* style, int32_t* color);

// Final equipment overrides after stock geoset selection: retail feet and helmet cosmetic hides.
void RetailRaceEquipment(CCharacterComponent* character);

// Answers the creator's EA_* commands (EA_RETAIL, EA_GET, EA_SET, EA_CYCLE, EA_CHOICES, EA_CHOICE_NAME, EA_RANDOM,
// EA_PREVIEW_*) for a mapped race. Returns the number of Lua results, or -1 if the command is not handled.
int RetailRaceCycleCommand(lua_State* L, CCharacterComponent* character, const char* command);

// Drops per-component retail state when the component is freed.
void RetailRaceForget(CCharacterComponent* character);

// Registers the world owner whose model reference must follow a per-character body change.
void RetailRaceBindOwner(CCharacterComponent* character, CGUnit_C* owner);

// Independent per-character cosmetic, transported separately from frozen appearance codecs.
bool RetailRaceEyeLightning(CCharacterComponent* character);
bool RetailRaceLegArmorHidden(CCharacterComponent* character);
void RetailRaceEyeLightning(CCharacterComponent* character, bool enabled);
void RetailRaceCreateEyeEffects(CDataStore* packet);
// Appends the creator's body sliders (BSH1 footer) after everything else. A new character only carries one when
// a slider is set; customization and race/faction changes always carry it so a reset reaches the server.
void BodyShapeCreateFooter(CDataStore* packet, bool always);

// The full Retail customization (one choice index per option, stock bytes included) travels like the body sliders:
// a footer on create / customize / faction / race change, a tail block on the character list, and one message per
// player in view. Looks are kept by guid; a component applies its owner's look once per update.
namespace CharacterRetailLook { struct Look; }
void RetailLookCreateFooter(CDataStore* packet);
void RetailRaceSetLook(uint64_t guid, const CharacterRetailLook::Look& look);
// The form (an app::Form) a character's saved look names, 0 if unknown: the model to create for it.
uint32_t RetailRaceLookForm(uint64_t guid);
// True for a Dracthyr player showing its dragon body (the client flies it with the dragon's own animations).
bool RetailRaceDragon(uint32_t race, uint64_t guid);
void RetailRaceClearLooks();
// Custom: SMSG_ENUM_CHARACTERS_RESULT tail blocks (RLE1 looks, RLP1 parked looks): read and stripped before stock
// parsing. Also clears the looks of the previous list.
void RetailRaceEnumTail(CDataStore* packet);
// Custom: SMSG_RETAIL_LOOK and SMSG_RETAIL_FORM_LOOK_ACK bodies. False when the message is malformed.
bool RetailRaceReceiveLook(CDataStore* msg);
bool RetailRaceReceiveFormAck(CDataStore* msg);
// Two Forms: the look a character is not showing (its other form), for the second model in character select.
void RetailRaceSetParkedLook(uint64_t guid, const CharacterRetailLook::Look& look);
bool RetailRaceParkedForm(uint64_t guid, uint32_t& form);
bool RetailRaceApplyParkedTo(CCharacterComponent* character, uint64_t guid);
// Applies the look saved for `guid` to a component (character select, creator). False when there is none or it
// does not fit the component's race and sex.
bool RetailRaceApplyLookTo(CCharacterComponent* character, uint64_t guid, bool refresh);

// Once per frame: keeps retail components moving through the stock section pipeline in the world. Stock drives that
// pipeline from the dynamic base-skin texture, which retail components deliberately do not bind.
void RetailRaceTick();

#endif
