#ifndef ENGINE_SOURCE_RETAIL_SLIMEGIANT_H
#define ENGINE_SOURCE_RETAIL_SLIMEGIANT_H

#include "Engine/Source/Retail/Appearance.h"
#include <algorithm>
#include <cstring>

namespace Retail::SlimeGiant {
// Private profile IDs are outside Blizzard's DB2 identifiers.
constexpr uint32_t Profile = 0xf0010000u;
constexpr uint32_t ColorOption = Profile + 1, DripOption = Profile + 2;
constexpr uint32_t Green = Profile + 16, Amber = Profile + 17, Mauve = Profile + 18;
constexpr uint32_t Red = Profile + 19, Gold = Profile + 20;
constexpr uint32_t Off = Profile + 32, On = Profile + 33;
constexpr uint32_t ExtraColor = Profile + 256, LocalTexture = Profile + 4096;
constexpr uint32_t OriginalColors = 5, ExtraColors = 30, Colors = OriginalColors + ExtraColors;
inline constexpr const char* ExtraColorNames[ExtraColors] = {
    "Crimson", "Scarlet", "Ember Orange", "Peach", "Amber (Custom)", "Lemon", "Chartreuse", "Lime",
    "Toxic Green", "Emerald", "Jade", "Mint", "Teal", "Turquoise", "Ice Blue", "Sky Blue", "Azure",
    "Cobalt", "Indigo", "Violet", "Lilac", "Magenta", "Hot Pink", "Rose", "Wine", "Bronze", "Copper",
    "Pearl", "Silver", "Charcoal"
};
constexpr uint32_t Body = 3490792;
constexpr uint16_t DripGeoset = 3101;
inline bool Race(uint32_t race) { return race == 71 || race == 72; }
inline bool Model(uint32_t id) { return id == Body; }
inline std::string Path(uint32_t id) { return "retail/slimegiant/" + std::to_string(id) + ".m2"; }
inline uint32_t PathModel(const std::string& path) { return path == Path(Body) ? Body : 0; }
inline uint32_t Count(uint32_t option) { return option == ColorOption ? Colors : option == DripOption ? 2 : 0; }
inline uint32_t ColorId(uint32_t index) {
    return index < OriginalColors ? Green + index : ExtraColor + index - OriginalColors;
}
inline bool ColorIndex(uint32_t id, uint32_t& index) {
    if (id >= Green && id <= Gold) { index = id - Green; return true; }
    if (id >= ExtraColor && id < ExtraColor + ExtraColors) { index = OriginalColors + id - ExtraColor; return true; }
    return false;
}
inline std::string TexturePath(uint32_t id) {
    return id >= LocalTexture && id < LocalTexture + ExtraColors * 2
        ? "custom/slimegiant/colors/" + std::to_string(id - LocalTexture) + ".blp" : std::string{};
}
inline bool Choice(uint32_t option, uint32_t index, Db2::appearance::ChrChoice& out) {
    if (index >= Count(option)) return false;
    out = {option == ColorOption ? ColorId(index) : Off + index, index, index, 0, 0};
    return true;
}
inline std::string ChoiceName(uint32_t id) {
    if (id >= ExtraColor && id < ExtraColor + ExtraColors) return ExtraColorNames[id - ExtraColor];
    switch (id) {
        case Green: return "Green"; case Amber: return "Amber"; case Mauve: return "Mauve";
        case Red: return "Red"; case Gold: return "Gold";
        case Off: return "Off"; case On: return "On"; default: return {};
    }
}
inline bool Build(uint32_t sex, const uint32_t* choices, uint32_t count, Db2::appearance::Recipe& out) {
    out.Clear();
    if (sex || count > 2 || (count && !choices)) return false;
    uint32_t color = 0;
    bool drip = false, hasColor = false, hasDrip = false;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t index = 0;
        if (ColorIndex(choices[i], index) && !hasColor) { color = index; hasColor = true; }
        else if ((choices[i] == Off || choices[i] == On) && !hasDrip) { drip = choices[i] == On; hasDrip = true; }
        else return false;
    }
    out.modelFileDataId = Body;
    out.textureLayoutId = 141; // Kul Tiran male's authored HD armor atlas.
    if (drip) out.geosets.push_back(DripGeoset);
    const uint32_t texture = color < OriginalColors ? 3491663 + 2 * color : LocalTexture + 2 * (color - OriginalColors);
    for (uint32_t i = 0; i < 2; ++i)
        out.layers.push_back({11 + i, 0, 1, UINT32_MAX, 0, texture + i});
    return true;
}

