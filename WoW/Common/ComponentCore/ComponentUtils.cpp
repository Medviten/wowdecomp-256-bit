#include <cstddef>
#include <cstdint>
#include "WoW/Common/ComponentCore/ComponentUtils.h"
#include "WoW/Common/ComponentCore/RetailRace.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/CharSectionsRec.h"
#include "WoW/Source/DB/Rec/CharacterFacialHairStylesRec.h"

// VERIFY: 764bd294 manual
// OFFSET: 0x004F39A0
bool ComponentFlagsMatch(uint32_t flags, COMPONENT_SELECTION selection) {
    switch (selection) {
        case COMPONENT_SELECTION_0:
            return (flags & 0x1) && !(flags & 0xC);

        case COMPONENT_SELECTION_1:
            return (flags & 0x1) && (flags & 0x14) && !(flags & 0x8);

        case COMPONENT_SELECTION_2:
            return (flags & 0x3) && !(flags & 0xC);

        case COMPONENT_SELECTION_3:
            return (flags & 0x3) && (flags & 0x14) && !(flags & 0x8);

        case COMPONENT_SELECTION_4:
            return true;

        case COMPONENT_SELECTION_5:
            return !(flags & 0xC);

        case COMPONENT_SELECTION_6:
            return (flags & 0x14) && !(flags & 0x8);

        default:
            return false;
    }
}

// VERIFY: e1c0c168 manual
// OFFSET: 0x004F3A40
COMPONENT_SELECTION GetSelectionFromContext(COMPONENT_CONTEXT context, int32_t classID) {
    switch (context) {
        case COMPONENT_CONTEXT_1:
            return static_cast<COMPONENT_SELECTION>((classID == 6) + COMPONENT_SELECTION_2);

        case COMPONENT_CONTEXT_2:
            return COMPONENT_SELECTION_4;

        case COMPONENT_CONTEXT_3:
            return static_cast<COMPONENT_SELECTION>((classID == 6) + COMPONENT_SELECTION_5);

        default:
            return static_cast<COMPONENT_SELECTION>(classID == 6);
    }
}

// VERIFY: 8a1f047c manual
// OFFSET: 0x004F3A90
COMPONENT_CONTEXT GetContextFromSelection(COMPONENT_SELECTION selection) {
    switch (selection) {
        case COMPONENT_SELECTION_2:
        case COMPONENT_SELECTION_3:
            return COMPONENT_CONTEXT_1;

        case COMPONENT_SELECTION_4:
            return COMPONENT_CONTEXT_2;

        case COMPONENT_SELECTION_5:
        case COMPONENT_SELECTION_6:
            return COMPONENT_CONTEXT_3;

        default:
            return COMPONENT_CONTEXT_0;
    }
}

// VERIFY: 28ddfb49 manual
// OFFSET: 0x004F3AE0
int32_t ComponentGetNumVariations(st_race* lookup, int32_t raceID, int32_t sexID, COMPONENT_VARIATIONS variation) {
    // Custom: a retail race reports the size of the matching retail option (RetailRace.cpp)
    const int32_t retail = RetailRaceSectionCount(raceID, sexID, variation, false);

    if (retail >= 0) {
        return retail;
    }

    CharVariationList& list = lookup[raceID * 2 + sexID].m_variation[variation];

    if (!list.m_variation) {
        return 0;
    }

    return list.m_numVariations;
}

// VERIFY: 725a4a25 auto
// OFFSET: 0x004F3B10
int32_t ComponentGetNumColors(st_race* lookup, int32_t raceID, int32_t sexID, COMPONENT_VARIATIONS variation, int32_t variationIndex) {
    // Custom: a retail race reports the size of the matching retail option (RetailRace.cpp)
    const int32_t retail = RetailRaceSectionCount(raceID, sexID, variation, true);

    if (retail >= 0) {
        return retail;
    }

    CharVariationList& list = lookup[raceID * 2 + sexID].m_variation[variation];

    if (!list.m_variation || variationIndex >= list.m_numVariations || list.m_numVariations <= 0 || !list.m_variation[variationIndex].m_color) {
        return 0;
    }

    return list.m_variation[variationIndex].m_numColors;
}

