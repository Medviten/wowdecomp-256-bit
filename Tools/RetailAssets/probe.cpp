#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Retail/Equipment.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cmath>

int main(int argc, char** argv) {
    const bool character = argc > 3 && std::string(argv[3]) == "--character";
    if (character) {
        for (int i = 3; i + 1 < argc; ++i) argv[i] = argv[i + 1];
        --argc;
    }
    if (argc != 3 && argc != 4 && argc != 5) {
        std::fprintf(stderr, "Usage: RetailAssetProbe <RetailCasc.ini> <retail/ID.m2|alias> [--inspect|--extract new-file|--helmet-donor legacy.m2|--animation ID[:variation]]\n");
        return 2;
    }
    const bool extract = argc == 5 && std::string(argv[3]) == "--extract";
    const bool helmet = argc == 5 && std::string(argv[3]) == "--helmet-donor";
    const bool emotes = argc == 4 && std::string(argv[3]) == "--emotes";
    const bool animation = emotes || (argc == 5 && std::string(argv[3]) == "--animation");
    const bool skinParent = argc == 5 && std::string(argv[3]) == "--skin-parent";
    const bool inspect = extract || (argc == 4 && std::string(argv[3]) == "--inspect");
    if (argc > 3 && !inspect && !helmet && !animation && !skinParent) return 2;
    const auto kind = !std::strncmp(argv[2], "forever/", 8) ? Retail::SourceKind::Forever : Retail::SourceKind::Retail;
    Retail::SourceScope scope(kind);
    Retail::CascSource source(kind);
    if (character && Retail::IsCdnPath(argv[2]))
        source.RegisterCharacterModel(uint32_t(std::strtoul(argv[2] + std::strlen(Retail::SourcePrefix()), nullptr, 10)));
    std::vector<uint8_t> bytes;
    std::string error;
    if (!source.Configure(argv[1], error) || !(inspect ? source.Inspect(argv[2], bytes, error) : source.Read(argv[2], bytes, error))) {
        std::fprintf(stderr, "%s: %s\n", argv[2], error.c_str());
        return 1;
    }
    if (extract) {
        std::ifstream existing(argv[4], std::ios::binary);
        if (existing.good()) { std::fprintf(stderr, "Extraction destination already exists\n"); return 1; }
        std::ofstream output(argv[4], std::ios::binary);
        if (!output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size())) return 1;
    }
    if (skinParent) {
        if (!std::strncmp(argv[4], "retail/", 7)) source.RegisterCharacterModel(uint32_t(std::strtoul(argv[4] + 7, nullptr, 10)));
        std::vector<uint8_t> parent;
        if (!source.Read(argv[4], parent, error)) { std::fprintf(stderr, "Parent: %s\n", error.c_str()); return 1; }
        const auto word = [](const std::vector<uint8_t>& data, size_t at) {
            uint32_t value; std::memcpy(&value, data.data() + at, 4); return value;
        };
        const auto cn = word(bytes, 0x2c), co = word(bytes, 0x30), pn = word(parent, 0x2c), po = word(parent, 0x30);
        bool same = cn == pn;
        for (uint32_t i = 0; same && i < cn; ++i) {
            const size_t a = co + i * 88, b = po + i * 88;
            same = !std::memcmp(bytes.data() + a, parent.data() + b, 16)
                && !std::memcmp(bytes.data() + a + 76, parent.data() + b + 76, 12);
        }
        bool mapped = same;
        if (!same) {
            std::map<uint32_t, uint32_t> names;
            mapped = true;
            for (uint32_t i = 0; i < pn; ++i) {
                const auto crc = word(parent, po + size_t(i) * 88 + 12);
                if (crc && !names.emplace(crc, i).second) mapped = false;
            }
            std::vector<uint32_t> bones;
            for (uint32_t i = 0; mapped && i < cn; ++i) {
                const auto crc = word(bytes, co + size_t(i) * 88 + 12);
                const auto match = names.find(crc);
                if (!crc || match == names.end()) { std::printf("Unmatched collection bone %u CRC %u\n", i, crc); mapped = false; break; }
                bones.push_back(match->second);
            }
            for (uint32_t i = 0; mapped && i < cn; ++i) {
                const uint32_t ancestor = word(bytes, co + size_t(i) * 88 + 8) & 65535;
                if (ancestor != 65535) {
                    const uint32_t expected = bones.at(ancestor);
                    uint32_t actual = word(parent, po + size_t(bones[i]) * 88 + 8) & 65535, depth = 0;
                    while (actual != expected && actual != 65535 && depth++ < pn)
                        actual = word(parent, po + size_t(actual) * 88 + 8) & 65535;
                    if (actual != expected) mapped = false;
                }
            }
        }
        std::printf("Collection rig: %u child bones, %u parent bones; identical bind structure=%d; compatible mapped rig=%d\n", cn, pn, same, mapped);
        if (!mapped) return 1;
    }
    if (helmet) {
        std::ifstream input(argv[4], std::ios::binary);
        const std::vector<uint8_t> legacy((std::istreambuf_iterator<char>(input)), {});
        Retail::HelmetAnchor donor, target;
        if (!Retail::ReadHelmetAnchor(legacy, donor, error) || !Retail::ReadHelmetAnchor(bytes, target, error)) {
            std::fprintf(stderr, "Helmet anchor: %s\n", error.c_str());
            return 1;
        }
        const auto shift = Retail::HelmetTranslation(donor, target);
        const auto word = [&](size_t at) { uint32_t v; std::memcpy(&v, bytes.data() + at, 4); return v; };
        const auto half = [&](size_t at) { uint16_t v; std::memcpy(&v, bytes.data() + at, 2); return v; };
        const uint16_t head = half(word(0x38) + 12), socket = half(word(0xfc) + 22);
        const uint16_t socketBone = half(word(0xf4) + size_t(socket) * 40 + 4);
        const size_t bone = word(0x30) + size_t(socketBone) * 88;
        const auto populated = [&](size_t track) {
            for (uint32_t i = 0; i < word(track + 4); ++i)
                if (word(word(track + 8) + size_t(i) * 8)) return true;
            return false;
        };
        bool frame = socketBone == head;
        if (!frame && half(bone + 8) == head && !populated(bone + 16) && !populated(bone + 56)) {
            const size_t track = bone + 36;
            frame = !populated(track);
            if (!frame && half(track + 2) != 65535 && half(track) <= 1 && word(track + 12)) {
                frame = true;
                for (uint32_t i = 0; frame && i < word(track + 12); ++i) {
                    const size_t pair = word(track + 16) + size_t(i) * 8;
                    for (uint32_t k = 0; frame && k < word(pair); ++k) {
                        uint16_t q[4]; std::memcpy(q, bytes.data() + word(pair + 4) + size_t(k) * 8, 8);
                        frame = Retail::IsIdentityHelmetRotation(q);
                    }
                }
            }
        }
        std::printf("Helmet socket uses supported head frame=%d (head %u, socket bone %u)\n", frame, head, socketBone);
        if (!frame) {
            uint16_t current = socketBone;
            for (uint32_t depth = 0; depth < 6 && current != head && current != 65535; ++depth) {
                const size_t p = word(0x30) + size_t(current) * 88;
                std::printf("Helper %u parent %u\n", current, half(p + 8));
                for (uint32_t t = 0; t < 3; ++t) {
                    const size_t track = p + 16 + t * 20;
                    std::printf("  track %u type %u loop %u populated %d\n", t, half(track), half(track + 2), populated(track));
                    if (half(track + 2) == 65535) continue;
                    for (uint32_t s = 0; s < word(track + 12); ++s) {
                        const size_t pair = word(track + 16) + size_t(s) * 8;
                        if (!word(pair)) continue;
                        if (t == 1) {
                            uint16_t q[4]; std::memcpy(q, bytes.data() + word(pair + 4), 8);
                            std::printf("    %u keys; first rotation %u %u %u %u\n", word(pair), q[0], q[1], q[2], q[3]);
                        } else {
                            float v[3]; std::memcpy(v, bytes.data() + word(pair + 4), 12);
                            std::printf("    %u keys; first vector %.8f %.8f %.8f\n", word(pair), v[0], v[1], v[2]);
                        }
                    }
                }
                current = half(p + 8);
            }
            std::printf("Retail-specific socket transform bypassed by legacy head-frame binding\n");
        }
        std::printf("Helmet donor head %.6f %.6f %.6f socket %.6f %.6f %.6f\n",
            donor.head[0], donor.head[1], donor.head[2], donor.socket[0], donor.socket[1], donor.socket[2]);
        std::printf("Retail head %.6f %.6f %.6f socket %.6f %.6f %.6f\n",
            target.head[0], target.head[1], target.head[2], target.socket[0], target.socket[1], target.socket[2]);
        std::printf("Helmet local correction %.6f %.6f %.6f\n", shift[0], shift[1], shift[2]);
        std::printf("Legacy head-frame position %.6f %.6f %.6f\n",
            target.head[0] + donor.socket[0] - donor.head[0],
            target.head[1] + donor.socket[1] - donor.head[1],
            target.head[2] + donor.socket[2] - donor.head[2]);
    }
    if (animation) {
        std::string requests;
        if (emotes) {
            const auto word = [&](size_t at) { uint32_t v; std::memcpy(&v, bytes.data() + at, 4); return v; };
            const uint32_t count = word(0x1c), offset = word(0x20);
            for (uint32_t i = 0; i < count; ++i) {
                const size_t p = offset + size_t(i) * 64;
                const uint32_t identity = word(p), id = identity & 65535, variation = identity >> 16;
                if ((id != 60 && id != 69 && id != 70 && id != 123) || (word(p + 12) & (0x20 | 0x40))) continue;
                if (!requests.empty()) requests += ',';
                requests += std::to_string(id) + ':' + std::to_string(variation);
            }
            if (requests.empty()) std::printf("Emotes are embedded or aliases; no external clips to fetch\n");
        }
        for (const char* cursor = emotes ? requests.c_str() : argv[4]; *cursor;) {
            char* end = nullptr;
            const unsigned long id = std::strtoul(cursor, &end, 10);
            if (end == cursor || id > 65535) return 2;
            unsigned long variation = 0;
            if (*end == ':') {
                cursor = end + 1;
                variation = std::strtoul(cursor, &end, 10);
                if (end == cursor || variation > 65535) return 2;
            }
            if (*end && *end != ',') return 2;
            cursor = *end ? end + 1 : end;
            std::string key = argv[2];
            key.resize(key.find_last_of('.'));
            char suffix[40];
            std::snprintf(suffix, sizeof(suffix), "%04lu-%02lu.anim", id, variation);
            key += suffix;
            std::vector<uint8_t> anim;
            if (!source.Read(key.c_str(), anim, error)) {
                std::fprintf(stderr, "%s: %s\n", key.c_str(), error.c_str());
                return 1;
            }
            Retail::AnimationLayout layout;
            if (!Retail::InspectAnimation(anim.data(), anim.size(), layout, error)) return 1;
            auto word = [](const std::vector<uint8_t>& data, size_t at) {
                uint32_t value; std::memcpy(&value, data.data() + at, 4); return value;
            };
            const size_t count = word(bytes, 0x1c), sequences = word(bytes, 0x20);
            size_t sequence = count;
            for (size_t i = 0; i < count; ++i) {
                if (word(bytes, sequences + i * 64) == (id | (variation << 16))) { sequence = i; break; }
            }
            if (sequence == count) return 1;
            const auto span = layout.bones.present ? layout.bones : layout.model;
            uint32_t movingBones = 0;
            for (size_t i = 0; i < word(bytes, 0x2c); ++i) {
                const size_t bone = word(bytes, 0x30) + i * 88;
                bool moving = false;
                for (size_t t = 0; t < 3; ++t) {
                    const size_t track = bone + 16 + t * 20;
                    if ((word(bytes, track) >> 16) != 0xffff) continue; // globals remain in the model
                    if (sequence >= word(bytes, track + 12)) continue;
                    const size_t pair = word(bytes, track + 16) + sequence * 8;
                    const size_t keys = word(bytes, pair), offset = span.offset + word(bytes, pair + 4);
                    size_t stride = t == 1 ? 8 : 12;
                    if ((word(bytes, track) & 0xffff) >= 2) stride *= 3;
                    for (size_t k = 1; k < keys; ++k)
                        moving = moving || std::memcmp(anim.data() + offset, anim.data() + offset + k * stride, stride) != 0;
                }
                if (moving) ++movingBones;
            }
            std::printf("Animation %lu:%lu validated: %zu bytes; model %zu, skeleton bones %zu, attachments %zu; %u bones have changing keys\n",
                id, variation, anim.size(), layout.model.size, layout.bones.size, layout.attachments.size, movingBones);
            if (!movingBones) { std::fprintf(stderr, "No skeletal motion in the requested animation\n"); return 1; }
        }
        return 0;
    }
    std::printf("%s: %zu decoded bytes (%s)\n", argv[2], bytes.size(), inspect ? "original CDN data" : "client-ready memory");
    if (bytes.size() >= 48 && std::memcmp(bytes.data(), "SKIN", 4) == 0) {
        uint32_t words[12];
        std::memcpy(words, bytes.data(), sizeof(words));
        std::printf("SKIN vertices %u; indices %u; bone indices %u; sections %u; batches %u; max bones %u\n",
            words[1], words[3], words[5], words[7], words[9], words[11]);
        if (words[7] && words[8] <= bytes.size() && 48 <= bytes.size() - words[8]) {
            uint16_t section[10];
            std::memcpy(section, bytes.data() + words[8], sizeof(section));
            std::printf("First section: %u vertices; %u indices; %u bones; combo %u; influences %u\n",
                section[3], section[5], section[6], section[7], section[8]);
        }
        if (words[9] && words[10] <= bytes.size() && 24 <= bytes.size() - words[10]) {
            uint16_t batch[12];
            std::memcpy(batch, bytes.data() + words[10], sizeof(batch));
            std::printf("First batch: shader 0x%04X; %u textures; combos %u/%u/%u/%u\n",
                batch[1], batch[7], batch[8], batch[9], batch[10], batch[11]);
        }
    }
    if (bytes.size() >= 4 && bytes[0] == 'M' && bytes[1] == 'D') {
        Retail::ModelFiles files;
        if (!Retail::InspectModel(bytes, files, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        std::printf("M2 version %u; skeleton %u; %zu skins, %zu textures, %zu animations\n",
            files.version, files.skeleton, files.skins.size(), files.textures.size(), files.animations.size());
        if (bytes.size() >= 0x138) {
            const size_t body = bytes[3] == '1' ? 8 : 0;
            uint32_t count;
            for (size_t offset : {size_t(0x48), size_t(0x70), size_t(0x80), size_t(0x88), size_t(0x90), size_t(0x98)}) {
                std::memcpy(&count, bytes.data() + body + offset, 4);
                std::printf("  table 0x%zX: %u entries\n", offset, count);
            }
        }
        for (auto id : files.skins) std::printf("  skin %u\n", id);
        for (auto id : files.textures) std::printf("  texture %u\n", id);
        if (inspect) {
            std::vector<uint8_t> prepared;
            std::vector<uint8_t> skeleton;
            if (files.skeleton && !source.Inspect(Retail::FileName(files.skeleton, ".skel").c_str(), skeleton, error))
                std::printf("Skeleton fetch: %s\n", error.c_str());
            if (!Retail::PrepareModel(bytes, prepared, files, error, skeleton.empty() ? nullptr : &skeleton))
                std::printf("Runtime support: %s\n", error.c_str());
            else
                std::printf("Runtime model layout supported; use without --inspect to validate skins too.\n");
        }
    }
    return 0;
}
