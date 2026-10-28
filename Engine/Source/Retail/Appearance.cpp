#include "Engine/Source/Retail/Appearance.h"
#include "Engine/Source/Retail/Db2.h"
#include "Engine/Source/Retail/SourceContext.h"
#include "Engine/Source/Retail/RacePolicy.h"
#include "Engine/Source/Retail/CreationPolicy.h"
#include "Engine/Source/Retail/SlimeGiant.h"
#include "Engine/Source/Retail/Centaur.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <set>

namespace Retail::Db2::appearance
{
    namespace
    {
        void AppearanceLog(const char* format, ...)
        {
            va_list args;
            va_start(args, format);
            std::fprintf(stderr, "appearance: ");
            std::vfprintf(stderr, format, args);
            std::fputc('\n', stderr);
            va_end(args);
        }

        // --- table declarations ------------------------------------------------------------------
        constexpr Field kChrRaceXChrModelFields[] = {
            { "ChrRacesID" }, { "ChrModelID" }, { "Sex" }, { "AllowedTransmogSlots" },
        };
        constexpr Definition kChrRaceXChrModel{
            "chrracexchrmodel", "DBFilesClient\\chrracexchrmodel.db2", 0xA203BC29u,
            kChrRaceXChrModelFields,
        };

        constexpr Field kChrModelFields[] = {
            { "FaceCustomizationOffset", 3 }, { "CustomizeOffset", 3 }, { "ID" }, { "Sex" },
            { "DisplayID" }, { "CharComponentTextureLayoutID" }, { "Flags" },
            { "SkeletonFileDataID" }, { "ModelFallbackChrModelID" }, { "TextureFallbackChrModelID" },
            { "HelmVisFallbackChrModelID" }, { "CustomizeScale" }, { "CustomizeFacing" },
            { "CameraDistanceOffset" }, { "BarberShopCameraOffsetScale" },
            { "BarberShopCameraRotationFacing" }, { "BarberShopCameraRotationOffset" },
        };
        constexpr Definition kChrModel{
            "chrmodel", "DBFilesClient\\chrmodel.db2", 0x03FAB755u, kChrModelFields,
        };

        constexpr Field kChrCustomizationOptionFields[] = {
            { "Name_lang", 1, true }, { "ID" }, { "SecondaryID" }, { "Flags" }, { "ChrModelID" },
            { "OrderIndex" }, { "ChrCustomizationCategoryID" }, { "OptionType" },
            { "BarberShopCostModifier" }, { "ChrCustomizationID" }, { "Requirement" },
            { "SecondaryOrderIndex" }, { "AddedInPatch" },
        };
        constexpr Definition kChrCustomizationOption{
            "chrcustomizationoption", "DBFilesClient\\chrcustomizationoption.db2", 0xDCC2A86Eu,
            kChrCustomizationOptionFields,
        };

        constexpr Field kChrCustomizationChoiceFields[] = {
            { "Name_lang", 1, true }, { "ID" }, { "ChrCustomizationOptionID" }, { "ChrCustomizationReqID" },
            { "ChrCustomizationVisReqID" }, { "OrderIndex" }, { "UiOrderIndex" }, { "Flags" },
            { "AddedInPatch" }, { "SoundKitID" }, { "SwatchColor", 2 },
        };
        constexpr Definition kChrCustomizationChoice{
            "chrcustomizationchoice", "DBFilesClient\\chrcustomizationchoice.db2", 0x9559C358u,
            kChrCustomizationChoiceFields,
        };

        constexpr Field kChrCustomizationElementFields[] = {
            { "ChrCustomizationChoiceID" }, { "RelatedChrCustomizationChoiceID" },
            { "ChrCustomizationGeosetID" }, { "ChrCustomizationSkinnedModelID" },
            { "ChrCustomizationMaterialID" }, { "ChrCustomizationBoneSetID" },
            { "ChrCustomizationCondModelID" }, { "ChrCustomizationDisplayInfoID" },
            { "ChrCustItemGeoModifyID" }, { "ChrCustomizationVoiceID" }, { "AnimKitID" },
            { "ParticleColorID" }, { "ChrCustGeoComponentLinkID" },
        };
        constexpr Definition kChrCustomizationElement{
            "chrcustomizationelement", "DBFilesClient\\chrcustomizationelement.db2", 0x6483C37Eu,
            kChrCustomizationElementFields,
        };

        constexpr Field kChrCustomizationGeosetFields[] = {
            { "GeosetType" }, { "GeosetID" }, { "Modifier" },
        };
        constexpr Definition kChrCustomizationGeoset{
            "chrcustomizationgeoset", "DBFilesClient\\chrcustomizationgeoset.db2", 0xBF55F2FBu,
            kChrCustomizationGeosetFields,
        };

        constexpr Field kChrCustomizationSkinnedModelFields[] = {
            { "CollectionsFileDataID" }, { "GeosetType" }, { "GeosetID" }, { "Modifier" }, { "Flags" },
        };
        constexpr Definition kChrCustomizationSkinnedModel{
            "chrcustomizationskinnedmodel", "DBFilesClient\\chrcustomizationskinnedmodel.db2",
            0x4C32AA8Au, kChrCustomizationSkinnedModelFields,
        };

        constexpr Field kChrCustomizationMaterialFields[] = {
            { "ChrModelTextureTargetID" }, { "MaterialResourcesID" },
        };
        constexpr Definition kChrCustomizationMaterial{
            "chrcustomizationmaterial", "DBFilesClient\\chrcustomizationmaterial.db2", 0xBE9767E9u,
            kChrCustomizationMaterialFields,
        };

        constexpr Field kChrCustomizationCondModelFields[] = {
            { "CreatureModelDataID" }, { "Field_9_0_1_34081_001", 4 },
        };
        constexpr Definition kChrCustomizationCondModel{
            "chrcustomizationcondmodel", "DBFilesClient\\chrcustomizationcondmodel.db2",
            0x601A4522u, kChrCustomizationCondModelFields,
        };