// VERIFY: 387cd93c manual
// OFFSET: 0x004F3B50
bool ComponentValidateBase(st_race* lookup, int32_t raceID, int32_t sexID, COMPONENT_VARIATIONS variation, int32_t variationIndex, int32_t colorIndex) {
    // Custom: a retail race's choices are not CharSections rows; check its row 0 instead (RetailRace.cpp)
    RetailRaceSectionArguments(raceID, sexID, &variationIndex, &colorIndex);

    if (static_cast<uint32_t>(variation) >= NUM_COMPONENT_VARIATIONS || variationIndex < 0) {
        return false;
    }

    CharVariationList& list = lookup[raceID * 2 + sexID].m_variation[variation];

    if (variationIndex >= list.m_numVariations || !list.m_numVariations || colorIndex < 0) {
        return false;
    }

    int32_t numColors = list.m_variation[variationIndex].m_numColors;

    // Custom: also require the row itself. numColors is the highest colour index + 1, so a gap in CharSections.dbc
    // (a skin with no matching face or underwear row) passed and LoadBaseVariation read through a null record.
    return colorIndex < numColors && numColors && list.m_variation[variationIndex].m_color[colorIndex];
}

// VERIFY: 04a6cba3 manual
// OFFSET: 0x004F3BA0
CharSectionsRec* ComponentGetSectionsRecord(st_race* lookup, int32_t raceID, int32_t sexID, COMPONENT_VARIATIONS variation, int32_t variationIndex, int32_t colorIndex, bool* found) {
    // Custom: a retail race's choices are not CharSections rows; read its row 0 instead (RetailRace.cpp)
    RetailRaceSectionArguments(raceID, sexID, &variationIndex, &colorIndex);

    if (static_cast<uint32_t>(variation) < NUM_COMPONENT_VARIATIONS && variationIndex >= 0) {
        CharVariationList& list = lookup[raceID * 2 + sexID].m_variation[variation];

        if (variationIndex < list.m_numVariations && list.m_numVariations && colorIndex >= 0) {
            int32_t numColors = list.m_variation[variationIndex].m_numColors;

            if (colorIndex < numColors && numColors) {
                if (found) {
                    *found = true;
                }

                return list.m_variation[variationIndex].m_color[colorIndex];
            }
        }
    }

    if (found) {
        *found = false;
    }

    return nullptr;
}

// VERIFY: e6e73c05 manual
// OFFSET: 0x004F3C10
bool ComponentValidateSkin(st_race* lookup, int32_t raceID, int32_t sexID, int32_t classID, int32_t colorIndex, COMPONENT_CONTEXT context) {
    // Custom: a retail race validates against its retail skin option (RetailRace.cpp)
    const int32_t retail = RetailRaceSectionCount(raceID, sexID, VARIATION_SKIN, true);

    if (retail >= 0) {
        return colorIndex >= 0 && colorIndex < retail;
    }

    CharVariationList& list = lookup[raceID * 2 + sexID].m_variation[VARIATION_SKIN];

    if (list.m_numVariations <= 0 || colorIndex < 0) {
        return false;
    }

    CharColorList& colors = list.m_variation[0];

    if (colorIndex >= colors.m_numColors || !colors.m_numColors || !colors.m_color[colorIndex]) {
        return false;
    }

    return ComponentFlagsMatch(colors.m_color[colorIndex]->m_flags, GetSelectionFromContext(context, classID));
}

