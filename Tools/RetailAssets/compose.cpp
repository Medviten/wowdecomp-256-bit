// Composes a retail race's textures to PNG files for inspection.
// Usage: RetailComposeProbe <RetailCasc.ini> <chrRaceId> <sex> <outputPrefix> [choiceId ...]
// Without choice IDs, the first choice of every option is used.
#include "Engine/Source/Retail/Appearance.h"
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/Compose.h"
#include "Engine/Source/Retail/RetailModel.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

namespace app = Retail::Db2::appearance;

namespace {

uint32_t Crc(const uint8_t* data, size_t size, uint32_t crc = 0) {
    static uint32_t table[256];
    if (!table[1]) for (uint32_t n = 0; n < 256; ++n) { uint32_t c = n; for (int k = 0; k < 8; ++k) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1; table[n] = c; }
    crc = ~crc;
    for (size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 255] ^ (crc >> 8);
    return ~crc;
}

void Be(std::vector<uint8_t>& out, uint32_t v) { for (int i = 3; i >= 0; --i) out.push_back(uint8_t(v >> (8 * i))); }

void Chunk(std::vector<uint8_t>& out, const char* tag, const std::vector<uint8_t>& body) {
    Be(out, uint32_t(body.size()));
    std::vector<uint8_t> typed(tag, tag + 4);
    typed.insert(typed.end(), body.begin(), body.end());
    out.insert(out.end(), typed.begin(), typed.end());
    Be(out, Crc(typed.data(), typed.size()));
}

// Uncompressed (stored-block) PNG; large but dependency free.
bool WritePng(const char* path, const Retail::Image& image) {
    std::vector<uint8_t> raw;
    for (uint32_t y = 0; y < image.height; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), image.rgba.begin() + size_t(y) * image.width * 4, image.rgba.begin() + size_t(y + 1) * image.width * 4);
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    for (size_t at = 0; at < raw.size(); at += 65535) {
        const size_t n = std::min<size_t>(65535, raw.size() - at);
        z.push_back(at + n >= raw.size());
        z.push_back(uint8_t(n)); z.push_back(uint8_t(n >> 8)); z.push_back(uint8_t(~n)); z.push_back(uint8_t((~n) >> 8));
        z.insert(z.end(), raw.begin() + at, raw.begin() + at + n);
    }
    Be(z, (b << 16) | a);
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a}, header;
    Be(header, image.width); Be(header, image.height);
    header.insert(header.end(), {8, 6, 0, 0, 0});
    Chunk(png, "IHDR", header); Chunk(png, "IDAT", z); Chunk(png, "IEND", {});
    FILE* file = std::fopen(path, "wb");
    if (!file) return false;
    std::fwrite(png.data(), 1, png.size(), file);
    std::fclose(file);
    return true;
}

bool Fetch(uint32_t fileDataId, std::vector<uint8_t>& bytes, void*) {
    std::string error;
    return Retail::Source().Read(Retail::FileName(fileDataId, ".blp").c_str(), bytes, error);
}

}