        // Declared because kChrModelTextureLayer names it as a relation target, and a target that is
        // not declared fails validation for the WHOLE set: one missing table is the difference
        // between every appearance resolving and none of them resolving.
        constexpr Field kCharComponentTextureLayoutsFields[] = {
            { "Width" }, { "Height" },
        };
        constexpr Definition kCharComponentTextureLayouts{
            "charcomponenttexturelayouts", "DBFilesClient\\charcomponenttexturelayouts.db2",
            0x0B9AF134u, kCharComponentTextureLayoutsFields,
        };

        constexpr Field kChrModelTextureLayerFields[] = {
            { "TextureType" }, { "Layer" }, { "Flags" }, { "BlendMode" },
            { "TextureSectionTypeBitMask" }, { "TextureSectionTypeBitMask2" },
            { "Field_9_0_1_34365_006", 3 }, { "ChrModelTextureTargetID", 2 },
        };
        constexpr Relation kChrModelTextureLayerRelations[] = {
            { "CharComponentTextureLayoutsID", RelationSource::ParentId, {}, 0,
              "charcomponenttexturelayouts" },
        };
        constexpr Definition kChrModelTextureLayer{
            "chrmodeltexturelayer", "DBFilesClient\\chrmodeltexturelayer.db2", 0xD0583FB4u,
            kChrModelTextureLayerFields, kChrModelTextureLayerRelations,
        };

        constexpr Field kCreatureDisplayInfoFields[] = {
            { "ID" }, { "ModelID" }, { "SoundID" }, { "SizeClass" }, { "CreatureModelScale" },
            { "CreatureModelAlpha" }, { "BloodID" }, { "ExtendedDisplayInfoID" }, { "NPCSoundID" },
            { "ParticleColorID" }, { "PortraitCreatureDisplayInfoID" },
            { "PortraitTextureFileDataID" }, { "ObjectEffectPackageID" }, { "AnimReplacementSetID" },
            { "Flags" }, { "StateSpellVisualKitID" }, { "PlayerOverrideScale" },
            { "PetInstanceScale" }, { "UnarmedWeaponType" }, { "MountPoofSpellVisualKitID" },
            { "DissolveEffectID" }, { "Gender" }, { "DissolveOutEffectID" },
            { "CreatureModelMinLod" }, { "ConditionalCreatureModelID" }, { "MountMaxBankingAngle" },
            { "Field_11_0_0_54210_026" }, { "TextureVariationFileDataID", 4 },
        };
        constexpr Definition kCreatureDisplayInfo{
            "creaturedisplayinfo", "DBFilesClient\\creaturedisplayinfo.db2", 0x7275F5F6u,
            kCreatureDisplayInfoFields,
        };

        constexpr Field kCreatureModelDataFields[] = {
            { "GeoBox", 6 }, { "Flags" }, { "FileDataID" }, { "WalkSpeed" }, { "RunSpeed" },
            { "BloodID" }, { "FootprintTextureID" }, { "FootprintTextureLength" },
            { "FootprintTextureWidth" }, { "FootprintParticleScale" }, { "FoleyMaterialID" },
            { "FootstepCameraEffectID" }, { "DeathThudCameraEffectID" }, { "SoundID" },
            { "SizeClass" }, { "CollisionWidth" }, { "CollisionHeight" }, { "WorldEffectScale" },
            { "CreatureGeosetDataID" }, { "HoverHeight" }, { "AttachedEffectScale" },
            { "ModelScale" }, { "MissileCollisionRadius" }, { "MissileCollisionPush" },
            { "MissileCollisionRaise" }, { "MountHeight" }, { "OverrideLootEffectScale" },
            { "OverrideNameScale" }, { "OverrideSelectionRadius" }, { "TamedPetBaseScale" },
            { "MountScaleOtherIndex" }, { "MountScaleSelf" }, { "Field_11_0_0_54210_032" },
            { "MountScaleOther", 2 },
        };
        constexpr Definition kCreatureModelData{
            "creaturemodeldata", "DBFilesClient\\creaturemodeldata.db2", 0x0F5449F8u,
            kCreatureModelDataFields,
        };

        constexpr Field kCreatureDisplayInfoGeosetDataFields[] = {
            { "GeosetIndex" }, { "GeosetValue" },
        };
        constexpr Relation kCreatureDisplayInfoGeosetDataRelations[] = {
            { "CreatureDisplayInfoID", RelationSource::ParentId, {}, 0, "creaturedisplayinfo" },
        };
        constexpr Definition kCreatureDisplayInfoGeosetData{
            "creaturedisplayinfogeosetdata", "DBFilesClient\\creaturedisplayinfogeosetdata.db2",
            0x5E539080u, kCreatureDisplayInfoGeosetDataFields, kCreatureDisplayInfoGeosetDataRelations,
        };

        // Where each piece of a composited character sheet goes. The column order is not guessed: the
        // file's own values name them. Field 0 repeats the layout id the relationship column already
        // carries, field 1 runs 0..14 across every layout, and the four that follow tile each layout
        // exactly -- layout 1's rows cover 1024x1024 with no gap and no overlap, which is the size
        // CharComponentTextureLayouts states for it.
        constexpr Field kCharComponentTextureSectionsFields[] = {
            { "CharComponentTextureLayoutID" }, { "SectionType" },
            { "X" }, { "Y" }, { "Width" }, { "Height" }, { "OverlapSectionMask" },
        };
        constexpr Relation kCharComponentTextureSectionsRelations[] = {
            { "LayoutID", RelationSource::ParentId, {}, 0, "charcomponenttexturelayouts" },
        };
        constexpr Definition kCharComponentTextureSections{
            "charcomponenttexturesections", "DBFilesClient\\charcomponenttexturesections.db2",
            0x2173BA71u, kCharComponentTextureSectionsFields, kCharComponentTextureSectionsRelations,
        };