// VERIFY: 4e4d2364 manual
// OFFSET: 0x004F3C70
bool ComponentValidateHair(st_race* lookup, int32_t raceID, int32_t sexID, int32_t classID, int32_t colorIndex, int32_t variationIndex, COMPONENT_CONTEXT context) {
    if (variationIndex < 0) {
        return false;
    }

    CharVariationList& list = lookup[raceID * 2 + sexID].m_variation[VARIATION_HAIR];

    if (variationIndex >= list.m_numVariations || !list.m_numVariations || colorIndex < 0) {
        return false;
    }

    CharColorList& colors = list.m_variation[variationIndex];

    if (colorIndex >= colors.m_numColors || !colors.m_numColors || !colors.m_color[colorIndex]) {
        return false;
    }

    return ComponentFlagsMatch(colors.m_color[colorIndex]->m_flags, GetSelectionFromContext(context, classID));
}

// VERIFY: e3e5bf4a manual
// OFFSET: 0x004F3CE0
bool ComponentValidateFace(st_race* lookup, int32_t raceID, int32_t sexID, int32_t classID, int32_t colorIndex, int32_t variationIndex, COMPONENT_CONTEXT context) {
    if (variationIndex < 0) {
        return false;
    }

    CharVariationList& list = lookup[raceID * 2 + sexID].m_variation[VARIATION_FACE];

    if (variationIndex >= list.m_numVariations || !list.m_numVariations || colorIndex < 0) {
        return false;
    }

    CharColorList& colors = list.m_variation[variationIndex];

    if (colorIndex >= colors.m_numColors || !colors.m_numColors || !colors.m_color[colorIndex]) {
        return false;
    }

    return ComponentFlagsMatch(colors.m_color[colorIndex]->m_flags, GetSelectionFromContext(context, classID));
}

// VERIFY: 9668fbd0 auto
// OFFSET: 0x004F3D50
bool ComponentValidateFacialFeature(st_race* lookup, uint32_t* facialFeatures, int32_t raceID, int32_t sexID, int32_t classID, int32_t colorIndex, int32_t variationIndex, COMPONENT_CONTEXT context) {
    if (variationIndex < 0) {
        return false;
    }

    int32_t index = raceID * 2 + sexID;

    if (static_cast<uint32_t>(variationIndex) > facialFeatures[index]) {
        return false;
    }

    CharVariationList& list = lookup[index].m_variation[VARIATION_FACIAL_HAIR];

    if (variationIndex >= list.m_numVariations || !list.m_numVariations || colorIndex < 0) {
        return true;
    }

    CharColorList& colors = list.m_variation[variationIndex];

    if (colorIndex >= colors.m_numColors || !colors.m_numColors) {
        return true;
    }

    if (!colors.m_color[colorIndex]) {
        return false;
    }

    return ComponentFlagsMatch(colors.m_color[colorIndex]->m_flags, GetSelectionFromContext(context, classID));
}

