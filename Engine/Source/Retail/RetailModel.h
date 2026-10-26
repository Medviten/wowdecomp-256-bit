#ifndef ENGINE_SOURCE_RETAIL_RETAILMODEL_H
#define ENGINE_SOURCE_RETAIL_RETAILMODEL_H

#include <cstdint>
#include <cstddef>
#include <array>
#include <string>
#include <vector>

namespace Retail {

struct AnimationFile {
    uint16_t id;
    uint16_t variation;
    uint32_t fileId;
};

struct AnimationSpan {
    size_t offset = 0, size = 0;
    bool present = false;
};

struct AnimationLayout {
    bool chunked = false;
    AnimationSpan model, bones, attachments;
};

// Chunk offsets are independent address spaces: AFM2 model tracks, AFSB skeleton bones,
// AFSA skeleton attachments. Legacy flat files use one span for all three.
bool InspectAnimation(const uint8_t* bytes, size_t size, AnimationLayout& layout, std::string& error);

struct ModelFiles {
    uint32_t version = 0;
    uint32_t skeleton = 0;
    std::vector<uint32_t> skins;
    std::vector<uint32_t> textures;
    std::vector<AnimationFile> animations;
};

struct HelmetAnchor {
    std::array<float, 3> head{};
    std::array<float, 3> socket{};
};

// Read head key-bone 6 and attachment 11 from an unrelocated, flat model. Used to retain
// installed helmet origins when a player body uses a different skeleton/socket convention.
bool ReadHelmetAnchor(const std::vector<uint8_t>& model, HelmetAnchor& anchor, std::string& error);
std::array<float, 3> HelmetTranslation(const HelmetAnchor& donor, const HelmetAnchor& target);

// All offsets remain file-relative. Preparation happens before the existing M2 relocation
// pass; cached CDN bytes are never changed. Unsupported features fail with a reason.
bool InspectModel(const std::vector<uint8_t>& bytes, ModelFiles& files, std::string& error);
// Flatten one already-resolved parent into a child SKEL. Resident offsets are
// rebuilt; external tracks keep their owning AFID. Inputs are never modified.
bool MergeSkeleton(const std::vector<uint8_t>& child, const std::vector<uint8_t>& parent,
    std::vector<uint8_t>& merged, std::string& error);
bool PrepareModel(const std::vector<uint8_t>& bytes, std::vector<uint8_t>& prepared,
    ModelFiles& files, std::string& error, const std::vector<uint8_t>* skeleton = nullptr);
bool ValidateSkin(const std::vector<uint8_t>& bytes, const std::vector<uint8_t>& model,
    std::string& error);
bool PrepareSkin(std::vector<uint8_t>& bytes, const std::vector<uint8_t>& model,
    std::string& error, bool degradeShaders = false);
// Unwrap model-only AFM2 files. Preserve split skeleton chunks for category-specific relocation.
bool StripAnimationChunk(std::vector<uint8_t>& bytes, std::string& error);
bool ValidateAnimation(const std::vector<uint8_t>& bytes, const std::vector<uint8_t>& model,
    uint16_t id, uint16_t variation, std::string& error);
std::string FileName(uint32_t fileId, const char* extension);

}
#endif
