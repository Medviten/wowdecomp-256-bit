// Retail character/creature appearance resolver. Derived from WarcraftXL wxl-db2 (GPL-3.0-or-later),
// https://github.com/WarcraftXL/wxl-db2 -- Copyright (C) 2026 WarcraftXL. Adapted for wowdecomp.
#ifndef ENGINE_SOURCE_RETAIL_APPEARANCE_H
#define ENGINE_SOURCE_RETAIL_APPEARANCE_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Retail::Db2::appearance
{
    struct TextureLayer {
        uint32_t textureType, layer, blendMode, sectionMask, materialResourcesId;
        uint32_t fileDataId = 0; // Creature palettes already name the original file, without a material resource.
    };
    struct AttachedModel { uint32_t fileDataId; uint16_t geoset; };
    struct ChrOption { uint32_t id, optionType, orderIndex, flags; };
    struct ChrChoice { uint32_t id, orderIndex, uiOrderIndex, flags, swatchColor; };
    struct TextureSection { uint32_t sectionType, x, y, width, height, overlapMask; };
}

namespace Retail::Db2 { class Table; }

namespace Retail::Db2::appearance
{
    /// Everything needed to draw one appearance, in the model side's vocabulary rather than the
    /// tables'. Both producers below fill this same shape: a creature's parts and a character's are
    /// the same question asked of different tables, and the consumer should not have to know which.
    /// Which of a race's two Two Forms models to use. FormDefault keeps the single model the race
    /// has always resolved to (Dracthyr stays Visage); the others are only meaningful for the
    /// Worgen (Retail 22) and Dracthyr (Retail 52/70) rows named here and fall back to default otherwise.
    enum Form : uint32_t { FormDefault = 0, FormWolf, FormHuman, FormDragon, FormVisage };

    struct Recipe
    {
        uint32_t modelFileDataId = 0;
        uint32_t skeletonFileDataId = 0; ///< 0 when the model is not split-skeleton
        uint32_t textureLayoutId = 0;    ///< the sheet layout `layers` address

        /// Final geoset ids, as the model's own submeshes carry them (group * 100 + value).
        std::vector<uint16_t>          geosets;
        std::vector<TextureLayer>  layers;
        std::vector<AttachedModel> attached;

        /// Empties the lists without giving their storage back: a recipe is asked for repeatedly and
        /// the answers are all about the same size, so the buffers are worth keeping.
        void Clear()
        {
            modelFileDataId = skeletonFileDataId = textureLayoutId = 0;
            geosets.clear();
            layers.clear();
            attached.clear();
        }
    };

    /// How the load went, for the panel and the log.
    struct StoreStatus
    {
        bool        loaded = false; ///< a load was attempted and every required table decoded
        bool        failed = false;
        std::string error;
        uint32_t    chrModels = 0, raceModels = 0, options = 0, choices = 0;
        uint32_t    elements = 0, geosets = 0;
        uint32_t    materials = 0, skinnedModels = 0, textureLayers = 0, textureLayouts = 0;
        uint32_t    textureSections = 0, conditionalModels = 0;
        uint32_t    displayInfos = 0, modelData = 0, creatureGeosets = 0;
    };

    /// Extra customization choices layered over the retail tables (the client supplies them from its archives).
    /// Fills `text` with lines `face <option> <sourceChoice> <newChoice> <defaultFileDataId> [<relatedChoice>=<fileDataId> ...]`
    /// and `#` comments. Each face clones the source choice, its elements and its materials under new ids, and points the
    /// cloned materials at the given textures. Must be set before the first appearance query.
    using ExtrasProvider = bool (*)(std::string& text);
    void SetExtrasProvider(ExtrasProvider provider);

    /// Triggers the load on first use.
    const StoreStatus& Status();

    /**
     * @brief The appearance of a player character.
     * @param chrRaceId   ChrRaces id.
     * @param sex         0 or 1, as ChrRaceXChrModel states it.
     * @param choiceIds   ChrCustomizationChoice ids, in any order.
     * @param choiceCount how many.
     * @param out         cleared, then filled.
     * @return false when the tables hold no model for that race and sex.
     */
    bool BuildForCharacter(uint32_t chrRaceId, uint32_t sex, const uint32_t* choiceIds,
                           uint32_t choiceCount, Recipe& out, uint32_t form = FormDefault);

    /**
     * @brief The appearance of a creature or NPC, entirely from its display entry.
     * @param creatureDisplayInfoId CreatureDisplayInfo id.
     * @param out                   cleared, then filled.
     * @return false when the tables hold no such display entry, or it names no model.
     */
    bool BuildForCreature(uint32_t creatureDisplayInfoId, Recipe& out);

    /// The ChrModel a race and sex resolve to, 0 when the tables pair them with none. The unit a
    /// model's customization hangs off is the ChrModel, not the race, so this is where an enumeration
    /// starts.
    uint32_t ChrModelForRace(uint32_t chrRaceId, uint32_t sex, uint32_t form = FormDefault);
    // Every reachable choice's acquisition roots, including related-choice combinations.
    // The downloader follows M2/SKEL edges from these roots separately.
    struct CharacterAssets { std::vector<uint32_t> models, textures, skeletons; };
    bool CollectCharacterAssets(uint32_t race, uint32_t sex, CharacterAssets& assets, std::string& error);

    /// The options a ChrModel offers, in the tables' own order. Enumerated rather than reduced to
    /// fixed slots: the table that used to map the stock client's five customization bytes onto
    /// modern choices ships zero rows now, so that mapping belongs outside this resolver, and it
    /// cannot be made without seeing what the options are.
    uint32_t OptionCount(uint32_t chrModelId);
    bool     OptionAt(uint32_t chrModelId, uint32_t index, ChrOption& out);

    /// The choices one option offers, likewise ordered.
    uint32_t ChoiceCount(uint32_t chrCustomizationOptionId);
    bool     ChoiceAt(uint32_t chrCustomizationOptionId, uint32_t index, ChrChoice& out);
    // Creator eligibility only. Raw enumerations and saved source IDs remain unchanged.
    bool CreationChoiceAllowed(uint32_t chrModelId, uint32_t choiceId);

    /// The layout a ChrModel's textures are addressed in, 0 when it names none. Separate from the
    /// recipe because the sheet has to be sized before a single choice is known.
    uint32_t LayoutForModel(uint32_t chrModelId);

    /// The sheet one layout describes. False when no layout carries that id, or it states no size.
    /// A model's UVs normalise against this, so composing a sheet of any other size puts every part
    /// of the model on the wrong part of the picture, however correctly each part was painted.
    bool LayoutSize(uint32_t layoutId, uint32_t& outWidth, uint32_t& outHeight);

    /// The regions tiling one layout, in the table's own order.
    uint32_t SectionCount(uint32_t layoutId);
    bool     SectionAt(uint32_t layoutId, uint32_t index, TextureSection& out);

    /// The FileDataID of the texture a MaterialResourcesID names (TextureFileData). Prefers the file whose
    /// UsageType equals `usage`, then usage 2, then the first. 0 when none.
    uint32_t MaterialFileDataId(uint32_t materialResourcesId, uint32_t usage = 0);

    /// Display names of an option ("Skin Color", "Hair Style" ...) and of a choice, "" if unknown.
    std::string OptionName(uint32_t optionId);
    std::string ChoiceName(uint32_t choiceId);

    /// Raw table access for consumers needing columns beyond Recipe. Null until loaded, or for a
    /// name never declared. Names are the lowercase file stems ("chrmodel", "creaturedisplayinfo").
    const Table* Get(std::string_view table);
}

#endif
