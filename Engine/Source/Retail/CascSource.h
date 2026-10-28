#ifndef ENGINE_SOURCE_RETAIL_CASCSOURCE_H
#define ENGINE_SOURCE_RETAIL_CASCSOURCE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Engine/Source/Retail/SourceContext.h"

namespace Retail {

struct HandItemAppearance {
    uint32_t modelId = 0;
    uint32_t textureId = 0; // Replaceable object-skin texture (type 2).
};

// One source per pinned build. Storage calls and dependency publication are serialized.
// Each source owns its retail/ or forever/ namespace plus explicit aliases; stock paths stay in MPQ.
class CascSource {
public:
    explicit CascSource(SourceKind kind = SourceKind::Retail);
    ~CascSource();
    CascSource(const CascSource&) = delete;
    CascSource& operator=(const CascSource&) = delete;

    bool Configure(const char* configPath, std::string& error);
    bool Handles(const char* name);
    bool Exists(const char* name, std::string& error);
    bool Read(const char* name, std::vector<uint8_t>& bytes, std::string& error);
    bool Inspect(const char* name, std::vector<uint8_t>& bytes, std::string& error);
    // Explicit player body/collection scope for legacy material approximation. Generic models
    // keep strict unsupported-shader rejection unless their existing shared-skeleton path applies.
    void RegisterCharacterModel(uint32_t fileDataId);
    // Like Inspect, but encrypted blocks without a key are zero-filled instead of failing.
    bool ReadTable(const char* name, std::vector<uint8_t>& bytes, std::string& error);
    void ReleaseModel(const char* name);
    bool CachedSize(const char* name, uint32_t& size);
    // Locality/progress checks may use durable original size before client preparation.
    // CachedSize above remains the exact prepared size for SFile reads.
    bool CachedOriginalSize(const char* name, uint32_t& size);
    bool HandItem(uint32_t displayId, HandItemAppearance& appearance, bool mainHand = false);
    bool SetHandItem(uint32_t displayId, HandItemAppearance appearance);
    bool SetMainHandPreview(HandItemAppearance appearance);
    bool HasClientTable(const char* name);
    bool MergeTable(const char* name, const std::vector<uint8_t>& base,
        std::vector<uint8_t>& merged, std::string& error);
    struct Statistics { uint64_t reused = 0, acquired = 0; };
    Statistics Stats();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Lazy process-wide source, configured by WOW_RETAIL_CONFIG or RetailCasc.ini in the
// working directory. Initialization does no network IO until a Retail reference is read.
CascSource& Source();
CascSource& SourceForPath(const char* name);

// Custom: called by a mode=files source for a FileDataID its folder lacks, with the file it reads (<id>.bin); returns
// true once that file exists. RetailPackBuild downloads the original there; the client leaves it unset.
using PackMissHandler = bool (*)(uint32_t fileDataId, const std::string& file);
void SetPackMissHandler(PackMissHandler handler);

}
#endif
