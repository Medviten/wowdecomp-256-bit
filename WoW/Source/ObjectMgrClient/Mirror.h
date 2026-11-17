#ifndef WOW_SOURCE_OBJECTMGRCLIENT_MIRROR_H
#define WOW_SOURCE_OBJECTMGRCLIENT_MIRROR_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/Container.h"
#include "WoW/Source/Object/Corpse.h"
#include "WoW/Source/Object/DynamicObject.h"
#include "WoW/Source/Object/GameObject.h"
#include "WoW/Source/Object/Item.h"
#include "WoW/Source/Object/Object.h"
#include "WoW/Source/Object/Player.h"
#include "WoW/Source/Object/Unit.h"

struct CGFieldDescriptor {
    /* 0000 */ const char* m_name;
    /* 0004 */ uint32_t m_offset;
    /* 0008 */ uint32_t m_count;
    /* 000C */ uint32_t m_type;
    /* 0010 */ uint32_t m_flags;
};

extern CGFieldDescriptor OBJECT_DISCRIPTORS[5];
extern CGFieldDescriptor ITEM_DESCRIPTORS[38];
extern CGFieldDescriptor CONTAINER_DESCRIPTORS[3];
extern CGFieldDescriptor UNIT_DESCRIPTORS[89];
extern CGFieldDescriptor PLAYER_DESCRIPTORS[216]; // Custom: was 215
extern CGFieldDescriptor GAMEOBJECT_DESCRIPTORS[8];
extern CGFieldDescriptor DYNAMICOBJECT_DESCRIPTORS[5];
extern CGFieldDescriptor CORPSE_DESCRIPTORS[10];
extern CGFieldDescriptor s_objectDescriptors[sizeof(CGObjectData) / sizeof(uint32_t)];
extern CGFieldDescriptor s_itemDescriptors[(sizeof(CGObjectData) + sizeof(CGItemData)) / sizeof(uint32_t)];
extern CGFieldDescriptor s_containerDescriptors[(sizeof(CGObjectData) + sizeof(CGItemData) + sizeof(CGContainerData)) / sizeof(uint32_t)];
extern CGFieldDescriptor s_unitDescriptors[(sizeof(CGObjectData) + sizeof(CGUnitData)) / sizeof(uint32_t)];
extern CGFieldDescriptor s_playerDescriptors[(sizeof(CGObjectData) + sizeof(CGUnitData) + sizeof(CGPlayerLocalData)) / sizeof(uint32_t)];
extern CGFieldDescriptor s_gameObjectDescriptors[(sizeof(CGObjectData) + sizeof(CGGameObjectData)) / sizeof(uint32_t)];
extern CGFieldDescriptor s_dynamicObjectDescriptors[(sizeof(CGObjectData) + sizeof(CGDynamicObjectData)) / sizeof(uint32_t)];
extern CGFieldDescriptor s_corpseDescriptors[(sizeof(CGObjectData) + sizeof(CGCorpseData)) / sizeof(uint32_t)];

void CopyAndExpandDescriptors(const CGFieldDescriptor* descriptors, CGFieldDescriptor* expanded, uint32_t count);

#endif
