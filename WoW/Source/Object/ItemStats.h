#ifndef WOW_SOURCE_OBJECT_ITEMSTATS_H
#define WOW_SOURCE_OBJECT_ITEMSTATS_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/DB/RaceMask.h"

class CDataStore;

class ItemStats_C {
    public:
    // Custom: bumped from 5, AllowableRace is 256-bit (old itemcache.wdb files are discarded)
    static const uint32_t s_cacheVersion = 6;

    /* 0004 */ uint32_t m_class;
    /* 0008 */ uint32_t m_subClass;
    /* 000C */ uint32_t m_soundOverrideSubclass;
    /* 0010 */ uint32_t m_displayId;
    /* 0014 */ uint32_t m_quality;
    /* 0018 */ uint32_t m_flags [2];
    /* 0020 */ int32_t m_buyPrice;
    /* 0024 */ uint32_t m_sellPrice;
    /* 0028 */ uint32_t m_inventoryType;
    /* 002C */ uint32_t m_allowableClass;
    // Custom: 256-bit (see RaceMask.h); the offsets below are those of the stock 32-bit layout
    /* 0030 */ RaceMask m_allowableRace;
    /* 0034 */ uint32_t m_itemLevel;
    /* 0038 */ uint32_t m_requiredLevel;
    /* 003C */ uint32_t m_requiredSkill;
    /* 0040 */ uint32_t m_requiredSkillRank;
    /* 0044 */ uint32_t m_requiredSpell;
    /* 0048 */ uint32_t m_requiredHonorRank;
    /* 004C */ uint32_t m_requiredCityRank;
    /* 0050 */ uint32_t m_requiredReputationFaction;
    /* 0054 */ uint32_t m_requiredReputationRank;
    /* 0058 */ uint32_t m_maxCount;
    /* 005C */ int32_t m_stackable;
    /* 0060 */ uint32_t m_containerSlots;
    /* 0064 */ int32_t m_statsCount;
    /* 0068 */ uint32_t m_statType [10];
    /* 0090 */ uint32_t m_statValue [10];
    /* 00B8 */ uint32_t m_scalingStatDistribution;
    /* 00BC */ uint32_t m_scalingStatValue;
    /* 00C0 */ float m_damageMin [2];
    /* 00C8 */ float m_damageMax [2];
    /* 00D0 */ uint32_t m_damageType [2];
    /* 00D8 */ uint32_t m_resistances [7];
    /* 00F4 */ int32_t m_delay;
    /* 00F8 */ uint32_t m_ammoType;
    /* 00FC */ float m_rangedModRange;
    /* 0100 */ uint32_t m_spellId [5];
    /* 0114 */ uint32_t m_spellTrigger [5];
    /* 0128 */ int32_t m_spellCharges [5];
    /* 013C */ uint32_t m_spellCooldown [5];
    /* 0150 */ uint32_t m_spellCategory [5];
    /* 0164 */ uint32_t m_spellCategoryCooldown [5];
    /* 0178 */ uint32_t m_bonding;
    /* 017C */ char* m_description;
    /* 0180 */ uint32_t m_pageText;
    /* 0184 */ uint32_t m_languageId;
    /* 0188 */ uint32_t m_pageMaterial;
    /* 018C */ uint32_t m_startQuest;
    /* 0190 */ uint32_t m_lockId;
    /* 0194 */ uint32_t m_material;
    /* 0198 */ uint32_t m_sheatheType;
    /* 019C */ uint32_t m_randomProperty;
    /* 01A0 */ uint32_t m_randomSuffix;
    /* 01A4 */ uint32_t m_block;
    /* 01A8 */ uint32_t m_itemSet;
    /* 01AC */ uint32_t m_maxDurability;
    /* 01B0 */ uint32_t m_area;
    /* 01B4 */ uint32_t m_map;
    /* 01B8 */ uint32_t m_bagFamily;
    /* 01BC */ uint32_t m_totemCategory;
    /* 01C0 */ uint32_t m_socketColor [3];
    /* 01CC */ uint32_t m_socketContent [3];
    /* 01D8 */ uint32_t m_socketBonus;
    /* 01DC */ uint32_t m_gemProperties;
    /* 01E0 */ uint32_t m_requiredDisenchantSkill;
    /* 01E4 */ float m_armorDamageModifier;
    /* 01E8 */ uint32_t m_duration;
    /* 01EC */ uint32_t m_itemLimitCategory;
    /* 01F0 */ uint32_t m_holidayId;
    /* 01F4 */ char* m_name [4];

    virtual ~ItemStats_C();

    ItemStats_C();
    void Pack(CDataStore* msg);
    void Unpack(CDataStore* msg);
    const char* GetName(int32_t index) const;
};

#endif
