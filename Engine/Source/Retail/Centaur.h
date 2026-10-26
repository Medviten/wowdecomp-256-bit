#ifndef ENGINE_SOURCE_RETAIL_CENTAUR_H
#define ENGINE_SOURCE_RETAIL_CENTAUR_H

#include "Engine/Source/Retail/Appearance.h"
#include "Engine/Source/Retail/SlimeGiant.h"
#include "Engine/Source/Retail/Compose.h"
#include <cctype>

namespace Retail::Centaur {
constexpr uint32_t Profile = 0xf0020000u, SkinOption = Profile + 1, FirstSkin = Profile + 16;
constexpr uint32_t MaleHeadOption = Profile + 2, FemaleHeadOption = Profile + 3, BeardOption = Profile + 4;
constexpr uint32_t MaleHead = Profile + 64, FemaleHead = Profile + 96, Beard = Profile + 128;
inline bool ProfileModel(uint32_t id) { return id == Profile || id == Profile + 1; }
constexpr uint32_t Bodies[] = {4036647, 4081379};
constexpr uint16_t FemaleChestWrap = 6299;
constexpr uint32_t Skins[2][16] = {
    {4036659, 4036672, 4036678, 4036679, 4036680, 4036681, 4036682, 4036683,
     4036685, 4036686, 4036687, 4036688, 4036689, 4036690, 4036691, 4036692},
    {4081391, 4081390, 4081392, 4081393, 4081394, 4081395, 4081396, 4081397,
     4081399, 4081400, 4081401, 4081402, 4081403, 4081404, 4081405, 4081406}
};
inline bool Race(uint32_t race) { return race == 73 || race == 74; }
inline bool BeltLayer(uint32_t display, uint32_t belt) { return belt && display == belt; }
inline bool CropBeltRows(Image& image) {
    if (!image.width || !image.height || image.rgba.size() != size_t(image.width) * image.height * 4) return false;
    uint32_t top = image.height, bottom = 0;
    for (uint32_t y = 0; y < image.height; ++y)
        for (uint32_t x = 0; x < image.width; ++x)
            if (image.rgba[(size_t(y) * image.width + x) * 4 + 3]) {
                top = std::min(top, y); bottom = y + 1;
            }
    if (top < bottom) {
        std::vector<uint8_t> rows(image.rgba.begin() + size_t(top) * image.width * 4,
            image.rgba.begin() + size_t(bottom) * image.width * 4);
        image.height = bottom - top;
        image.rgba.swap(rows);
    }
    return true;
}
inline int32_t EquipmentSection(uint32_t section, uint32_t display, uint32_t boots, bool hasFoot) {
    if (!boots || display != boots) return int32_t(section);
    if (section == 7 || (section == 6 && !hasFoot)) return 7;
    return -1;
}
inline bool Model(uint32_t id) { return id == Bodies[0] || id == Bodies[1]; }
inline std::string Path(uint32_t id) { return "retail/centaur/" + std::to_string(id) + ".m2"; }
inline uint32_t PathModel(const std::string& path) {
    for (uint32_t id : Bodies) if (path == Path(id)) return id;
    return 0;
}
inline uint32_t PickupSequence(const char* filename, uint32_t sequence) {
    std::string path = filename ? filename : "";
    for (char& c : path) c = c == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(c)));
    if (!PathModel(path)) return sequence;
    return sequence == 50 ? 128 : sequence == 188 ? 123 : sequence == 189 ? 129 : sequence;
}
inline uint32_t Count(uint32_t option) {
    return option == SkinOption ? 16 : option == MaleHeadOption ? 9 : option == FemaleHeadOption ? 14
        : option == BeardOption ? 8 : 0;
}
inline uint32_t VisibleHeads(uint32_t sex) { return sex ? 8 : 4; }
inline uint32_t First(uint32_t option) {
    return option == SkinOption ? FirstSkin : option == MaleHeadOption ? MaleHead
        : option == FemaleHeadOption ? FemaleHead : Beard;
}
inline uint32_t Option(uint32_t model, uint32_t index) {
    return index == 0 ? SkinOption : index == 1 ? (model == Profile ? MaleHeadOption : FemaleHeadOption)
        : model == Profile && index == 2 ? BeardOption : 0;
}
inline std::string OptionName(uint32_t option) {
    return option == SkinOption ? "Skin Color" : option == MaleHeadOption || option == FemaleHeadOption
        ? "Head Style" : option == BeardOption ? "Facial Hair" : "";
}
inline bool Choice(uint32_t option, uint32_t index, Db2::appearance::ChrChoice& out) {
    if (index >= Count(option)) return false;
    out = {First(option) + index, index, index, 0, 0};
    return true;
}
inline std::string ChoiceName(uint32_t id) {
    for (uint32_t option : {SkinOption, MaleHeadOption, FemaleHeadOption, BeardOption})
        if (id >= First(option) && id < First(option) + Count(option))
            return id == First(option) && option != SkinOption ? "None"
                : (option == SkinOption ? "Skin " : "Style ")
                    + std::to_string(id - First(option) + (option == SkinOption ? 1 : 0));
    return "";
}
inline bool Allowed(uint32_t model, uint32_t id) {
    for (uint32_t i = 0; i < (model == Profile ? 3u : 2u); ++i) {
        const uint32_t option = Option(model, i);
        if (id >= First(option) && id < First(option) + Count(option)) return true;
    }
    return false;
}
inline bool CreationAllowed(uint32_t model, uint32_t id) {
    if (!Allowed(model, id)) return false;
    const uint32_t first = model == Profile ? MaleHead : FemaleHead;
    return id < first || id >= first + Count(model == Profile ? MaleHeadOption : FemaleHeadOption)
        || id < first + VisibleHeads(model - Profile);
}
inline bool Build(uint32_t sex, const uint32_t* choices, uint32_t count, Db2::appearance::Recipe& out) {
    out.Clear();
    if (sex > 1 || count > (sex ? 2u : 3u) || (count && !choices)) return false;
    uint32_t color = 0, head = 0, beard = 0, seen = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (!Allowed(Profile + sex, choices[i])) return false;
        const uint32_t headFirst = sex ? FemaleHead : MaleHead;
        const uint32_t group = choices[i] >= Beard ? 2 : choices[i] >= headFirst ? 1 : 0;
        if (seen & (1u << group)) return false;
        seen |= 1u << group;
        if (group == 0) color = choices[i] - FirstSkin;
        else if (group == 1) head = choices[i] - headFirst;
        else beard = choices[i] - Beard;
    }
    if (head >= VisibleHeads(sex)) head = 0; // Removed helmets retain their saved byte but render as None.
    out.modelFileDataId = Bodies[sex];
    out.textureLayoutId = 141;
    out.geosets = {6701}; // Source tail; source cosmetic groups are isolated from equipment selectors.
    if (sex) {
        out.geosets.push_back(6101); // Female arms are a separate authored anatomy mesh.
        out.geosets.push_back(6201); // The female chest has an independent source selector.
        out.geosets.push_back(FemaleChestWrap); // Isolate the native wrap from source chest outfit 202 (6202).
    }
    if (head) out.geosets.push_back(uint16_t(7000 + head));
    if (beard) out.geosets.push_back(uint16_t(6100 + beard));
    out.layers.push_back({11, 0, 1, UINT32_MAX, 0, Skins[sex][color]});
    out.layers.push_back({12, 0, 1, UINT32_MAX, 0, sex ? 4081389u : 4036660u});
    return true;
}

