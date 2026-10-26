#ifndef ENGINE_SOURCE_RETAIL_EQUIPMENT_H
#define ENGINE_SOURCE_RETAIL_EQUIPMENT_H

#include <cstdint>
#include <vector>

namespace Retail {

// Packed quaternion zero is halfway through the unsigned range. Accept its two quantized
// representations and either sign of unit W; a rotating socket needs a different fitting method.
inline bool IsIdentityHelmetRotation(const uint16_t* quaternion) {
    for (std::size_t axis = 0; axis < 3; ++axis)
        if (quaternion[axis] != 32767 && quaternion[axis] != 32768) return false;
    return quaternion[3] == 0 || quaternion[3] == 65535;
}

struct EquipmentLayer {
    uint32_t section;
    uint32_t layer;
    uint32_t displayId;
    void* texture;
};

// Match the stock section compositor's ordering and robe exclusions. Item IDs and masks survive
// DestroyRenderTextures; a null temporary handle does not mean that the item was unequipped.
template <class Section>
std::vector<EquipmentLayer> CollectEquipmentLayers(const Section* sections, bool robe) {
    constexpr uint32_t counts[] = {2, 7, 1, 5, 7, 3, 6, 1};
    std::vector<EquipmentLayer> layers;
    for (uint32_t section = 0; section < 8; ++section) {
        const uint32_t first = robe && (section == 5 || section == 6) ? 1 : 0;
        const uint32_t last = robe && section == 5 ? 2 : robe && section == 6 ? 4 : counts[section];
        for (uint32_t layer = first; layer < last; ++layer) {
            const auto& data = sections[section];
            if ((data.m_layerMask & (1u << layer)) && (data.m_layerItemDisplayId[layer] || data.m_layerTex[layer])) {
                layers.push_back({section, layer, data.m_layerItemDisplayId[layer], data.m_layerTex[layer]});
            }
        }
    }
    return layers;
}

}
#endif