        constexpr Field kTextureFileDataFields[] = {
            { "ID" }, { "UsageType" }, { "MaterialResourcesID" },
        };
        constexpr Definition kTextureFileData{
            "texturefiledata", "DBFilesClient\\texturefiledata.db2", 0xBD7C74C2u, kTextureFileDataFields,
        };

        constexpr Definition kDefinitions[] = {
            kChrRaceXChrModel, kChrModel, kChrCustomizationOption, kChrCustomizationChoice,
            kChrCustomizationElement, kChrCustomizationGeoset, kChrCustomizationSkinnedModel,
            kChrCustomizationMaterial, kChrModelTextureLayer, kCreatureDisplayInfo,
            kCreatureModelData, kCreatureDisplayInfoGeosetData, kCharComponentTextureLayouts,
            kCharComponentTextureSections, kChrCustomizationCondModel,
        };

        // Loaded on first material lookup: 200k rows nobody needs until a texture is composed.

        struct Store {
            Table textureFileData;
            std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, uint32_t>>> materialFiles;
            std::unordered_map<uint32_t, uint32_t> extraFiles; // synthetic MaterialResourcesID -> FileDataID
            std::once_flag textureFileOnce;
            StoreStatus status;
            std::once_flag loadOnce;
            Table raceModel, chrModel, option, choice, element, geoset, skinnedModel, condModel;
            Table material, textureLayer, displayInfo, modelData, creatureGeoset;
            Table textureLayout, textureSection;
            std::unordered_map<uint64_t, uint32_t> modelByRaceSex;
            std::unordered_map<uint32_t, std::vector<const wdc5::Row*>> elementsByChoice;
            std::unordered_map<uint32_t, std::vector<const wdc5::Row*>> layersByLayout;
            std::unordered_map<uint32_t, std::vector<const wdc5::Row*>> geosetsByDisplay;
            std::unordered_map<uint32_t, std::vector<const wdc5::Row*>> sectionsByLayout;
            std::unordered_map<uint32_t, std::vector<const wdc5::Row*>> optionsByModel;
            std::unordered_map<uint32_t, std::vector<const wdc5::Row*>> choicesByOption;
        };
        Store& State() {
            static Store retail, forever;
            return currentSource == SourceKind::Forever ? forever : retail;
        }

        void LoadTextureFileData()
        {
            std::string error;
            if (!State().textureFileData.Load(kTextureFileData, &error))
            {
                AppearanceLog("texturefiledata: %s", error.c_str());
                return;
            }
            for (const wdc5::Row& row : State().textureFileData.Rows())
                State().materialFiles[State().textureFileData.Value(row, "MaterialResourcesID")].push_back(
                    { State().textureFileData.Value(row, "UsageType"), row.id });
        }

        constexpr uint64_t RaceSexKey(uint32_t race, uint32_t sex)
        {
            return (static_cast<uint64_t>(race) << 32) | sex;
        }

        /// A geoset group and its chosen value become the one number a submesh carries: the tables
        /// express the pair, the model only ever knows the product.
        ///
        /// Solid for a character, where the group is stated outright as a geoset TYPE. Applied to a
        /// creature on the strength of the analogy alone -- CreatureDisplayInfoGeosetData states an
        /// INDEX, and whether that index is the group itself or one less than it has not been checked
        /// against a model. If creature parts come out shifted by exactly one group, this is why.
        constexpr uint16_t GeosetId(uint32_t group, uint32_t value)
        {
            return static_cast<uint16_t>(group * 100u + value);
        }

        /// Puts each fan-out in the order the tables intend, so a caller's index is stable. Ties break
        /// on row id, because two rows sharing an OrderIndex must still not swap between runs.
        void SortByOrderIndex(std::unordered_map<uint32_t, std::vector<const wdc5::Row*>>& index,
                              const Table& table)
        {
            for (auto& [key, rows] : index)
            {
                std::sort(rows.begin(), rows.end(),
                          [&table](const wdc5::Row* a, const wdc5::Row* b)
                          {
                              const uint32_t oa = table.Value(*a, "OrderIndex");
                              const uint32_t ob = table.Value(*b, "OrderIndex");
                              return oa != ob ? oa < ob : a->id < b->id;
                          });
            }
        }

        ExtrasProvider g_extrasProvider = nullptr;

