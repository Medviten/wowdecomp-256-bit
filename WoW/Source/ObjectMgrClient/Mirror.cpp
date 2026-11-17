#include "WoW/Source/ObjectMgrClient/Mirror.h"
#include <cstring>
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

// STATIC: 0xAC5C48
CGFieldDescriptor OBJECT_DISCRIPTORS[5] = {
    { "OBJECT_FIELD_GUID", 0, 2, 4, 0x1 },
    { "OBJECT_FIELD_TYPE", 2, 1, 1, 0x1 },
    { "OBJECT_FIELD_ENTRY", 3, 1, 1, 0x1 },
    { "OBJECT_FIELD_SCALE_X", 4, 1, 3, 0x1 },
    { "OBJECT_FIELD_PADDING", 5, 1, 1, 0x0 },
};

// STATIC: 0xAC5CE8
CGFieldDescriptor ITEM_DESCRIPTORS[38] = {
    { "ITEM_FIELD_OWNER", 0, 2, 4, 0x1 },
    { "ITEM_FIELD_CONTAINED", 2, 2, 4, 0x1 },
    { "ITEM_FIELD_CREATOR", 4, 2, 4, 0x1 },
    { "ITEM_FIELD_GIFTCREATOR", 6, 2, 4, 0x1 },
    { "ITEM_FIELD_STACK_COUNT", 8, 1, 1, 0x14 },
    { "ITEM_FIELD_DURATION", 9, 1, 1, 0x14 },
    { "ITEM_FIELD_SPELL_CHARGES", 10, 5, 1, 0x14 },
    { "ITEM_FIELD_FLAGS", 15, 1, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_1_1", 16, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_1_3", 18, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_2_1", 19, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_2_3", 21, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_3_1", 22, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_3_3", 24, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_4_1", 25, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_4_3", 27, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_5_1", 28, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_5_3", 30, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_6_1", 31, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_6_3", 33, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_7_1", 34, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_7_3", 36, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_8_1", 37, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_8_3", 39, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_9_1", 40, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_9_3", 42, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_10_1", 43, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_10_3", 45, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_11_1", 46, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_11_3", 48, 1, 2, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_12_1", 49, 2, 1, 0x1 },
    { "ITEM_FIELD_ENCHANTMENT_12_3", 51, 1, 2, 0x1 },
    { "ITEM_FIELD_PROPERTY_SEED", 52, 1, 1, 0x1 },
    { "ITEM_FIELD_RANDOM_PROPERTIES_ID", 53, 1, 1, 0x1 },
    { "ITEM_FIELD_DURABILITY", 54, 1, 1, 0x14 },
    { "ITEM_FIELD_MAXDURABILITY", 55, 1, 1, 0x14 },
    { "ITEM_FIELD_CREATE_PLAYED_TIME", 56, 1, 1, 0x1 },
    { "ITEM_FIELD_PAD", 57, 1, 1, 0x0 },
};

// STATIC: 0xAC5CAC
CGFieldDescriptor CONTAINER_DESCRIPTORS[3] = {
    { "CONTAINER_FIELD_NUM_SLOTS", 0, 1, 1, 0x1 },
    { "CONTAINER_ALIGN_PAD", 1, 1, 5, 0x0 },
    { "CONTAINER_FIELD_SLOT_1", 2, 72, 4, 0x1 },
};

// STATIC: 0xAC5FE0
CGFieldDescriptor UNIT_DESCRIPTORS[89] = {
    { "UNIT_FIELD_CHARM", 0, 2, 4, 0x1 },
    { "UNIT_FIELD_SUMMON", 2, 2, 4, 0x1 },
    { "UNIT_FIELD_CRITTER", 4, 2, 4, 0x2 },
    { "UNIT_FIELD_CHARMEDBY", 6, 2, 4, 0x1 },
    { "UNIT_FIELD_SUMMONEDBY", 8, 2, 4, 0x1 },
    { "UNIT_FIELD_CREATEDBY", 10, 2, 4, 0x1 },
    { "UNIT_FIELD_TARGET", 12, 2, 4, 0x1 },
    { "UNIT_FIELD_CHANNEL_OBJECT", 14, 2, 4, 0x1 },
    { "UNIT_CHANNEL_SPELL", 16, 1, 1, 0x1 },
    { "UNIT_FIELD_BYTES_0", 17, 1, 5, 0x1 },
    { "UNIT_FIELD_HEALTH", 18, 1, 1, 0x1 },
    { "UNIT_FIELD_POWER1", 19, 1, 1, 0x1 },
    { "UNIT_FIELD_POWER2", 20, 1, 1, 0x1 },
    { "UNIT_FIELD_POWER3", 21, 1, 1, 0x1 },
    { "UNIT_FIELD_POWER4", 22, 1, 1, 0x1 },
    { "UNIT_FIELD_POWER5", 23, 1, 1, 0x1 },
    { "UNIT_FIELD_POWER6", 24, 1, 1, 0x1 },
    { "UNIT_FIELD_POWER7", 25, 1, 1, 0x1 },
    { "UNIT_FIELD_MAXHEALTH", 26, 1, 1, 0x1 },
    { "UNIT_FIELD_MAXPOWER1", 27, 1, 1, 0x1 },
    { "UNIT_FIELD_MAXPOWER2", 28, 1, 1, 0x1 },
    { "UNIT_FIELD_MAXPOWER3", 29, 1, 1, 0x1 },
    { "UNIT_FIELD_MAXPOWER4", 30, 1, 1, 0x1 },
    { "UNIT_FIELD_MAXPOWER5", 31, 1, 1, 0x1 },
    { "UNIT_FIELD_MAXPOWER6", 32, 1, 1, 0x1 },
    { "UNIT_FIELD_MAXPOWER7", 33, 1, 1, 0x1 },
    { "UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER", 34, 7, 3, 0x6 },
    { "UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER", 41, 7, 3, 0x6 },
    { "UNIT_FIELD_LEVEL", 48, 1, 1, 0x1 },
    { "UNIT_FIELD_FACTIONTEMPLATE", 49, 1, 1, 0x1 },
    { "UNIT_VIRTUAL_ITEM_SLOT_ID", 50, 3, 1, 0x1 },
    { "UNIT_FIELD_FLAGS", 53, 1, 1, 0x1 },
    { "UNIT_FIELD_FLAGS_2", 54, 1, 1, 0x1 },
    { "UNIT_FIELD_AURASTATE", 55, 1, 1, 0x1 },
    { "UNIT_FIELD_BASEATTACKTIME", 56, 2, 1, 0x1 },
    { "UNIT_FIELD_RANGEDATTACKTIME", 58, 1, 1, 0x2 },
    { "UNIT_FIELD_BOUNDINGRADIUS", 59, 1, 3, 0x1 },
    { "UNIT_FIELD_COMBATREACH", 60, 1, 3, 0x1 },
    { "UNIT_FIELD_DISPLAYID", 61, 1, 1, 0x1 },
    { "UNIT_FIELD_NATIVEDISPLAYID", 62, 1, 1, 0x1 },
    { "UNIT_FIELD_MOUNTDISPLAYID", 63, 1, 1, 0x1 },
    { "UNIT_FIELD_MINDAMAGE", 64, 1, 3, 0x26 },
    { "UNIT_FIELD_MAXDAMAGE", 65, 1, 3, 0x26 },
    { "UNIT_FIELD_MINOFFHANDDAMAGE", 66, 1, 3, 0x26 },
    { "UNIT_FIELD_MAXOFFHANDDAMAGE", 67, 1, 3, 0x26 },
    { "UNIT_FIELD_BYTES_1", 68, 1, 5, 0x1 },
    { "UNIT_FIELD_PETNUMBER", 69, 1, 1, 0x1 },
    { "UNIT_FIELD_PET_NAME_TIMESTAMP", 70, 1, 1, 0x1 },
    { "UNIT_FIELD_PETEXPERIENCE", 71, 1, 1, 0x4 },
    { "UNIT_FIELD_PETNEXTLEVELEXP", 72, 1, 1, 0x4 },
    { "UNIT_DYNAMIC_FLAGS", 73, 1, 1, 0x100 },
    { "UNIT_MOD_CAST_SPEED", 74, 1, 3, 0x1 },
    { "UNIT_CREATED_BY_SPELL", 75, 1, 1, 0x1 },
    { "UNIT_NPC_FLAGS", 76, 1, 1, 0x100 },
    { "UNIT_NPC_EMOTESTATE", 77, 1, 1, 0x1 },
    { "UNIT_FIELD_STAT0", 78, 1, 1, 0x6 },
    { "UNIT_FIELD_STAT1", 79, 1, 1, 0x6 },
    { "UNIT_FIELD_STAT2", 80, 1, 1, 0x6 },
    { "UNIT_FIELD_STAT3", 81, 1, 1, 0x6 },
    { "UNIT_FIELD_STAT4", 82, 1, 1, 0x6 },
    { "UNIT_FIELD_POSSTAT0", 83, 1, 1, 0x6 },
    { "UNIT_FIELD_POSSTAT1", 84, 1, 1, 0x6 },
    { "UNIT_FIELD_POSSTAT2", 85, 1, 1, 0x6 },
    { "UNIT_FIELD_POSSTAT3", 86, 1, 1, 0x6 },
    { "UNIT_FIELD_POSSTAT4", 87, 1, 1, 0x6 },
    { "UNIT_FIELD_NEGSTAT0", 88, 1, 1, 0x6 },
    { "UNIT_FIELD_NEGSTAT1", 89, 1, 1, 0x6 },
    { "UNIT_FIELD_NEGSTAT2", 90, 1, 1, 0x6 },
    { "UNIT_FIELD_NEGSTAT3", 91, 1, 1, 0x6 },
    { "UNIT_FIELD_NEGSTAT4", 92, 1, 1, 0x6 },
    { "UNIT_FIELD_RESISTANCES", 93, 7, 1, 0x26 },
    { "UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE", 100, 7, 1, 0x6 },
    { "UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE", 107, 7, 1, 0x6 },
    { "UNIT_FIELD_BASE_MANA", 114, 1, 1, 0x1 },
    { "UNIT_FIELD_BASE_HEALTH", 115, 1, 1, 0x6 },
    { "UNIT_FIELD_BYTES_2", 116, 1, 5, 0x1 },
    { "UNIT_FIELD_ATTACK_POWER", 117, 1, 1, 0x6 },
    { "UNIT_FIELD_ATTACK_POWER_MODS", 118, 1, 2, 0x6 },
    { "UNIT_FIELD_ATTACK_POWER_MULTIPLIER", 119, 1, 3, 0x6 },
    { "UNIT_FIELD_RANGED_ATTACK_POWER", 120, 1, 1, 0x6 },
    { "UNIT_FIELD_RANGED_ATTACK_POWER_MODS", 121, 1, 2, 0x6 },
    { "UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER", 122, 1, 3, 0x6 },
    { "UNIT_FIELD_MINRANGEDDAMAGE", 123, 1, 3, 0x6 },
    { "UNIT_FIELD_MAXRANGEDDAMAGE", 124, 1, 3, 0x6 },
    { "UNIT_FIELD_POWER_COST_MODIFIER", 125, 7, 1, 0x6 },
    { "UNIT_FIELD_POWER_COST_MULTIPLIER", 132, 7, 3, 0x6 },
    { "UNIT_FIELD_MAXHEALTHMODIFIER", 139, 1, 3, 0x6 },
    { "UNIT_FIELD_HOVERHEIGHT", 140, 1, 3, 0x1 },
    { "UNIT_FIELD_PADDING", 141, 1, 1, 0x0 },
};

// STATIC: 0xAC66D8
CGFieldDescriptor PLAYER_DESCRIPTORS[217] = { // Custom: + PLAYER_QUEST_LOG_EXT, PLAYER_QUEST_LOG_SIZE (was 215)
    { "PLAYER_DUEL_ARBITER", 0, 2, 4, 0x1 },
    { "PLAYER_FLAGS", 2, 1, 1, 0x1 },
    { "PLAYER_GUILDID", 3, 1, 1, 0x1 },
    { "PLAYER_GUILDRANK", 4, 1, 1, 0x1 },
    { "PLAYER_BYTES", 5, 1, 5, 0x1 },
    { "PLAYER_BYTES_2", 6, 1, 5, 0x1 },
    { "PLAYER_BYTES_3", 7, 1, 5, 0x1 },
    { "PLAYER_DUEL_TEAM", 8, 1, 1, 0x1 },
    { "PLAYER_GUILD_TIMESTAMP", 9, 1, 1, 0x1 },
    { "PLAYER_QUEST_LOG_1_1", 10, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_1_2", 11, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_1_3", 12, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_1_4", 14, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_2_1", 15, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_2_2", 16, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_2_3", 17, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_2_5", 19, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_3_1", 20, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_3_2", 21, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_3_3", 22, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_3_5", 24, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_4_1", 25, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_4_2", 26, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_4_3", 27, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_4_5", 29, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_5_1", 30, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_5_2", 31, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_5_3", 32, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_5_5", 34, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_6_1", 35, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_6_2", 36, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_6_3", 37, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_6_5", 39, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_7_1", 40, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_7_2", 41, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_7_3", 42, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_7_5", 44, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_8_1", 45, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_8_2", 46, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_8_3", 47, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_8_5", 49, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_9_1", 50, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_9_2", 51, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_9_3", 52, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_9_5", 54, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_10_1", 55, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_10_2", 56, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_10_3", 57, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_10_5", 59, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_11_1", 60, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_11_2", 61, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_11_3", 62, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_11_5", 64, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_12_1", 65, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_12_2", 66, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_12_3", 67, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_12_5", 69, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_13_1", 70, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_13_2", 71, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_13_3", 72, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_13_5", 74, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_14_1", 75, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_14_2", 76, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_14_3", 77, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_14_5", 79, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_15_1", 80, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_15_2", 81, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_15_3", 82, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_15_5", 84, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_16_1", 85, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_16_2", 86, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_16_3", 87, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_16_5", 89, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_17_1", 90, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_17_2", 91, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_17_3", 92, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_17_5", 94, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_18_1", 95, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_18_2", 96, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_18_3", 97, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_18_5", 99, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_19_1", 100, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_19_2", 101, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_19_3", 102, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_19_5", 104, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_20_1", 105, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_20_2", 106, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_20_3", 107, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_20_5", 109, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_21_1", 110, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_21_2", 111, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_21_3", 112, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_21_5", 114, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_22_1", 115, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_22_2", 116, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_22_3", 117, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_22_5", 119, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_23_1", 120, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_23_2", 121, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_23_3", 122, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_23_5", 124, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_24_1", 125, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_24_2", 126, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_24_3", 127, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_24_5", 129, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_25_1", 130, 1, 1, 0x40 },
    { "PLAYER_QUEST_LOG_25_2", 131, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_25_3", 132, 2, 2, 0x2 },
    { "PLAYER_QUEST_LOG_25_5", 134, 1, 1, 0x2 },
    { "PLAYER_VISIBLE_ITEM_1_ENTRYID", 135, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_1_ENCHANTMENT", 136, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_2_ENTRYID", 137, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_2_ENCHANTMENT", 138, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_3_ENTRYID", 139, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_3_ENCHANTMENT", 140, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_4_ENTRYID", 141, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_4_ENCHANTMENT", 142, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_5_ENTRYID", 143, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_5_ENCHANTMENT", 144, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_6_ENTRYID", 145, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_6_ENCHANTMENT", 146, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_7_ENTRYID", 147, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_7_ENCHANTMENT", 148, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_8_ENTRYID", 149, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_8_ENCHANTMENT", 150, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_9_ENTRYID", 151, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_9_ENCHANTMENT", 152, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_10_ENTRYID", 153, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_10_ENCHANTMENT", 154, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_11_ENTRYID", 155, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_11_ENCHANTMENT", 156, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_12_ENTRYID", 157, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_12_ENCHANTMENT", 158, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_13_ENTRYID", 159, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_13_ENCHANTMENT", 160, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_14_ENTRYID", 161, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_14_ENCHANTMENT", 162, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_15_ENTRYID", 163, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_15_ENCHANTMENT", 164, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_16_ENTRYID", 165, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_16_ENCHANTMENT", 166, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_17_ENTRYID", 167, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_17_ENCHANTMENT", 168, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_18_ENTRYID", 169, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_18_ENCHANTMENT", 170, 1, 2, 0x1 },
    { "PLAYER_VISIBLE_ITEM_19_ENTRYID", 171, 1, 1, 0x1 },
    { "PLAYER_VISIBLE_ITEM_19_ENCHANTMENT", 172, 1, 2, 0x1 },
    { "PLAYER_CHOSEN_TITLE", 173, 1, 1, 0x1 },
    { "PLAYER_FAKE_INEBRIATION", 174, 1, 1, 0x1 },
    { "PLAYER_FIELD_PAD_0", 175, 1, 1, 0x0 },
    { "PLAYER_FIELD_INV_SLOT_HEAD", 176, 46, 4, 0x2 },
    { "PLAYER_FIELD_PACK_SLOT_1", 222, 32, 4, 0x2 },
    { "PLAYER_FIELD_BANK_SLOT_1", 254, 56, 4, 0x2 },
    { "PLAYER_FIELD_BANKBAG_SLOT_1", 310, 14, 4, 0x2 },
    { "PLAYER_FIELD_VENDORBUYBACK_SLOT_1", 324, 24, 4, 0x2 },
    { "PLAYER_FIELD_KEYRING_SLOT_1", 348, 64, 4, 0x2 },
    { "PLAYER_FIELD_CURRENCYTOKEN_SLOT_1", 412, 64, 4, 0x2 },
    { "PLAYER_FARSIGHT", 476, 2, 4, 0x2 },
    { "PLAYER__FIELD_KNOWN_TITLES", 478, 2, 4, 0x2 },
    { "PLAYER__FIELD_KNOWN_TITLES1", 480, 2, 4, 0x2 },
    { "PLAYER__FIELD_KNOWN_TITLES2", 482, 2, 4, 0x2 },
    { "PLAYER_FIELD_KNOWN_CURRENCIES", 484, 2, 4, 0x2 },
    { "PLAYER_XP", 486, 1, 1, 0x2 },
    { "PLAYER_NEXT_LEVEL_XP", 487, 1, 1, 0x2 },
    { "PLAYER_SKILL_INFO_1_1", 488, 384, 2, 0x2 },
    { "PLAYER_CHARACTER_POINTS1", 872, 1, 1, 0x2 },
    { "PLAYER_CHARACTER_POINTS2", 873, 1, 1, 0x2 },
    { "PLAYER_TRACK_CREATURES", 874, 1, 1, 0x2 },
    { "PLAYER_TRACK_RESOURCES", 875, 1, 1, 0x2 },
    { "PLAYER_BLOCK_PERCENTAGE", 876, 1, 3, 0x2 },
    { "PLAYER_DODGE_PERCENTAGE", 877, 1, 3, 0x2 },
    { "PLAYER_PARRY_PERCENTAGE", 878, 1, 3, 0x2 },
    { "PLAYER_EXPERTISE", 879, 1, 1, 0x2 },
    { "PLAYER_OFFHAND_EXPERTISE", 880, 1, 1, 0x2 },
    { "PLAYER_CRIT_PERCENTAGE", 881, 1, 3, 0x2 },
    { "PLAYER_RANGED_CRIT_PERCENTAGE", 882, 1, 3, 0x2 },
    { "PLAYER_OFFHAND_CRIT_PERCENTAGE", 883, 1, 3, 0x2 },
    { "PLAYER_SPELL_CRIT_PERCENTAGE1", 884, 7, 3, 0x2 },
    { "PLAYER_SHIELD_BLOCK", 891, 1, 1, 0x2 },
    { "PLAYER_SHIELD_BLOCK_CRIT_PERCENTAGE", 892, 1, 3, 0x2 },
    { "PLAYER_EXPLORED_ZONES_1", 893, 128, 5, 0x2 },
    { "PLAYER_REST_STATE_EXPERIENCE", 1021, 1, 1, 0x2 },
    { "PLAYER_FIELD_COINAGE", 1022, 1, 1, 0x2 },
    { "PLAYER_FIELD_MOD_DAMAGE_DONE_POS", 1023, 7, 1, 0x2 },
    { "PLAYER_FIELD_MOD_DAMAGE_DONE_NEG", 1030, 7, 1, 0x2 },
    { "PLAYER_FIELD_MOD_DAMAGE_DONE_PCT", 1037, 7, 1, 0x2 },
    { "PLAYER_FIELD_MOD_HEALING_DONE_POS", 1044, 1, 1, 0x2 },
    { "PLAYER_FIELD_MOD_HEALING_PCT", 1045, 1, 3, 0x2 },
    { "PLAYER_FIELD_MOD_HEALING_DONE_PCT", 1046, 1, 3, 0x2 },
    { "PLAYER_FIELD_MOD_TARGET_RESISTANCE", 1047, 1, 1, 0x2 },
    { "PLAYER_FIELD_MOD_TARGET_PHYSICAL_RESISTANCE", 1048, 1, 1, 0x2 },
    { "PLAYER_FIELD_BYTES", 1049, 1, 5, 0x2 },
    { "PLAYER_AMMO_ID", 1050, 1, 1, 0x2 },
    { "PLAYER_SELF_RES_SPELL", 1051, 1, 1, 0x2 },
    { "PLAYER_FIELD_PVP_MEDALS", 1052, 1, 1, 0x2 },
    { "PLAYER_FIELD_BUYBACK_PRICE_1", 1053, 12, 1, 0x2 },
    { "PLAYER_FIELD_BUYBACK_TIMESTAMP_1", 1065, 12, 1, 0x2 },
    { "PLAYER_FIELD_KILLS", 1077, 1, 2, 0x2 },
    { "PLAYER_FIELD_TODAY_CONTRIBUTION", 1078, 1, 1, 0x2 },
    { "PLAYER_FIELD_YESTERDAY_CONTRIBUTION", 1079, 1, 1, 0x2 },
    { "PLAYER_FIELD_LIFETIME_HONORBALE_KILLS", 1080, 1, 1, 0x2 },
    { "PLAYER_FIELD_BYTES2", 1081, 1, 6, 0x2 },
    { "PLAYER_FIELD_WATCHED_FACTION_INDEX", 1082, 1, 1, 0x2 },
    { "PLAYER_FIELD_COMBAT_RATING_1", 1083, 25, 1, 0x2 },
    { "PLAYER_FIELD_ARENA_TEAM_INFO_1_1", 1108, 21, 1, 0x2 },
    { "PLAYER_FIELD_HONOR_CURRENCY", 1129, 1, 1, 0x2 },
    { "PLAYER_FIELD_ARENA_CURRENCY", 1130, 1, 1, 0x2 },
    { "PLAYER_FIELD_MAX_LEVEL", 1131, 1, 1, 0x2 },
    { "PLAYER_FIELD_DAILY_QUESTS_1", 1132, 25, 1, 0x2 },
    { "PLAYER_RUNE_REGEN_1", 1157, 4, 3, 0x2 },
    { "PLAYER_NO_REAGENT_COST_1", 1161, 3, 1, 0x2 },
    { "PLAYER_FIELD_GLYPH_SLOTS_1", 1164, 6, 1, 0x2 },
    { "PLAYER_FIELD_GLYPHS_1", 1170, 6, 1, 0x2 },
    { "PLAYER_GLYPHS_ENABLED", 1176, 1, 1, 0x2 },
    { "PLAYER_PET_SPELL_POWER", 1177, 1, 1, 0x2 },
    { "PLAYER_QUEST_LOG_EXT", 1178, PLAYER_QUEST_LOG_EXT_FIELDS, 1, 0x2 }, // Custom: quest log slots 25-254
    { "PLAYER_QUEST_LOG_SIZE", 1178 + PLAYER_QUEST_LOG_EXT_FIELDS, 1, 1, 0x2 }, // Custom: Quests.LogSize
};

// STATIC: 0xAC77A8
CGFieldDescriptor GAMEOBJECT_DESCRIPTORS[8] = {
    { "OBJECT_FIELD_CREATED_BY", 0, 2, 4, 0x1 },
    { "GAMEOBJECT_DISPLAYID", 2, 1, 1, 0x1 },
    { "GAMEOBJECT_FLAGS", 3, 1, 1, 0x1 },
    { "GAMEOBJECT_PARENTROTATION", 4, 4, 3, 0x1 },
    { "GAMEOBJECT_DYNAMIC", 8, 1, 2, 0x100 },
    { "GAMEOBJECT_FACTION", 9, 1, 1, 0x1 },
    { "GAMEOBJECT_LEVEL", 10, 1, 1, 0x1 },
    { "GAMEOBJECT_BYTES_1", 11, 1, 5, 0x1 },
};

// STATIC: 0xAC7848
CGFieldDescriptor DYNAMICOBJECT_DESCRIPTORS[5] = {
    { "DYNAMICOBJECT_CASTER", 0, 2, 4, 0x1 },
    { "DYNAMICOBJECT_BYTES", 2, 1, 5, 0x1 },
    { "DYNAMICOBJECT_SPELLID", 3, 1, 1, 0x1 },
    { "DYNAMICOBJECT_RADIUS", 4, 1, 3, 0x1 },
    { "DYNAMICOBJECT_CASTTIME", 5, 1, 1, 0x1 },
};

// STATIC: 0xAC78B0
CGFieldDescriptor CORPSE_DESCRIPTORS[10] = {
    { "CORPSE_FIELD_OWNER", 0, 2, 4, 0x1 },
    { "CORPSE_FIELD_PARTY", 2, 2, 4, 0x1 },
    { "CORPSE_FIELD_DISPLAY_ID", 4, 1, 1, 0x1 },
    { "CORPSE_FIELD_ITEM", 5, 19, 1, 0x1 },
    { "CORPSE_FIELD_BYTES_1", 24, 1, 5, 0x1 },
    { "CORPSE_FIELD_BYTES_2", 25, 1, 5, 0x1 },
    { "CORPSE_FIELD_GUILD", 26, 1, 1, 0x1 },
    { "CORPSE_FIELD_FLAGS", 27, 1, 1, 0x1 },
    { "CORPSE_FIELD_DYNAMIC_FLAGS", 28, 1, 1, 0x100 },
    { "CORPSE_FIELD_PAD", 29, 1, 1, 0x0 },
};

// STATIC: 0x00B6BAC0
CGFieldDescriptor s_corpseDescriptors[(sizeof(CGObjectData) + sizeof(CGCorpseData)) / sizeof(uint32_t)];

// STATIC: 0x00B6BD90
CGFieldDescriptor s_dynamicObjectDescriptors[(sizeof(CGObjectData) + sizeof(CGDynamicObjectData)) / sizeof(uint32_t)];

// STATIC: 0x00B6BE80
CGFieldDescriptor s_gameObjectDescriptors[(sizeof(CGObjectData) + sizeof(CGGameObjectData)) / sizeof(uint32_t)];

// STATIC: 0x00B6BFE8
CGFieldDescriptor s_playerDescriptors[(sizeof(CGObjectData) + sizeof(CGUnitData) + sizeof(CGPlayerLocalData)) / sizeof(uint32_t)];

// STATIC: 0x00B72780
CGFieldDescriptor s_unitDescriptors[(sizeof(CGObjectData) + sizeof(CGUnitData)) / sizeof(uint32_t)];

// STATIC: 0x00B73310
CGFieldDescriptor s_containerDescriptors[(sizeof(CGObjectData) + sizeof(CGItemData) + sizeof(CGContainerData)) / sizeof(uint32_t)];

// STATIC: 0x00B73DD8
CGFieldDescriptor s_itemDescriptors[(sizeof(CGObjectData) + sizeof(CGItemData)) / sizeof(uint32_t)];

// STATIC: 0x00B742D8
CGFieldDescriptor s_objectDescriptors[sizeof(CGObjectData) / sizeof(uint32_t)];

// VERIFY: 1619cd13 manual
// OFFSET: 0x004F56C0
void CopyAndExpandDescriptors(const CGFieldDescriptor* descriptors, CGFieldDescriptor* expanded, uint32_t count) {
    uint32_t index = 0;

    for (uint32_t i = 0; i < count; i++) {
        const CGFieldDescriptor& descriptor = descriptors[i];

        for (uint32_t j = 0; j < descriptor.m_count; j++) {
            expanded[index + j] = descriptor;
        }

        index += descriptor.m_count;
    }
}

// VERIFY: ec13bbe4 auto
// OFFSET: 0x004F5720
void MirrorInitialize() {
    CopyAndExpandDescriptors(OBJECT_DISCRIPTORS, s_objectDescriptors, sizeof(OBJECT_DISCRIPTORS) / sizeof(OBJECT_DISCRIPTORS[0]));
    memcpy(s_itemDescriptors, s_objectDescriptors, sizeof(s_objectDescriptors));
    CopyAndExpandDescriptors(ITEM_DESCRIPTORS, &s_itemDescriptors[sizeof(s_objectDescriptors) / sizeof(s_objectDescriptors[0])], sizeof(ITEM_DESCRIPTORS) / sizeof(ITEM_DESCRIPTORS[0]));
    memcpy(s_containerDescriptors, s_itemDescriptors, sizeof(s_itemDescriptors));
    CopyAndExpandDescriptors(CONTAINER_DESCRIPTORS, &s_containerDescriptors[sizeof(s_itemDescriptors) / sizeof(s_itemDescriptors[0])], sizeof(CONTAINER_DESCRIPTORS) / sizeof(CONTAINER_DESCRIPTORS[0]));
    memcpy(s_unitDescriptors, s_objectDescriptors, sizeof(s_objectDescriptors));
    CopyAndExpandDescriptors(UNIT_DESCRIPTORS, &s_unitDescriptors[sizeof(s_objectDescriptors) / sizeof(s_objectDescriptors[0])], sizeof(UNIT_DESCRIPTORS) / sizeof(UNIT_DESCRIPTORS[0]));
    memcpy(s_playerDescriptors, s_unitDescriptors, sizeof(s_unitDescriptors));
    CopyAndExpandDescriptors(PLAYER_DESCRIPTORS, &s_playerDescriptors[sizeof(s_unitDescriptors) / sizeof(s_unitDescriptors[0])], sizeof(PLAYER_DESCRIPTORS) / sizeof(PLAYER_DESCRIPTORS[0]));
    memcpy(s_gameObjectDescriptors, s_objectDescriptors, sizeof(s_objectDescriptors));
    CopyAndExpandDescriptors(GAMEOBJECT_DESCRIPTORS, &s_gameObjectDescriptors[sizeof(s_objectDescriptors) / sizeof(s_objectDescriptors[0])], sizeof(GAMEOBJECT_DESCRIPTORS) / sizeof(GAMEOBJECT_DESCRIPTORS[0]));
    memcpy(s_dynamicObjectDescriptors, s_objectDescriptors, sizeof(s_objectDescriptors));
    CopyAndExpandDescriptors(DYNAMICOBJECT_DESCRIPTORS, &s_dynamicObjectDescriptors[sizeof(s_objectDescriptors) / sizeof(s_objectDescriptors[0])], sizeof(DYNAMICOBJECT_DESCRIPTORS) / sizeof(DYNAMICOBJECT_DESCRIPTORS[0]));
    memcpy(s_corpseDescriptors, s_objectDescriptors, sizeof(s_objectDescriptors));
    CopyAndExpandDescriptors(CORPSE_DESCRIPTORS, &s_corpseDescriptors[sizeof(s_objectDescriptors) / sizeof(s_objectDescriptors[0])], sizeof(CORPSE_DESCRIPTORS) / sizeof(CORPSE_DESCRIPTORS[0]));
}