// VERIFY: 432f1f55 manual
// OFFSET: 0x004F3DD0
int32_t BuildComponentArray(uint32_t count, st_race** lookupOut) {
    if (!lookupOut) {
        return 0;
    }

    st_race* lookup = new (__FILE__, __LINE__) st_race[count];

    // Custom: the original sized these lists from contiguous runs of CharSections records, assuming the DBC is sorted
    // by race, sex, section, variation and color (Blizzard's is). With custom rows appended out of order, a race/sex/section
    // split across runs got an array sized for its last run only, and the color pass then wrote past it (heap
    // corruption that crashed on whichever race the heap layout happened to put in harm's way). These passes take the
    // maximum over all records instead, which gives the same sizes for sorted data. Records whose race is outside
    // ChrRaces or whose sex isn't 0/1 are skipped, as they would index past the lookup.
    auto getList = [lookup, count](const CharSectionsRec* rec) -> CharVariationList* {
        if (rec->m_baseSection < 0 || rec->m_baseSection >= NUM_COMPONENT_VARIATIONS || rec->m_sexID < 0 || rec->m_sexID > 1 || rec->m_raceID < 0) {
            return nullptr;
        }

        uint32_t index = static_cast<uint32_t>(rec->m_raceID) * 2 + static_cast<uint32_t>(rec->m_sexID);
        return index < count ? &lookup[index].m_variation[rec->m_baseSection] : nullptr;
    };

    // Pass 1: number of variations per race/sex/section
    for (int32_t i = 0; i < g_charSectionsDB.m_numRecords; i++) {
        CharSectionsRec* rec = &g_charSectionsDB.m_records[i];
        CharVariationList* list = getList(rec);

        if (list && rec->m_variationIndex >= 0 && rec->m_variationIndex + 1 > list->m_numVariations) {
            list->m_numVariations = rec->m_variationIndex + 1;
        }
    }

    for (uint32_t i = 0; i < count; i++) {
        for (int32_t j = 0; j < NUM_COMPONENT_VARIATIONS; j++) {
            CharVariationList& list = lookup[i].m_variation[j];

            if (list.m_numVariations > 0) {
                list.m_variation = new (__FILE__, __LINE__) CharColorList[list.m_numVariations];
            }
        }
    }

    // Pass 2: number of colors per variation
    for (int32_t i = 0; i < g_charSectionsDB.m_numRecords; i++) {
        CharSectionsRec* rec = &g_charSectionsDB.m_records[i];
        CharVariationList* list = getList(rec);

        if (!list || rec->m_variationIndex < 0 || rec->m_variationIndex >= list->m_numVariations || rec->m_colorIndex < 0) {
            continue;
        }

        CharColorList& colors = list->m_variation[rec->m_variationIndex];

        if (rec->m_colorIndex + 1 > colors.m_numColors) {
            colors.m_numColors = rec->m_colorIndex + 1;
        }
    }

    for (uint32_t i = 0; i < count; i++) {
        for (int32_t j = 0; j < NUM_COMPONENT_VARIATIONS; j++) {
            CharVariationList& list = lookup[i].m_variation[j];

            for (int32_t k = 0; k < list.m_numVariations; k++) {
                CharColorList& colors = list.m_variation[k];

                if (colors.m_numColors > 0) {
                    colors.m_color = new (__FILE__, __LINE__) CharSectionsRec*[colors.m_numColors]();
                }
            }
        }
    }

    for (int32_t i = 0; i < g_charSectionsDB.m_numRecords; i++) {
        CharSectionsRec* rec = (i < 0 || i >= g_charSectionsDB.m_numRecords) ? nullptr : &g_charSectionsDB.m_records[i];

        if (!getList(rec)) {
            continue;
        }

        int32_t recVariation = rec->m_variationIndex;
        int32_t recColor = rec->m_colorIndex;
        CharVariationList& list = *getList(rec);

        if (list.m_numVariations > 0 && recVariation < list.m_numVariations) {
            CharColorList& colors = list.m_variation[recVariation];

            if (colors.m_numColors > 0 && recColor < colors.m_numColors) {
                colors.m_color[recColor] = rec;
            }
        }
    }

    *lookupOut = lookup;

    return 1;
}

// VERIFY: 4f2a06af manual
// OFFSET: 0x004F41B0
int32_t CountFacialFeatures(uint32_t count, uint32_t** facialFeatures) {
    uint32_t* counts = static_cast<uint32_t*>(SMemAlloc(sizeof(uint32_t) * count, __FILE__, __LINE__, SMEM_FLAG_ZEROMEMORY));

    if (!counts) {
        return 0;
    }

    for (int32_t i = 0; i < g_characterFacialHairStylesDB.m_numRecords; i++) {
        CharacterFacialHairStylesRec* rec = (i < 0 || i >= g_characterFacialHairStylesDB.m_numRecords) ? nullptr : &g_characterFacialHairStylesDB.m_records[i];

        if (!rec) {
            return 0;
        }

        // Custom: skip rows whose race is outside ChrRaces or whose sex isn't 0/1 instead of writing past counts
        uint32_t index = static_cast<uint32_t>(rec->m_raceID) * 2 + static_cast<uint32_t>(rec->m_sexID);

        if (rec->m_raceID < 0 || rec->m_sexID < 0 || rec->m_sexID > 1 || index >= count) {
            continue;
        }

        counts[index]++;
    }

    *facialFeatures = counts;

    return 1;
}