        /// Clones each configured face choice over the retail tables, so a face can be added without a new DB2.
        /// Runs after the tables decode and before any index over them is built.
        void ApplyExtras()
        {
            std::string text;
            if (currentSource != SourceKind::Retail || !g_extrasProvider || !g_extrasProvider(text)) return;

            uint32_t elementNext = 9000000, materialNext = 9000000, resourceNext = 9000000;
            std::istringstream lines(text);
            for (std::string line; std::getline(lines, line);)
            {
                std::istringstream words(line);
                std::string kind;
                uint32_t option = 0, source = 0, id = 0, fallback = 0;
                if (!(words >> kind >> option >> source >> id >> fallback) || kind != "face") continue;
                std::unordered_map<uint32_t, uint32_t> skins;
                for (std::string pair; words >> pair;)
                {
                    uint32_t related = 0, file = 0;
                    if (std::sscanf(pair.c_str(), "%u=%u", &related, &file) == 2) skins[related] = file;
                }

                const wdc5::Row* sourceRow = State().choice.Find(source);
                if (!sourceRow || State().choice.Value(*sourceRow, "ChrCustomizationOptionID") != option || State().choice.Find(id))
                {
                    AppearanceLog("extra face %u: source %u missing or id taken", id, source);
                    continue;
                }

                uint32_t order = 0, uiOrder = 0;
                for (const wdc5::Row& row : State().choice.Rows())
                {
                    if (State().choice.Value(row, "ChrCustomizationOptionID") != option) continue;
                    order = std::max(order, State().choice.Value(row, "OrderIndex") + 1);
                    uiOrder = std::max(uiOrder, State().choice.Value(row, "UiOrderIndex") + 1);
                }
                if (!State().choice.Clone(source, id, { { "OrderIndex", order }, { "UiOrderIndex", uiOrder } })) continue;

                std::vector<uint32_t> elements;
                for (const wdc5::Row& row : State().element.Rows())
                    if (State().element.Value(row, "ChrCustomizationChoiceID") == source) elements.push_back(row.id);
                for (const uint32_t elementId : elements)
                {
                    const wdc5::Row* element = State().element.Find(elementId);
                    const uint32_t related = State().element.Value(*element, "RelatedChrCustomizationChoiceID");
                    const uint32_t material = State().element.Value(*element, "ChrCustomizationMaterialID");
                    std::vector<std::pair<std::string_view, uint32_t>> sets = { { "ChrCustomizationChoiceID", id } };
                    if (material)
                    {
                        const auto skin = skins.find(related);
                        const uint32_t file = skin == skins.end() ? fallback : skin->second;
                        if (!file || !State().material.Clone(material, materialNext, { { "MaterialResourcesID", resourceNext } })) continue;
                        State().extraFiles[resourceNext++] = file;
                        sets.push_back({ "ChrCustomizationMaterialID", materialNext++ });
                    }
                    State().element.Clone(elementId, elementNext++, std::move(sets));
                }
                AppearanceLog("extra face %u added to option %u from choice %u", id, option, source);
            }
        }

        void Load()
        {
            std::string error;
            if (!ValidateDefinitions(kDefinitions, &error))
            {
                State().status.failed = true;
                State().status.error = error;
                AppearanceLog("%s", error.c_str());
                return;
            }

            const struct { Table* table; const Definition* def; uint32_t* rows; } kLoads[] = {
                { &State().raceModel,      &kDefinitions[0],  &State().status.raceModels      },
                { &State().chrModel,       &kDefinitions[1],  &State().status.chrModels       },
                { &State().option,         &kDefinitions[2],  &State().status.options         },
                { &State().choice,         &kDefinitions[3],  &State().status.choices         },
                { &State().element,        &kDefinitions[4],  &State().status.elements        },
                { &State().geoset,         &kDefinitions[5],  &State().status.geosets         },
                { &State().skinnedModel,   &kDefinitions[6],  &State().status.skinnedModels   },
                { &State().material,       &kDefinitions[7],  &State().status.materials       },
                { &State().textureLayer,   &kDefinitions[8],  &State().status.textureLayers   },
                { &State().displayInfo,    &kDefinitions[9],  &State().status.displayInfos    },
                { &State().modelData,      &kDefinitions[10], &State().status.modelData       },
                { &State().creatureGeoset, &kDefinitions[11], &State().status.creatureGeosets },
                { &State().textureLayout,  &kDefinitions[12], &State().status.textureLayouts  },
                { &State().textureSection, &kDefinitions[13], &State().status.textureSections },
                { &State().condModel,      &kDefinitions[14], &State().status.conditionalModels },
            };

            bool allOk = true;
            for (const auto& load : kLoads)
            {
                std::string tableError;
                if (load.table->Load(*load.def, &tableError))
                {
                    *load.rows = static_cast<uint32_t>(load.table->Rows().size());
                    continue;
                }
                // Creature tables may be encrypted without the matching TACT key; characters
                // then resolve everything except their model file.
                const bool optional = load.table == &State().displayInfo || load.table == &State().creatureGeoset;
                if (!optional) allOk = false;
                if (!State().status.error.empty()) State().status.error += " | ";
                State().status.error += std::string(load.def->name) + ": " + tableError;
            }

            State().status.loaded = allOk;
            State().status.failed = !allOk;
            if (!allOk)
            {
                AppearanceLog("%s", State().status.error.c_str());
                return;
            }

            ApplyExtras();

            for (const wdc5::Row& row : State().raceModel.Rows())
            {
                const uint64_t key = RaceSexKey(State().raceModel.Value(row, "ChrRacesID"),
                                                State().raceModel.Value(row, "Sex"));
                State().modelByRaceSex[key] = State().raceModel.Value(row, "ChrModelID");
            }
            for (const wdc5::Row& row : State().element.Rows())
                State().elementsByChoice[State().element.Value(row, "ChrCustomizationChoiceID")].push_back(&row);
            for (const wdc5::Row& row : State().textureLayer.Rows())
                State().layersByLayout[row.parentId].push_back(&row);
            for (const wdc5::Row& row : State().creatureGeoset.Rows())
                State().geosetsByDisplay[row.parentId].push_back(&row);
            // Keyed on the stated layout rather than the relation's parent: the parent is what the
            // file's own foreign key resolves to, and a section that names its layout in a field
            // would otherwise be filed under nothing.
            for (const wdc5::Row& row : State().textureSection.Rows())
            {
                const uint32_t layout = State().textureSection.Value(row, "CharComponentTextureLayoutID");
                State().sectionsByLayout[layout ? layout : row.parentId].push_back(&row);
            }

            for (const wdc5::Row& row : State().option.Rows())
                State().optionsByModel[State().option.Value(row, "ChrModelID")].push_back(&row);
            for (const wdc5::Row& row : State().choice.Rows())
                State().choicesByOption[State().choice.Value(row, "ChrCustomizationOptionID")].push_back(&row);
            SortByOrderIndex(State().optionsByModel, State().option);
            SortByOrderIndex(State().choicesByOption, State().choice);

            AppearanceLog("%u chr model(s) over %u race/sex pair(s), %u option(s) and"
                      " %u choice(s) expanding to %u element(s), %u creature display(s) |"
                      " %u sheet layout(s) tiled by %u section(s)",
                      State().status.chrModels, static_cast<unsigned>(State().modelByRaceSex.size()),
                      State().status.options, State().status.choices, State().status.elements, State().status.displayInfos,
                      State().status.textureLayouts, State().status.textureSections);
        }

