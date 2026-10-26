#ifndef RETAIL_BODY_SHAPE_FACE_H
#define RETAIL_BODY_SHAPE_FACE_H

#include <cstdint>

// Face bone regions, identified by the bone-name hash stored in every Retail skeleton bone. The hash is the same
// bone on every race and sex, while its position is not, so regions are picked once on the human male skeleton
// (by position around the head bone) and then looked up by hash on any model. Not every skeleton has every bone:
// ears and jaw-angle exist on the human male only, so those sliders are simply absent elsewhere.

namespace BodyShape {

struct FaceRegionDef {
    const char* name;
    bool perSide; // scale about each side's own centre (eyes, ears) instead of the whole group's
    const uint32_t* crcs;
    uint32_t count;
};

inline constexpr uint32_t FaceCrc_Brow[] = { 0x692d05c0u, 0x1947f14fu, 0x804ea0f5u, 0xf7499063u, 0x001c26d9u, 0x7076d256u, 0xe97f83ecu, 0x9e78b37au, 0xc067e3e3u };
inline constexpr uint32_t FaceCrc_Eyes[] = { 0x0c98e225u, 0x9591b39fu, 0xd07f95f6u, 0xacf602c4u, 0x35ff537eu, 0xe79e7265u };
inline constexpr uint32_t FaceCrc_Nose[] = { 0xca2cab67u, 0x8c714084u, 0x7a3d9ad2u, 0x130cb9cbu };
inline constexpr uint32_t FaceCrc_Mouth[] = { 0xeb41f8e8u, 0x054f99c4u, 0x9c46c87eu, 0xf9cf7609u, 0x17c11725u, 0x60c627b3u, 0x0b8196a2u, 0xccea6f5cu, 0x55e33ee6u, 0x22e40e70u, 0xa963d12bu, 0xde64e1bdu, 0x306a8091u };
inline constexpr uint32_t FaceCrc_Chin[] = { 0x5d2ea802u };
inline constexpr uint32_t FaceCrc_Cheeks[] = { 0x5aefeb49u, 0x2de8dbdfu, 0xd0e31386u, 0x3eed72aau, 0x49ea423cu, 0xb4e18a65u };
inline constexpr uint32_t FaceCrc_JawAngle[] = { 0xcd04f5a5u, 0xa9066c46u };
inline constexpr uint32_t FaceCrc_Ears[] = { 0x2f456247u, 0x826c0babu, 0xb3841136u, 0x540da41fu, 0x1ead78dau, 0x300f3dfcu, 0xb64c33fdu, 0x1b655a11u, 0x2a8d408cu, 0x87a42960u };

inline constexpr FaceRegionDef FaceRegions[] = {
    { "Brow", true, FaceCrc_Brow, 9 },
    { "Eyes", true, FaceCrc_Eyes, 6 },
    { "Nose", false, FaceCrc_Nose, 4 },
    { "Mouth", false, FaceCrc_Mouth, 13 },
    { "Chin", false, FaceCrc_Chin, 1 },
    { "Cheeks", true, FaceCrc_Cheeks, 6 },
    { "JawAngle", true, FaceCrc_JawAngle, 2 },
    { "Ears", true, FaceCrc_Ears, 10 },
};

} // namespace BodyShape

#endif
