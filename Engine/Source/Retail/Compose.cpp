#include "Engine/Source/Retail/Compose.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace Retail {
namespace {

uint32_t U32(const std::vector<uint8_t>& b, size_t p) {
    return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) | (uint32_t(b[p + 2]) << 16) | (uint32_t(b[p + 3]) << 24);
}

void Rgb565(uint16_t v, uint8_t out[3]) {
    out[0] = uint8_t(((v >> 11) & 31) * 255 / 31);
    out[1] = uint8_t(((v >> 5) & 63) * 255 / 63);
    out[2] = uint8_t((v & 31) * 255 / 31);
}

// Decodes one 4x4 colour block. `opaque3` selects DXT1's 3-colour + transparent mode.
void ColorBlock(const uint8_t* src, bool dxt1, uint8_t out[16][4]) {
    const uint16_t c0 = uint16_t(src[0] | src[1] << 8), c1 = uint16_t(src[2] | src[3] << 8);
    uint8_t palette[4][4];
    Rgb565(c0, palette[0]); Rgb565(c1, palette[1]);
    palette[0][3] = palette[1][3] = 255;
    if (!dxt1 || c0 > c1) {
        for (int i = 0; i < 3; ++i) {
            palette[2][i] = uint8_t((2 * palette[0][i] + palette[1][i]) / 3);
            palette[3][i] = uint8_t((palette[0][i] + 2 * palette[1][i]) / 3);
        }
        palette[2][3] = palette[3][3] = 255;
    } else {
        for (int i = 0; i < 3; ++i) { palette[2][i] = uint8_t((palette[0][i] + palette[1][i]) / 2); palette[3][i] = 0; }
        palette[2][3] = 255; palette[3][3] = 0;
    }
    const uint32_t bits = uint32_t(src[4]) | uint32_t(src[5]) << 8 | uint32_t(src[6]) << 16 | uint32_t(src[7]) << 24;
    for (int i = 0; i < 16; ++i) std::memcpy(out[i], palette[(bits >> (2 * i)) & 3], 4);
}

void Dxt5Alpha(const uint8_t* src, uint8_t out[16]) {
    uint8_t a[8] = {src[0], src[1]};
    if (a[0] > a[1]) for (int i = 1; i < 7; ++i) a[1 + i] = uint8_t(((7 - i) * a[0] + i * a[1]) / 7);
    else { for (int i = 1; i < 5; ++i) a[1 + i] = uint8_t(((5 - i) * a[0] + i * a[1]) / 5); a[6] = 0; a[7] = 255; }
    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) bits |= uint64_t(src[2 + i]) << (8 * i);
    for (int i = 0; i < 16; ++i) out[i] = a[(bits >> (3 * i)) & 7];
}

}

bool DecodeBlp(const std::vector<uint8_t>& b, Image& out, std::string& error) {
    out = {};
    if (b.size() < 148 || std::memcmp(b.data(), "BLP2", 4) != 0 || U32(b, 4) != 1) { error = "Not a BLP2 texture"; return false; }
    const uint8_t encoding = b[8], alphaDepth = b[9], alphaEncoding = b[10];
    const uint32_t width = U32(b, 12), height = U32(b, 16), offset = U32(b, 20), size = U32(b, 84);
    if (!width || !height || width > 8192 || height > 8192 || offset > b.size() || size > b.size() - offset) {
        error = "Invalid BLP dimensions or mip table"; return false;
    }
    out.width = width; out.height = height;
    out.rgba.assign(size_t(width) * height * 4, 0);
    const uint8_t* data = b.data() + offset;
    if (encoding == 3) {
        if (size < size_t(width) * height * 4) { error = "Truncated BLP pixels"; return false; }
        for (size_t i = 0; i < size_t(width) * height; ++i) {
            out.rgba[i * 4 + 0] = data[i * 4 + 2]; out.rgba[i * 4 + 1] = data[i * 4 + 1];
            out.rgba[i * 4 + 2] = data[i * 4 + 0]; out.rgba[i * 4 + 3] = data[i * 4 + 3];
        }
        return true;
    }
    if (encoding == 1) {
        const size_t pixels = size_t(width) * height;
        const size_t alphaBytes = alphaDepth == 8 ? pixels : alphaDepth == 1 ? (pixels + 7) / 8 : alphaDepth == 4 ? (pixels + 1) / 2 : 0;
        if (size < pixels + alphaBytes) { error = "Truncated BLP palette image"; return false; }
        for (size_t i = 0; i < pixels; ++i) {
            const uint32_t color = U32(b, 148 + size_t(data[i]) * 4);
            out.rgba[i * 4 + 0] = uint8_t(color >> 16); out.rgba[i * 4 + 1] = uint8_t(color >> 8); out.rgba[i * 4 + 2] = uint8_t(color);
            uint8_t alpha = 255;
            if (alphaDepth == 8) alpha = data[pixels + i];
            else if (alphaDepth == 1) alpha = (data[pixels + i / 8] >> (i % 8)) & 1 ? 255 : 0;
            else if (alphaDepth == 4) alpha = uint8_t(((data[pixels + i / 2] >> (4 * (i % 2))) & 15) * 17);
            out.rgba[i * 4 + 3] = alpha;
        }
        return true;
    }
    if (encoding != 2) { error = "Unsupported BLP encoding"; return false; }
    const bool dxt1 = alphaDepth == 0 || alphaEncoding == 0;
    const bool dxt3 = !dxt1 && alphaEncoding == 1;
    const uint32_t blockBytes = dxt1 ? 8 : 16;
    const uint32_t bw = (width + 3) / 4, bh = (height + 3) / 4;
    if (size < size_t(bw) * bh * blockBytes) { error = "Truncated BLP DXT data"; return false; }
    for (uint32_t by = 0; by < bh; ++by) {
        for (uint32_t bx = 0; bx < bw; ++bx) {
            const uint8_t* block = data + (size_t(by) * bw + bx) * blockBytes;
            uint8_t colors[16][4], alpha[16];
            ColorBlock(dxt1 ? block : block + 8, dxt1, colors);
            if (dxt1) for (int i = 0; i < 16; ++i) alpha[i] = colors[i][3];
            else if (dxt3) for (int i = 0; i < 16; ++i) alpha[i] = uint8_t(((block[i / 2] >> (4 * (i % 2))) & 15) * 17);
            else Dxt5Alpha(block, alpha);
            for (int i = 0; i < 16; ++i) {
                const uint32_t x = bx * 4 + i % 4, y = by * 4 + i / 4;
                if (x >= width || y >= height) continue;
                uint8_t* pixel = &out.rgba[(size_t(y) * width + x) * 4];
                pixel[0] = colors[i][0]; pixel[1] = colors[i][1]; pixel[2] = colors[i][2]; pixel[3] = alpha[i];
            }
        }
    }
    return true;
}