        void EnsureLoaded()
        {
            std::call_once(State().loadOnce, &Load);
        }

        uint32_t ModelFileFromDisplay(uint32_t displayId)
        {
            const wdc5::Row* display = State().displayInfo.Find(displayId);
            if (!display) return 0;
            const wdc5::Row* model = State().modelData.Find(State().displayInfo.Value(*display, "ModelID"));
            return model ? State().modelData.Value(*model, "FileDataID") : 0;
        }

        void AddMaterialLayers(uint32_t materialId, uint32_t layoutId, Recipe& out)
        {
            const wdc5::Row* material = State().material.Find(materialId);
            if (!material) return;

            const uint32_t target = State().material.Value(*material, "ChrModelTextureTargetID");
            const uint32_t resource = State().material.Value(*material, "MaterialResourcesID");

            const auto layers = State().layersByLayout.find(layoutId);
            if (layers == State().layersByLayout.end()) return;

            for (const wdc5::Row* layer : layers->second)
            {
                if (State().textureLayer.Value(*layer, "ChrModelTextureTargetID") != target) continue;
                out.layers.push_back({
                    State().textureLayer.Value(*layer, "TextureType"),
                    State().textureLayer.Value(*layer, "Layer"),
                    State().textureLayer.Value(*layer, "BlendMode"),
                    State().textureLayer.Value(*layer, "TextureSectionTypeBitMask"),
                    resource,
                });
            }
        }

        /**
         * @brief Everything one taken choice contributes, filtered by what else was taken.
         *
         * An element may name a SECOND choice it depends on, which is how two options cross: a hair
         * style lists one element per hair colour, each waiting for its colour to be chosen too.
         * Taking them all hands back every colour of that style at once, stacked -- nineteen of them
         * for one human hairstyle.
         */
        void AddChoice(uint32_t choiceId, uint32_t layoutId, const uint32_t* selected,
                       uint32_t selectedCount, Recipe& out, bool allowModelSwap)
        {
            const auto elements = State().elementsByChoice.find(choiceId);
            if (elements == State().elementsByChoice.end()) return;
            bool visibleCollection = false, blindfoldMaterial = false;

            for (const wdc5::Row* element : elements->second)
            {
                const uint32_t needs = State().element.Value(*element, "RelatedChrCustomizationChoiceID");
                if (needs)
                {
                    bool alsoTaken = false;
                    for (uint32_t i = 0; i < selectedCount && !alsoTaken; ++i)
                        alsoTaken = selected[i] == needs;
                    if (!alsoTaken) continue;
                }

                if (const uint32_t id = State().element.Value(*element, "ChrCustomizationGeosetID"))
                {
                    if (const wdc5::Row* geoset = State().geoset.Find(id))
                        out.geosets.push_back(GeosetId(State().geoset.Value(*geoset, "GeosetType"),
                                                       State().geoset.Value(*geoset, "GeosetID")));
                }
                if (const uint32_t id = State().element.Value(*element, "ChrCustomizationSkinnedModelID"))
                {
                    if (const wdc5::Row* skinned = State().skinnedModel.Find(id)) {
                        out.attached.push_back({
                            State().skinnedModel.Value(*skinned, "CollectionsFileDataID"),
                            GeosetId(State().skinnedModel.Value(*skinned, "GeosetType"),
                                     State().skinnedModel.Value(*skinned, "GeosetID")),
                        });
                        visibleCollection = visibleCollection || (out.attached.back().fileDataId && out.attached.back().geoset % 100);
                    }
                }
                if (const uint32_t id = State().element.Value(*element, "ChrCustomizationMaterialID")) {
                    AddMaterialLayers(id, layoutId, out);
                    const auto* material = State().material.Find(id);
                    blindfoldMaterial = blindfoldMaterial || (material && State().material.Value(*material, "ChrModelTextureTargetID") == 40);
                }
                if (const uint32_t id = allowModelSwap ? State().element.Value(*element, "ChrCustomizationCondModelID") : 0) {
                    if (const auto* conditional = State().condModel.Find(id)) {
                        bool unconditional = true;
                        for (uint32_t i = 0; i < 4; ++i)
                            unconditional = unconditional && !State().condModel.Value(*conditional, "Field_9_0_1_34081_001", i);
                        const auto* model = State().modelData.Find(State().condModel.Value(*conditional, "CreatureModelDataID"));
                        if (unconditional && model && State().modelData.Value(*model, "FileDataID"))
                            out.modelFileDataId = State().modelData.Value(*model, "FileDataID");
                    }
                }
            }
            // Shared Elf blindfolds are offered with ordinary eye colors, but the new palette
            // materials are gated to Demon Hunter eye choices on a different ChrModel. If none
            // matches, use the first authored blindfold palette without changing selected eyes.
            // Limit this to target 40 on a visible selected collection.
            if (visibleCollection && !blindfoldMaterial) {
                uint32_t fallback = 0, option = 0, order = UINT32_MAX;
                bool compatible = true;
                for (const auto* element : elements->second) {
                    const auto* related = State().choice.Find(State().element.Value(*element, "RelatedChrCustomizationChoiceID"));
                    const uint32_t id = State().element.Value(*element, "ChrCustomizationMaterialID");
                    const auto* material = State().material.Find(id);
                    if (!related || !material || State().material.Value(*material, "ChrModelTextureTargetID") != 40) continue;
                    const uint32_t current = State().choice.Value(*related, "ChrCustomizationOptionID");
                    if (option && option != current) compatible = false;
                    option = current;
                    const uint32_t rank = State().choice.Value(*related, "OrderIndex");
                    if (rank < order) { order = rank; fallback = id; }
                }
                if (compatible && fallback) AddMaterialLayers(fallback, layoutId, out);
            }
        }
    }

