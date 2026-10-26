#ifndef ENGINE_SOURCE_RETAIL_COMPOSE_H
#define ENGINE_SOURCE_RETAIL_COMPOSE_H

#include "Engine/Source/Retail/Appearance.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Retail {

// Straight (non-premultiplied) RGBA, row 0 at the top.
struct Image {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgba;
};

// Decodes mip 0 of a BLP2 (palette, DXT1/3/5 or raw BGRA).
bool DecodeBlp(const std::vector<uint8_t>& blp, Image& out, std::string& error);

// Raw BGRA BLP2 with a full mip chain, in the form the client's texture cache loads.
void BuildBlp(const Image& image, std::vector<uint8_t>& blp);

// Stretches `src` over a rectangle of `dst` (bilinear) with a retail blend mode (alpha-over for most modes).
void PaintImage(Image& dst, const Image& src, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t blend);

// One composed model texture: `textureType` is the M2 texture type (1 body skin, 6 hair, 19 eyes ...).
struct ComposedTexture {
    uint32_t textureType = 0;
    Image image;
};

// Builds every texture the recipe's layers paint. `fetch` returns the bytes of a retail BLP by FileDataID;
// layers whose file cannot be fetched are skipped and named in `warnings`.
using BlpFetch = bool (*)(uint32_t fileDataId, std::vector<uint8_t>& bytes, void* context);
bool ComposeTextures(const Db2::appearance::Recipe& recipe, BlpFetch fetch, void* context,
    std::vector<ComposedTexture>& out, std::string& warnings);

}
#endif