void BuildBlp(const Image& image, std::vector<uint8_t>& blp) {
    blp.assign(1172, 0);
    std::memcpy(blp.data(), "BLP2", 4);
    auto put = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) blp[at + i] = uint8_t(v >> (8 * i)); };
    put(4, 1);
    blp[8] = 3; blp[9] = 8; blp[10] = 8; blp[11] = 1; // raw BGRA, 8-bit alpha, preferred format 8 below, mips present
    put(12, image.width); put(16, image.height);
    std::vector<uint8_t> level = image.rgba;
    uint32_t width = image.width, height = image.height;
    for (uint32_t mip = 0; mip < 16; ++mip) {
        std::vector<uint8_t> bgra = level;
        for (size_t i = 0; i + 3 < bgra.size(); i += 4) std::swap(bgra[i], bgra[i + 2]);
        put(20 + mip * 4, uint32_t(blp.size()));
        put(84 + mip * 4, uint32_t(bgra.size()));
        blp.insert(blp.end(), bgra.begin(), bgra.end());
        if (width == 1 && height == 1) break;
        const uint32_t nextWidth = std::max(1u, width / 2), nextHeight = std::max(1u, height / 2);
        std::vector<uint8_t> next(size_t(nextWidth) * nextHeight * 4);
        for (uint32_t y = 0; y < nextHeight; ++y)
            for (uint32_t x = 0; x < nextWidth; ++x)
                for (uint32_t c = 0; c < 4; ++c) {
                    uint32_t sum = 0;
                    for (uint32_t dy = 0; dy < 2; ++dy)
                        for (uint32_t dx = 0; dx < 2; ++dx)
                            sum += level[(size_t(std::min(y * 2 + dy, height - 1)) * width + std::min(x * 2 + dx, width - 1)) * 4 + c];
                    next[(size_t(y) * nextWidth + x) * 4 + c] = uint8_t(sum / 4);
                }
        level = std::move(next); width = nextWidth; height = nextHeight;
    }
}