    void SetExtrasProvider(ExtrasProvider provider)
    {
        g_extrasProvider = provider;
    }

    const StoreStatus& Status()
    {
        EnsureLoaded();
        return State().status;
    }

    // Worgen's Human form borrows generic Human (Retail race 1): no distinct Gilnean ChrModel exists.
    static uint32_t ModelForForm(uint32_t chrRaceId, uint32_t sex, uint32_t form, uint32_t rowModel)
    {
        if (currentSource != SourceKind::Retail) return rowModel;
        const bool dracthyr = chrRaceId == 52 || chrRaceId == 70;
        if (form == FormDragon && dracthyr) return rowModel;
        if (form == FormVisage && dracthyr && sex < 2) return 127 + sex;
        if (form == FormWolf && chrRaceId == 22) return rowModel;
        if (form == FormHuman && chrRaceId == 22) {
            const auto human = State().modelByRaceSex.find(RaceSexKey(1, sex));
            return human != State().modelByRaceSex.end() ? human->second : 0;
        }
        return CharacterModel(chrRaceId, sex, rowModel);
    }

    bool BuildForCharacter(uint32_t chrRaceId, uint32_t sex, const uint32_t* choiceIds,
                           uint32_t choiceCount, Recipe& out, uint32_t form)
    {
        if (currentSource == SourceKind::Retail && chrRaceId == Centaur::Profile)
            return Centaur::Build(sex, choiceIds, choiceCount, out);
        if (currentSource == SourceKind::Retail && chrRaceId == SlimeGiant::Profile)
            return SlimeGiant::Build(sex, choiceIds, choiceCount, out);
        out.Clear();
        EnsureLoaded();
        if (!State().status.loaded) return false;

        const auto pair = State().modelByRaceSex.find(RaceSexKey(chrRaceId, sex));
        if (pair == State().modelByRaceSex.end()) return false;

        const uint32_t selectedModel = ModelForForm(chrRaceId, sex, form, pair->second);
        const wdc5::Row* model = State().chrModel.Find(selectedModel);
        if (!model) return false;

        out.skeletonFileDataId = State().chrModel.Value(*model, "SkeletonFileDataID");
        out.textureLayoutId    = State().chrModel.Value(*model, "CharComponentTextureLayoutID");
        out.modelFileDataId    = ModelFileFromDisplay(State().chrModel.Value(*model, "DisplayID"));

        for (uint32_t i = 0; i < choiceCount; ++i) {
            const auto* choice = State().choice.Find(choiceIds[i]);
            const auto* option = choice ? State().option.Find(State().choice.Value(*choice, "ChrCustomizationOptionID")) : nullptr;
            if (!option || State().option.Value(*option, "ChrModelID") != selectedModel) continue;
            AddChoice(choiceIds[i], out.textureLayoutId, choiceIds, choiceCount, out,
                sex == 0 && (chrRaceId == 2 || chrRaceId == 36)); // Orc posture; other conditional forms need separate integration.
        }

        std::stable_sort(out.layers.begin(), out.layers.end(),
                         [](const TextureLayer& a, const TextureLayer& b) {
                             return a.layer < b.layer;
                         });

        return out.modelFileDataId != 0;
    }

    bool BuildForCreature(uint32_t creatureDisplayInfoId, Recipe& out)
    {
        out.Clear();
        EnsureLoaded();
        if (!State().status.loaded) return false;

        out.modelFileDataId = ModelFileFromDisplay(creatureDisplayInfoId);
        if (!out.modelFileDataId) return false;

        const auto geosets = State().geosetsByDisplay.find(creatureDisplayInfoId);
        if (geosets != State().geosetsByDisplay.end())
            for (const wdc5::Row* row : geosets->second)
                out.geosets.push_back(GeosetId(State().creatureGeoset.Value(*row, "GeosetIndex"),
                                               State().creatureGeoset.Value(*row, "GeosetValue")));

        return true;
    }

    uint32_t ChrModelForRace(uint32_t chrRaceId, uint32_t sex, uint32_t form)
    {
        if (currentSource == SourceKind::Retail && chrRaceId == Centaur::Profile)
            return sex < 2 ? Centaur::Profile + sex : 0;
        if (currentSource == SourceKind::Retail && chrRaceId == SlimeGiant::Profile)
            return sex == 0 ? SlimeGiant::Profile : 0;
        EnsureLoaded();
        if (!State().status.loaded) return 0;
        const auto pair = State().modelByRaceSex.find(RaceSexKey(chrRaceId, sex));
        return pair != State().modelByRaceSex.end() ? ModelForForm(chrRaceId, sex, form, pair->second) : 0;
    }

    bool CreationChoiceAllowed(uint32_t chrModelId, uint32_t choiceId)
    {
        if (currentSource == SourceKind::Retail && Centaur::ProfileModel(chrModelId))
            return Centaur::CreationAllowed(chrModelId, choiceId);
        if (currentSource == SourceKind::Retail && chrModelId == SlimeGiant::Profile)
            return !SlimeGiant::ChoiceName(choiceId).empty();
        EnsureLoaded();
        if (!State().status.loaded) return false;
        const auto* choice = State().choice.Find(choiceId);
        const auto* option = choice ? State().option.Find(State().choice.Value(*choice, "ChrCustomizationOptionID")) : nullptr;
        return option && State().option.Value(*option, "ChrModelID") == chrModelId &&
            AllowCreationChoice(currentSource, chrModelId, State().choice.Value(*choice, "ChrCustomizationReqID"),
                State().choice.Text(*choice, "Name_lang"));
    }