inline uint32_t Word(const std::vector<uint8_t>& b, size_t at) {
    uint32_t value; std::memcpy(&value, b.data() + at, 4); return value;
}
inline uint16_t Half(const std::vector<uint8_t>& b, size_t at) {
    uint16_t value; std::memcpy(&value, b.data() + at, 2); return value;
}
inline void Put(std::vector<uint8_t>& b, size_t at, uint32_t value) {
    std::memcpy(b.data() + at, &value, 4);
}
inline bool Array(const std::vector<uint8_t>& b, size_t at, size_t stride) {
    return at <= b.size() && b.size() - at >= 8 && Word(b, at + 4) <= b.size() &&
        Word(b, at) <= (b.size() - Word(b, at + 4)) / stride;
}

// Preserve the authored Kul Tiran-compatible rig, sockets, animations and all original texture slots.
inline bool PrepareModel(std::vector<uint8_t>& b, uint32_t id, std::string& error) {
    if (!Model(id) || b.size() < 0x130 || !Array(b, 0x2c, 88) || Word(b, 0x2c) != 208 ||
        !Array(b, 0x50, 16) || Word(b, 0x50) != 3 || !Array(b, 0x68, 2)) {
        error = "SlimeSkeleton player adapter: source layout changed"; return false;
    }
    auto output = b;
    std::vector<uint8_t> textures(b.begin() + Word(b, 0x54), b.begin() + Word(b, 0x54) + 3 * 16);
    if (Word(textures, 0) != 11 || Word(textures, 32) != 12) {
        error = "SlimeSkeleton player adapter: source palette slots changed"; return false;
    }
    textures.resize(5 * 16, 0);
    Put(textures, 3 * 16, 14); // Transparent armor sheet, independent of skeleton/slime colors.
    Put(textures, 4 * 16, 2);  // Existing Wrath cloak binding, forwarded to the Kul Tiran cape collection.
    Put(output, 0x50, 5); Put(output, 0x54, uint32_t(output.size()));
    output.insert(output.end(), textures.begin(), textures.end());
    std::vector<uint16_t> textureMap(std::max(16u, Word(b, 0x68)), 65535);
    for (uint32_t i = 0; i < Word(b, 0x68); ++i) textureMap[i] = Half(b, Word(b, 0x6c) + i * 2);
    textureMap[11] = 0; textureMap[12] = 2; textureMap[14] = 3; textureMap[2] = 4;
    Put(output, 0x68, uint32_t(textureMap.size())); Put(output, 0x6c, uint32_t(output.size()));
    const auto* first = reinterpret_cast<const uint8_t*>(textureMap.data());
    output.insert(output.end(), first, first + textureMap.size() * 2);
    b.swap(output);
    return true;
}
inline bool PrepareSkin(std::vector<uint8_t>& b, std::string& error) {
    if (b.size() < 48 || std::memcmp(b.data(), "SKIN", 4) || !Array(b, 0x1c, 48) || Word(b, 0x1c) != 4) {
        error = "SlimeSkeleton player adapter: source skin layout changed"; return false;
    }
    for (uint32_t i = 0; i < 4; ++i) if (Half(b, Word(b, 0x20) + i * 48) != 0) {
        error = "SlimeSkeleton player adapter: unexpected source geoset"; return false;
    }
    const size_t at = Word(b, 0x20) + 3 * 48;
    b[at] = uint8_t(DripGeoset); b[at + 1] = uint8_t(DripGeoset >> 8);
    return true;
}
}
#endif