// Use the existing checked binary helpers; keep original vertices, tracks and material slots intact.
inline bool PrepareModel(std::vector<uint8_t>& b, uint32_t id, std::string& error) {
    using namespace SlimeGiant;
    if (!Centaur::Model(id) || b.size() < 0x130 || !Array(b, 0x2c, 88) || Word(b, 0x2c) != 208 ||
        !Array(b, 0x50, 16) || Word(b, 0x50) != 4 || !Array(b, 0x68, 2) ||
        !Array(b, 0xf0, 40) || !Array(b, 0xf8, 2) || !Array(b, 0x34, 2) || Word(b, 0x34) <= 6 ||
        !Array(b, 0x1c, 64) || !Word(b, 0x1c) || Word(b, 0x1c) > 65532) {
        error = "Centaur player adapter: source layout changed"; return false;
    }
    auto output = b;
    const uint32_t sequenceCount = Word(b, 0x1c);
    std::vector<uint8_t> sequences(b.begin() + Word(b, 0x20),
        b.begin() + Word(b, 0x20) + size_t(sequenceCount) * 64);
    std::vector<uint16_t> owners(sequenceCount);
    for (uint32_t i = 0; i < sequenceCount; ++i) {
        uint32_t owner = i;
        for (uint32_t depth = 0; Word(sequences, owner * 64 + 12) & 0x40; ++depth) {
            owner = Half(sequences, owner * 64 + 62);
            if (owner >= sequenceCount || depth >= sequenceCount) {
                error = "Centaur animation alias owner is invalid"; return false;
            }
        }
        owners[i] = uint16_t(owner);
    }
    // The Wrath loader traverses a circular list, including the real clip; inherited SKEL links are not that list.
    for (uint16_t i = 0; i < sequenceCount; ++i)
        std::memcpy(sequences.data() + size_t(i) * 64 + 62, &i, 2);
    for (uint16_t i = 0; i < sequenceCount; ++i) {
        if (owners[i] == i) continue;
        const size_t link = size_t(owners[i]) * 64 + 62;
        const uint16_t next = Half(sequences, link);
        std::memcpy(sequences.data() + size_t(i) * 64 + 62, &next, 2);
        std::memcpy(sequences.data() + link, &i, 2);
    }
    Put(output, 0x20, uint32_t(output.size()));
    output.insert(output.end(), sequences.begin(), sequences.end());
    std::vector<uint8_t> textures(b.begin() + Word(b, 0x54), b.begin() + Word(b, 0x54) + 4 * 16);
    textures.resize(7 * 16, 0);
    Put(textures, 4 * 16, 14); Put(textures, 5 * 16, 2); Put(textures, 6 * 16, 15);
    Put(output, 0x50, 7); Put(output, 0x54, uint32_t(output.size()));
    output.insert(output.end(), textures.begin(), textures.end());
    std::vector<uint16_t> textureMap(std::max(16u, Word(b, 0x68)), 65535);
    for (uint32_t i = 0; i < Word(b, 0x68); ++i) textureMap[i] = Half(b, Word(b, 0x6c) + i * 2);
    textureMap[14] = 4; textureMap[2] = 5; textureMap[15] = 6;
    Put(output, 0x68, uint32_t(textureMap.size())); Put(output, 0x6c, uint32_t(output.size()));
    const auto* first = reinterpret_cast<const uint8_t*>(textureMap.data());
    output.insert(output.end(), first, first + textureMap.size() * 2);

    std::vector<uint8_t> attachments(b.begin() + Word(b, 0xf4),
        b.begin() + Word(b, 0xf4) + Word(b, 0xf0) * 40);
    std::vector<uint16_t> lookup(std::max(12u, Word(b, 0xf8)), 65535);
    for (uint32_t i = 0; i < Word(b, 0xf8); ++i) lookup[i] = Half(b, Word(b, 0xfc) + i * 2);
    const uint16_t head = Half(b, Word(b, 0x38) + 6 * 2);
    if (head >= Word(b, 0x2c)) { error = "Centaur is missing its head bone"; return false; }
    // Audited key bones 0/1 are the left/right humanoid shoulders on both Centaur rigs.
    for (uint32_t socket : {5u, 6u, 11u}) {
        if (lookup[socket] != 65535) { error = "Centaur equipment socket ownership changed"; return false; }
        const uint16_t bone = socket == 11 ? head : Half(b, Word(b, 0x38) + (socket == 5 ? 1 : 0) * 2);
        if (bone >= Word(b, 0x2c)) { error = "Centaur shoulder bone is missing"; return false; }
        lookup[socket] = uint16_t(attachments.size() / 40);
        const size_t at = attachments.size(); attachments.resize(at + 40, 0);
        Put(attachments, at, socket);
        std::memcpy(attachments.data() + at + 4, &bone, 2);
        std::memcpy(attachments.data() + at + 8, b.data() + Word(b, 0x30) + size_t(bone) * 88 + 76, 12);
        attachments[at + 22] = attachments[at + 23] = 255; // No global sequence for socket visibility.
    }
    Put(output, 0xf0, uint32_t(attachments.size() / 40)); Put(output, 0xf4, uint32_t(output.size()));
    output.insert(output.end(), attachments.begin(), attachments.end());
    Put(output, 0xf8, uint32_t(lookup.size())); Put(output, 0xfc, uint32_t(output.size()));
    first = reinterpret_cast<const uint8_t*>(lookup.data());
    output.insert(output.end(), first, first + lookup.size() * 2);
    b.swap(output);
    return true;
}
inline bool PrepareSkin(std::vector<uint8_t>& b, std::string& error, bool female = false) {
    using namespace SlimeGiant;
    if (b.size() < 48 || std::memcmp(b.data(), "SKIN", 4) || !Array(b, 0x1c, 48) || !Array(b, 0x24, 24)) {
        error = "Centaur skin adapter: invalid source mesh table"; return false;
    }
    for (uint32_t i = 0; i < Word(b, 0x1c); ++i)
        if (Half(b, Word(b, 0x20) + i * 48) > 1101) {
            error = "Centaur source geoset ownership changed"; return false;
        }
    if (female) {
        std::vector<uint8_t> batches;
        const uint32_t count = Word(b, 0x24), offset = Word(b, 0x28);
        for (uint32_t i = 0; i < count; ++i) {
            const size_t at = offset + size_t(i) * 24;
            const uint16_t section = Half(b, at + 4);
            if (section >= Word(b, 0x1c)) {
                error = "Centaur source batch section is invalid"; return false;
            }
            const uint16_t geoset = Half(b, Word(b, 0x20) + size_t(section) * 48);
            // The playable female never draws the source waist belts or quiver, regardless of visibility resets.
            if (geoset == 601 || geoset == 602 || geoset == 902) continue;
            batches.insert(batches.end(), b.begin() + at, b.begin() + at + 24);
        }
        if (batches.size() != size_t(count) * 24) {
            Put(b, 0x24, uint32_t(batches.size() / 24)); Put(b, 0x28, uint32_t(b.size()));
            b.insert(b.end(), batches.begin(), batches.end());
        }
    }
    for (uint32_t i = 0; i < Word(b, 0x1c); ++i) {
        const size_t at = Word(b, 0x20) + i * 48;
        const uint16_t source = Half(b, at);
        bool top = false;
        if (female && source == 201)
            for (uint32_t batch = 0; batch < Word(b, 0x24); ++batch) {
                const size_t offset = Word(b, 0x28) + batch * 24;
                top = top || (Half(b, offset + 4) == i && Half(b, offset + 10) == 0);
            }
        const uint16_t target = top ? FemaleChestWrap : source ? source + 6000 : 0;
        std::memcpy(b.data() + at, &target, 2);
    }
    return true;
}
}
#endif