int ComposeCase(int argc, char** argv) {
    if (argc == 5 && std::string(argv[2]) == "texture") {
        _putenv_s(Retail::currentSource == Retail::SourceKind::Forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[1]);
        std::vector<uint8_t> bytes;
        Retail::Image image;
        std::string error;
        if (!Fetch(uint32_t(std::strtoul(argv[3], nullptr, 10)), bytes, nullptr)
            || !Retail::DecodeBlp(bytes, image, error) || !WritePng(argv[4], image)) {
            std::fprintf(stderr, "Texture decode: %s\n", error.c_str()); return 1;
        }
        std::printf("Decoded texture %s: %ux%u -> %s\n", argv[3], image.width, image.height, argv[4]);
        return 0;
    }
    if (argc < 5) {
        std::fprintf(stderr, "Usage: RetailComposeProbe <RetailCasc.ini> <chrRaceId> <sex> <outputPrefix> [choiceId ...]\n");
        return 2;
    }
    _putenv_s(Retail::currentSource == Retail::SourceKind::Forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[1]);
    const uint32_t race = uint32_t(std::strtoul(argv[2], nullptr, 10)), sex = uint32_t(std::atoi(argv[3]));
    // WOW_COMPOSE_FORM selects a Two Forms model (app::Form value, e.g. 3 = Dracthyr Dragon).
    const char* formEnv = std::getenv("WOW_COMPOSE_FORM");
    const uint32_t form = formEnv ? uint32_t(std::strtoul(formEnv, nullptr, 10)) : 0;
    std::vector<uint32_t> choices;
    // Arguments are choice IDs, or "optionId:index" to pick the index-th choice of an option (others default to 0).
    std::vector<std::pair<uint32_t, uint32_t>> picks;
    for (int i = 5; i < argc; ++i) {
        const std::string arg = argv[i];
        const size_t colon = arg.find(':');
        if (colon != std::string::npos) picks.emplace_back(uint32_t(std::strtoul(arg.c_str(), nullptr, 10)),
            uint32_t(std::strtoul(arg.c_str() + colon + 1, nullptr, 10)));
        else choices.push_back(uint32_t(std::strtoul(arg.c_str(), nullptr, 10)));
    }
    if (choices.empty()) {
        const uint32_t model = app::ChrModelForRace(race, sex, form);
        for (uint32_t i = 0, n = app::OptionCount(model); i < n; ++i) {
            app::ChrOption option{};
            app::ChrChoice choice{};
            uint32_t index = 0;
            for (const auto& pick : picks) if (app::OptionAt(model, i, option) && pick.first == option.id) index = pick.second;
            if (app::OptionAt(model, i, option) && app::ChoiceCount(option.id) && app::ChoiceAt(option.id, index, choice)) choices.push_back(choice.id);
        }
    }
    for (uint32_t id : choices) std::printf("choice %u\n", id);
    app::Recipe recipe;
    if (!app::BuildForCharacter(race, sex, choices.data(), uint32_t(choices.size()), recipe, form)) {
        std::fprintf(stderr, "No model for race %u sex %u\n", race, sex);
        return 1;
    }
    std::printf("recipe model %u layout %u\n", recipe.modelFileDataId, recipe.textureLayoutId);
    for (const auto& layer : recipe.layers)
        std::printf("layer type %u order %u blend %u mask %d material %u file %u\n", layer.textureType, layer.layer, layer.blendMode,
            int(layer.sectionMask), layer.materialResourcesId, app::MaterialFileDataId(layer.materialResourcesId, 0));
    std::vector<Retail::ComposedTexture> textures;
    std::string warnings;
    const bool ok = Retail::ComposeTextures(recipe, Fetch, nullptr, textures, warnings);
    if (!warnings.empty()) std::fprintf(stderr, "%s", warnings.c_str());
    for (const auto& texture : textures) {
        const std::string path = std::string(argv[4]) + "-type" + std::to_string(texture.textureType) + ".png";
        std::printf("type %u: %ux%u -> %s (%s)\n", texture.textureType, texture.image.width, texture.image.height, path.c_str(),
            WritePng(path.c_str(), texture.image) ? "written" : "FAILED");
    }
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    const bool forever = argc > 1 && std::string(argv[1]) == "--forever";
    if (forever) { --argc; ++argv; }
    Retail::SourceScope sourceScope(forever ? Retail::SourceKind::Forever : Retail::SourceKind::Retail);
    if (argc != 4 || std::string(argv[2]) != "batch") return ComposeCase(argc, argv);
    std::ifstream cases(argv[3]);
    if (!cases) return 2;
    std::string line;
    uint32_t total = 0, failures = 0;
    while (std::getline(cases, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> args{argv[0], argv[1]};
        std::istringstream words(line);
        std::string word;
        while (words >> word) args.push_back(word);
        std::vector<char*> pointers;
        for (auto& arg : args) pointers.push_back(arg.data());
        if (ComposeCase(int(pointers.size()), pointers.data())) ++failures;
        ++total;
    }
    std::printf("Batch: %u cases, %u failures\n", total, failures);
    return failures ? 1 : 0;
}