    bool CollectCharacterAssets(uint32_t race, uint32_t sex, CharacterAssets& assets, std::string& error, uint32_t form)
    {
        assets = {};
        error.clear();
        if (currentSource == SourceKind::Retail && race == Centaur::Profile) {
            if (sex > 1) { error = "Centaur sex is out of range"; return false; }
            assets.models = {Centaur::Bodies[sex]};
            assets.textures.assign(std::begin(Centaur::Skins[sex]), std::end(Centaur::Skins[sex]));
            assets.textures.push_back(sex ? 4081389 : 4036660);
            return true;
        }
        if (currentSource == SourceKind::Retail && race == SlimeGiant::Profile) {
            if (sex) { error = "Slime Giant has one authored body"; return false; }
            assets.models = {SlimeGiant::Body};
            assets.textures = {3491663, 3491664, 3491665, 3491666, 3491667,
                               3491668, 3491669, 3491670, 3491671, 3491672};
            return true;
        }
        Recipe base;
        if (!BuildForCharacter(race, sex, nullptr, 0, base, form)) { error = "No character model for requested race/sex"; return false; }
        std::set<uint32_t> models{base.modelFileDataId}, textures, skeletons, choices;
        if (base.skeletonFileDataId) skeletons.insert(base.skeletonFileDataId);
        const uint32_t model = ChrModelForRace(race, sex, form);
        for (uint32_t i = 0; i < OptionCount(model); ++i) {
            ChrOption option{};
            if (!OptionAt(model, i, option)) return false;
            for (uint32_t j = 0; j < ChoiceCount(option.id); ++j) {
                ChrChoice choice{};
                if (!ChoiceAt(option.id, j, choice)) return false;
                choices.insert(choice.id);
            }
        }
        for (const auto& row : State().element.Rows()) {
            if (!choices.count(State().element.Value(row, "ChrCustomizationChoiceID"))) continue;
            const uint32_t related = State().element.Value(row, "RelatedChrCustomizationChoiceID");
            if (related && !choices.count(related)) continue;
            if (const auto* material = State().material.Find(State().element.Value(row, "ChrCustomizationMaterialID"))) {
                const auto id = MaterialFileDataId(State().material.Value(*material, "MaterialResourcesID"));
                if (!id) { error = "Customization material has no texture FileDataID"; return false; }
                textures.insert(id);
            }
            if (const auto* part = State().skinnedModel.Find(State().element.Value(row, "ChrCustomizationSkinnedModelID"))) {
                const auto id = State().skinnedModel.Value(*part, "CollectionsFileDataID");
                if (id) models.insert(id);
            }
            if (const auto* part = State().condModel.Find(State().element.Value(row, "ChrCustomizationCondModelID"))) {
                const auto* data = State().modelData.Find(State().condModel.Value(*part, "CreatureModelDataID"));
                const auto id = data ? State().modelData.Value(*data, "FileDataID") : 0;
                if (id) models.insert(id);
            }
        }
        assets.models.assign(models.begin(), models.end());
        assets.textures.assign(textures.begin(), textures.end());
        assets.skeletons.assign(skeletons.begin(), skeletons.end());
        return true;
    }

    uint32_t OptionCount(uint32_t chrModelId)
    {
        if (currentSource == SourceKind::Retail && Centaur::ProfileModel(chrModelId))
            return chrModelId == Centaur::Profile ? 3 : 2;
        if (currentSource == SourceKind::Retail && chrModelId == SlimeGiant::Profile) return 2;
        EnsureLoaded();
        if (!State().status.loaded) return 0;
        const auto options = State().optionsByModel.find(chrModelId);
        return options != State().optionsByModel.end() ? static_cast<uint32_t>(options->second.size()) : 0;
    }

    bool OptionAt(uint32_t chrModelId, uint32_t index, ChrOption& out)
    {
        if (currentSource == SourceKind::Retail && Centaur::ProfileModel(chrModelId)) {
            if (index >= (chrModelId == Centaur::Profile ? 3u : 2u)) return false;
            out = {Centaur::Option(chrModelId, index), 0, index, 0};
            return true;
        }
        if (currentSource == SourceKind::Retail && chrModelId == SlimeGiant::Profile) {
            if (index >= 2) return false;
            out = {index == 0 ? SlimeGiant::ColorOption : SlimeGiant::DripOption, 0, index, 0};
            return true;
        }
        EnsureLoaded();
        if (!State().status.loaded) return false;
        const auto options = State().optionsByModel.find(chrModelId);
        if (options == State().optionsByModel.end() || index >= options->second.size()) return false;

        const wdc5::Row& row = *options->second[index];
        out.id         = row.id;
        out.optionType = State().option.Value(row, "OptionType");
        out.orderIndex = State().option.Value(row, "OrderIndex");
        out.flags      = State().option.Value(row, "Flags");
        return true;
    }

    uint32_t ChoiceCount(uint32_t chrCustomizationOptionId)
    {
        if (currentSource == SourceKind::Retail && Centaur::Count(chrCustomizationOptionId))
            return Centaur::Count(chrCustomizationOptionId);
        if (currentSource == SourceKind::Retail && SlimeGiant::Count(chrCustomizationOptionId))
            return SlimeGiant::Count(chrCustomizationOptionId);
        EnsureLoaded();
        if (!State().status.loaded) return 0;
        const auto choices = State().choicesByOption.find(chrCustomizationOptionId);
        return choices != State().choicesByOption.end() ? static_cast<uint32_t>(choices->second.size()) : 0;
    }