namespace {

struct Rect { uint32_t x, y, w, h; };

// Stretches `src` over `rect` of `dst` with bilinear sampling and the given blend.
void Paint(Image& dst, const Image& src, const Rect& rect, uint32_t blend, bool legacyAlphaOver = false) {
    if (!rect.w || !rect.h || rect.x + rect.w > dst.width || rect.y + rect.h > dst.height) return;
    for (uint32_t y = 0; y < rect.h; ++y) {
        const float fy = (float(y) + 0.5f) * src.height / rect.h - 0.5f;
        const int y0 = std::max(0, int(std::floor(fy))), y1 = std::min(int(src.height) - 1, y0 + 1);
        const float ty = std::min(1.0f, std::max(0.0f, fy - float(y0)));
        for (uint32_t x = 0; x < rect.w; ++x) {
            const float fx = (float(x) + 0.5f) * src.width / rect.w - 0.5f;
            const int x0 = std::max(0, int(std::floor(fx))), x1 = std::min(int(src.width) - 1, x0 + 1);
            const float tx = std::min(1.0f, std::max(0.0f, fx - float(x0)));
            float s[4];
            for (int c = 0; c < 4; ++c) {
                const float top = src.rgba[(size_t(y0) * src.width + x0) * 4 + c] * (1 - tx) + src.rgba[(size_t(y0) * src.width + x1) * 4 + c] * tx;
                const float bottom = src.rgba[(size_t(y1) * src.width + x0) * 4 + c] * (1 - tx) + src.rgba[(size_t(y1) * src.width + x1) * 4 + c] * tx;
                s[c] = top * (1 - ty) + bottom * ty;
            }
            uint8_t* d = &dst.rgba[(size_t(rect.y + y) * dst.width + rect.x + x) * 4];
            const float a = s[3] / 255.0f, da = d[3] / 255.0f;
            auto byte = [](float value) { return uint8_t(std::min(255.0f, std::max(0.0f, value)) + 0.5f); };
            if (blend == 0 || blend == 1) { // DB2 blit preserves RGB even under zero alpha.
                for (int c = 0; c < 4; ++c) d[c] = byte(s[c]);
            } else if (legacyAlphaOver) { // Legacy equipment PaintImage callers use straight alpha-over.
                const float oa = a + da * (1 - a);
                for (int c = 0; c < 3; ++c)
                    d[c] = byte(oa > 0 ? (s[c] * a + d[c] * da * (1 - a)) / oa : 0);
                d[3] = byte(oa * 255.0f);
            } else {
                // ChrModelTextureLayer uses the character compositor's enum, not M2 material
                // blend modes. Match wow.export char.fragment.shader + CharMaterialRenderer at
                // c2fd7bde36a712be78a5da896c995b84fbfa2545: shader result, then GPU blending.
                // In particular, opaque white tattoo texels multiply to unchanged skin.
                const float fa = blend == 4 ? da * a : a;
                for (int c = 0; c < 3; ++c) {
                    const float base = d[c] / 255.0f, src = s[c] / 255.0f;
                    float result = src;
                    if (blend == 4) result = base * src; // Multiply
                    else if (blend == 6) // Overlay, thresholded by the source.
                        result = src < 0.5f ? 2 * base * src : 1 - 2 * (1 - base) * (1 - src);
                    else if (blend == 7) result = 1 - (1 - base) * (1 - src); // Screen
                    d[c] = byte((result * fa + base * (1 - fa)) * 255.0f);
                }
                // Mode 9 has separate ONE/INVSRCALPHA alpha factors. The other modes
                // use SRCALPHA/INVSRCALPHA for alpha as well as RGB (including mode 15).
                d[3] = byte((blend == 9 ? fa + da * (1 - fa) : fa * fa + da * (1 - fa)) * 255.0f);
            }
        }
    }
}

}

void PaintImage(Image& dst, const Image& src, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t blend) {
    Paint(dst, src, {x, y, width, height}, blend, blend == 2);
}

bool ComposeTextures(const Db2::appearance::Recipe& recipe, BlpFetch fetch, void* context,
    std::vector<ComposedTexture>& out, std::string& warnings) {
    namespace app = Db2::appearance;
    out.clear();
    uint32_t sheetWidth = 0, sheetHeight = 0;
    const bool haveSheet = app::LayoutSize(recipe.textureLayoutId, sheetWidth, sheetHeight);
    std::vector<app::TextureSection> sections;
    for (uint32_t i = 0, n = app::SectionCount(recipe.textureLayoutId); i < n; ++i) {
        app::TextureSection section{};
        if (app::SectionAt(recipe.textureLayoutId, i, section)) sections.push_back(section);
    }
    std::map<uint32_t, ComposedTexture> byType;
    for (const app::TextureLayer& layer : recipe.layers) { // recipe.layers is ordered by layer
        const uint32_t file = layer.fileDataId ? layer.fileDataId : app::MaterialFileDataId(layer.materialResourcesId, 0);
        std::vector<uint8_t> bytes;
        Image image;
        std::string error;
        if (!file || !fetch(file, bytes, context) || !DecodeBlp(bytes, image, error)) {
            warnings += "material " + std::to_string(layer.materialResourcesId) + " (file " + std::to_string(file) + ") unavailable" +
                (error.empty() ? "" : ": " + error) + "\n";
            continue;
        }
        ComposedTexture& target = byType[layer.textureType];
        target.textureType = layer.textureType;
        if (!target.image.width) {
            // Body sheets use the layout size; every other type takes the size of its first layer.
            const bool body = layer.textureType == 1 && haveSheet;
            target.image.width = body ? sheetWidth : image.width;
            target.image.height = body ? sheetHeight : image.height;
            target.image.rgba.assign(size_t(target.image.width) * target.image.height * 4, 0);
        }
        const int32_t mask = int32_t(layer.sectionMask);
        if (mask == -1 || layer.textureType != 1) {
            Paint(target.image, image, {0, 0, target.image.width, target.image.height}, layer.blendMode);
            continue;
        }
        for (const app::TextureSection& section : sections)
            if (section.sectionType < 32 && (uint32_t(mask) >> section.sectionType & 1))
                Paint(target.image, image, {section.x, section.y, section.width, section.height}, layer.blendMode);
    }
    for (auto& entry : byType) out.push_back(std::move(entry.second));
    return !out.empty();
}

}