    bool ChoiceAt(uint32_t chrCustomizationOptionId, uint32_t index, ChrChoice& out)
    {
        if (currentSource == SourceKind::Retail && Centaur::Count(chrCustomizationOptionId))
            return Centaur::Choice(chrCustomizationOptionId, index, out);
        if (currentSource == SourceKind::Retail && SlimeGiant::Count(chrCustomizationOptionId))
            return SlimeGiant::Choice(chrCustomizationOptionId, index, out);
        EnsureLoaded();
        if (!State().status.loaded) return false;
        const auto choices = State().choicesByOption.find(chrCustomizationOptionId);
        if (choices == State().choicesByOption.end() || index >= choices->second.size()) return false;

        const wdc5::Row& row = *choices->second[index];
        out.id           = row.id;
        out.orderIndex   = State().choice.Value(row, "OrderIndex");
        out.uiOrderIndex = State().choice.Value(row, "UiOrderIndex");
        out.flags        = State().choice.Value(row, "Flags");
        out.swatchColor  = State().choice.Value(row, "SwatchColor");
        return true;
    }

    uint32_t LayoutForModel(uint32_t chrModelId)
    {
        if (currentSource == SourceKind::Retail && Centaur::ProfileModel(chrModelId)) return 141;
        EnsureLoaded();
        if (!State().status.loaded) return 0;
        const wdc5::Row* model = State().chrModel.Find(chrModelId);
        return model ? State().chrModel.Value(*model, "CharComponentTextureLayoutID") : 0;
    }

    bool LayoutSize(uint32_t layoutId, uint32_t& outWidth, uint32_t& outHeight)
    {
        EnsureLoaded();
        if (!State().status.loaded) return false;

        for (const wdc5::Row& row : State().textureLayout.Rows())
        {
            if (row.id != layoutId) continue;
            outWidth  = State().textureLayout.Value(row, "Width");
            outHeight = State().textureLayout.Value(row, "Height");
            return outWidth != 0 && outHeight != 0;
        }
        return false;
    }

    uint32_t SectionCount(uint32_t layoutId)
    {
        EnsureLoaded();
        if (!State().status.loaded) return 0;
        const auto sections = State().sectionsByLayout.find(layoutId);
        return sections != State().sectionsByLayout.end()
             ? static_cast<uint32_t>(sections->second.size()) : 0;
    }

    bool SectionAt(uint32_t layoutId, uint32_t index, TextureSection& out)
    {
        EnsureLoaded();
        if (!State().status.loaded) return false;
        const auto sections = State().sectionsByLayout.find(layoutId);
        if (sections == State().sectionsByLayout.end() || index >= sections->second.size()) return false;

        const wdc5::Row& row = *sections->second[index];
        out.sectionType = State().textureSection.Value(row, "SectionType");
        out.x           = State().textureSection.Value(row, "X");
        out.y           = State().textureSection.Value(row, "Y");
        out.width       = State().textureSection.Value(row, "Width");
        out.height      = State().textureSection.Value(row, "Height");
        out.overlapMask = State().textureSection.Value(row, "OverlapSectionMask");
        return true;
    }

    uint32_t MaterialFileDataId(uint32_t materialResourcesId, uint32_t usage)
    {
        if (const auto extra = State().extraFiles.find(materialResourcesId); extra != State().extraFiles.end()) return extra->second;
        std::call_once(State().textureFileOnce, &LoadTextureFileData);
        const auto it = State().materialFiles.find(materialResourcesId);
        if (it == State().materialFiles.end()) return 0;
        for (const uint32_t wanted : { usage, 2u })
            for (const auto& candidate : it->second)
                if (candidate.first == wanted) return candidate.second;
        return it->second.front().second;
    }

    std::string OptionName(uint32_t optionId)
    {
        if (currentSource == SourceKind::Retail && !Centaur::OptionName(optionId).empty())
            return Centaur::OptionName(optionId);
        if (currentSource == SourceKind::Retail && optionId == SlimeGiant::ColorOption) return "Slime Color";
        if (currentSource == SourceKind::Retail && optionId == SlimeGiant::DripOption) return "Slime Drip";
        EnsureLoaded();
        if (!State().status.loaded) return {};
        const wdc5::Row* row = State().option.Find(optionId);
        return row ? State().option.Text(*row, "Name_lang") : std::string();
    }

    std::string ChoiceName(uint32_t choiceId)
    {
        if (currentSource == SourceKind::Retail && !Centaur::ChoiceName(choiceId).empty())
            return Centaur::ChoiceName(choiceId);
        if (currentSource == SourceKind::Retail && !SlimeGiant::ChoiceName(choiceId).empty())
            return SlimeGiant::ChoiceName(choiceId);
        EnsureLoaded();
        if (!State().status.loaded) return {};
        const wdc5::Row* row = State().choice.Find(choiceId);
        return row ? State().choice.Text(*row, "Name_lang") : std::string();
    }

    const Table* Get(std::string_view table)
    {
        EnsureLoaded();
        if (!State().status.loaded) return nullptr;

        const struct { std::string_view name; const Table* table; } kTables[] = {
            { "chrracexchrmodel",              &State().raceModel      },
            { "texturefiledata",               &State().textureFileData },
            { "chrmodel",                      &State().chrModel       },
            { "chrcustomizationoption",        &State().option         },
            { "chrcustomizationchoice",        &State().choice         },
            { "chrcustomizationelement",       &State().element        },
            { "chrcustomizationgeoset",        &State().geoset         },
            { "chrcustomizationskinnedmodel",  &State().skinnedModel   },
            { "chrcustomizationmaterial",      &State().material       },
            { "chrcustomizationcondmodel",     &State().condModel      },
            { "chrmodeltexturelayer",          &State().textureLayer   },
            { "creaturedisplayinfo",           &State().displayInfo    },
            { "creaturemodeldata",             &State().modelData      },
            { "creaturedisplayinfogeosetdata", &State().creatureGeoset },
            { "charcomponenttexturelayouts",   &State().textureLayout  },
            { "charcomponenttexturesections",  &State().textureSection },
        };
        for (const auto& entry : kTables)
            if (entry.name == table) return entry.table;
        return nullptr;
    }
}
