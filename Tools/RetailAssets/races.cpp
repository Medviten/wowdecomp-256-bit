// Lists retail race customization from the pinned build's DB2 tables.
// Usage: RetailRaceProbe <RetailCasc.ini> [chrRaceId sex]
#include "Engine/Source/Retail/Appearance.h"
#include "Engine/Source/Retail/Db2.h"
#include "Engine/Source/Retail/SourceContext.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iomanip>
#include <set>

namespace app = Retail::Db2::appearance;

// WOW_RETAIL_EXTRAS names a file in the client's RetailExtras.txt format, so extra choices can be checked offline.
bool ExtrasFromFile(std::string& text) {
    const char* path = std::getenv("WOW_RETAIL_EXTRAS");
    std::ifstream in(path ? path : "", std::ios::binary);
    if (!in) return false;
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

int main(int argc, char** argv) {
    app::SetExtrasProvider(&ExtrasFromFile);
    const bool forever = argc > 1 && std::string(argv[1]) == "--forever";
    if (forever) { --argc; ++argv; }
    Retail::SourceScope scope(forever ? Retail::SourceKind::Forever : Retail::SourceKind::Retail);
    if (argc == 4 && std::string(argv[2]) == "find-choice") {
        _putenv_s(forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[1]);
        if (!app::Status().loaded) return 1;
        const auto* table = app::Get("chrcustomizationchoice");
        for (const auto& row : table->Rows()) {
            const auto name = app::ChoiceName(row.id);
            if (name.find(argv[3]) == std::string::npos) continue;
            const auto option = table->Value(row, "ChrCustomizationOptionID");
            std::printf("choice %u %s; option %u %s; requirement %u\n", row.id, name.c_str(), option,
                app::OptionName(option).c_str(), table->Value(row, "ChrCustomizationReqID"));
        }
        return 0;
    }
    if (argc == 4 && std::string(argv[2]) == "forms") {
        // RetailRaceProbe <ini> forms <chrRaceId>: model, options and default-choice recipe of each Two Forms form, both sexes.
        _putenv_s(forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[1]);
        if (!app::Status().loaded) return 1;
        const uint32_t race = uint32_t(std::strtoul(argv[3], nullptr, 10));
        static const char* const names[] = {"default", "wolf", "human", "dragon", "visage"};
        int failures = 0;
        for (uint32_t form = 0; form <= app::FormVisage; ++form) for (uint32_t sex = 0; sex < 2; ++sex) {
            const uint32_t model = app::ChrModelForRace(race, sex, form);
            const uint32_t optionCount = model ? app::OptionCount(model) : 0;
            std::vector<uint32_t> defaults;
            for (uint32_t i = 0; i < optionCount; ++i) {
                app::ChrOption option{};
                app::ChrChoice choice{};
                if (app::OptionAt(model, i, option) && app::ChoiceAt(option.id, 0, choice)) defaults.push_back(choice.id);
            }
            app::Recipe recipe;
            const bool ok = model && app::BuildForCharacter(race, sex, defaults.data(), uint32_t(defaults.size()), recipe, form);
            std::printf("race %u sex %u %-7s: chrModel %u, %zu options, %zu layers, m2 file %u, %zu geosets, %zu attached%s\n", race, sex, names[form],
                model, size_t(optionCount), ok ? recipe.layers.size() : 0, ok ? recipe.modelFileDataId : 0,
                ok ? recipe.geosets.size() : 0, ok ? recipe.attached.size() : 0, ok ? "" : " (none)");
            if (ok && std::getenv("WOW_FORMS_GEOSETS")) { for (const auto geoset : recipe.geosets) std::printf(" %u", unsigned(geoset)); std::printf("\n"); }
            if (form != app::FormDefault && !ok) ++failures;
        }
        return failures ? 1 : 0;
    }
    if (argc == 4 && std::string(argv[2]) == "choices") {
        _putenv_s(forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[1]);
        if (!app::Status().loaded) return 1;
        const uint32_t option = uint32_t(std::strtoul(argv[3], nullptr, 10));
        for (uint32_t i = 0; i < app::ChoiceCount(option); ++i) {
            app::ChrChoice choice{};
            if (!app::ChoiceAt(option, i, choice)) return 1;
            std::printf("%u %u %s\n", i, choice.id, app::ChoiceName(choice.id).c_str());
        }
        return 0;
    }
    if (argc == 4 && std::string(argv[2]) == "catalog") {
        _putenv_s(forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[1]);
        if (!app::Status().loaded) return 1;
        std::ofstream out(argv[3]);
        out << "[";
        bool comma = false;
        for (uint32_t race = 1; race <= 120; ++race) for (uint32_t sex = 0; sex < 2; ++sex) {
            const auto model = app::ChrModelForRace(race, sex);
            if (!model) continue;
            std::vector<uint32_t> defaults;
            if (comma) out << ",";
            comma = true;
            out << "\n{\"race\":" << race << ",\"sex\":" << sex << ",\"chrModel\":" << model << ",\"options\":[";
            for (uint32_t i = 0; i < app::OptionCount(model); ++i) {
                app::ChrOption option{};
                if (!app::OptionAt(model, i, option)) return 1;
                if (i) out << ",";
                out << "{\"id\":" << option.id << ",\"name\":" << std::quoted(app::OptionName(option.id)) << ",\"choices\":[";
                for (uint32_t j = 0; j < app::ChoiceCount(option.id); ++j) {
                    app::ChrChoice choice{};
                    if (!app::ChoiceAt(option.id, j, choice)) return 1;
                    if (!j) defaults.push_back(choice.id);
                    if (j) out << ",";
                    out << choice.id;
                }
                out << "]}";
            }
            app::Recipe recipe;
            if (!app::BuildForCharacter(race, sex, defaults.data(), uint32_t(defaults.size()), recipe)) return 1;
            out << "],\"model\":" << recipe.modelFileDataId << ",\"layout\":" << recipe.textureLayoutId << ",\"geosets\":[";
            for (size_t i = 0; i < recipe.geosets.size(); ++i) { if (i) out << ","; out << recipe.geosets[i]; }
            out << "],\"attached\":" << recipe.attached.size() << ",\"collections\":[";
            for (size_t i = 0; i < recipe.attached.size(); ++i) {
                if (i) out << ",";
                out << "{\"file\":" << recipe.attached[i].fileDataId << ",\"geoset\":" << recipe.attached[i].geoset << "}";
            }
            out << "]}";
        }
        out << "\n]\n";
        return out.good() ? 0 : 1;
    }
    if (argc == 5 && std::string(argv[2]) == "audit") {
        _putenv_s(forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[1]);
        if (!app::Status().loaded) return 1;
        const uint32_t race = uint32_t(std::strtoul(argv[3], nullptr, 10));
        const uint32_t sex = uint32_t(std::strtoul(argv[4], nullptr, 10));
        const uint32_t model = app::ChrModelForRace(race, sex);
        if (!model) return 1;
        std::vector<app::ChrOption> options;
        std::vector<uint32_t> defaults;
        for (uint32_t i = 0; i < app::OptionCount(model); ++i) {
            app::ChrOption option{};
            app::ChrChoice choice{};
            if (!app::OptionAt(model, i, option) || !app::ChoiceAt(option.id, 0, choice)) return 1;
            options.push_back(option);
            defaults.push_back(choice.id);
        }
        uint32_t total = 0, failures = 0, attached = 0, missingMaterials = 0;
        std::set<std::pair<uint32_t, uint16_t>> collections;
        for (size_t i = 0; i < options.size(); ++i) {
            uint32_t withAttached = 0;
            for (uint32_t j = 0; j < app::ChoiceCount(options[i].id); ++j) {
                app::ChrChoice choice{};
                if (!app::ChoiceAt(options[i].id, j, choice)) return 1;
                auto picks = defaults;
                picks[i] = choice.id;
                app::Recipe recipe;
                if (!app::BuildForCharacter(race, sex, picks.data(), uint32_t(picks.size()), recipe)
                    || !recipe.modelFileDataId || !recipe.textureLayoutId) ++failures;
                if (!recipe.attached.empty()) { ++withAttached; ++attached; }
                for (const auto& part : recipe.attached) collections.emplace(part.fileDataId, part.geoset);
                for (const auto& layer : recipe.layers)
                    if (!app::MaterialFileDataId(layer.materialResourcesId, 0)) ++missingMaterials;
                ++total;
            }
            std::printf("option %u %s: %u choices, %u recipes with attached models\n", options[i].id,
                app::OptionName(options[i].id).c_str(), app::ChoiceCount(options[i].id), withAttached);
        }
        std::printf("race %u sex %u: %u individual-choice recipes, %u failures, %u with attached models, %u unresolved materials\n",
            race, sex, total, failures, attached, missingMaterials);
        for (const auto& part : collections)
            std::printf("collection file %u geoset %u\n", part.first, part.second);
        return failures || missingMaterials ? 1 : 0;
    }
    if (argc >= 4 && std::string(argv[2]) == "dump") {
        // RetailRaceProbe <ini> dump <table> [field value]: prints rows, optionally those with field == value.
        _putenv_s(forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[1]);
        app::Status();
        const Retail::Db2::Table* table = app::Get(argv[3]);
        if (!table) { std::fprintf(stderr, "unknown table\n"); return 1; }
        const auto names = table->FieldNames();
        int shown = 0;
        for (const auto& row : table->Rows()) {
            if (argc >= 6) {
                const std::string field = argv[4];
                const std::string want = argv[5];
                const uint32_t have = field == "@parent" ? row.parentId : field == "@id" ? row.id : table->Value(row, field);
                if (have != uint32_t(std::strtoul(want.c_str(), nullptr, 10))) continue;
            }
            std::printf("id %u parent %u:", row.id, row.parentId);
            for (const auto& name : names) {
                std::printf(" %s=", name.c_str());
                for (size_t e = 0; e < table->ElementCount(name); ++e) std::printf("%s%d", e ? "," : "", int(table->Value(row, name, e)));
            }
            std::printf("\n");
            if (++shown >= 200) break;
        }
        return 0;
    }
    if (argc == 4 && std::string(argv[2]) == "choice-textures") {
        // RetailRaceProbe <ini> choice-textures <optionId>: each choice's materials down to their texture FileDataIDs.
        _putenv_s(forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[1]);
        if (!app::Status().loaded) return 1;
        const auto* elements = app::Get("chrcustomizationelement");
        const auto* materials = app::Get("chrcustomizationmaterial");
        const uint32_t option = uint32_t(std::strtoul(argv[3], nullptr, 10));
        for (uint32_t i = 0; i < app::ChoiceCount(option); ++i) {
            app::ChrChoice choice{};
            if (!app::ChoiceAt(option, i, choice)) return 1;
            for (const auto& row : elements->Rows()) {
                if (elements->Value(row, "ChrCustomizationChoiceID") != choice.id) continue;
                const uint32_t material = elements->Value(row, "ChrCustomizationMaterialID");
                if (!material) continue;
                const auto* m = materials->Find(material);
                if (!m) continue;
                const uint32_t resource = materials->Value(*m, "MaterialResourcesID");
                std::printf("choice %u idx %u related %u element %u material %u target %u resource %u file %u\n", choice.id, i,
                    elements->Value(row, "RelatedChrCustomizationChoiceID"), row.id, material,
                    materials->Value(*m, "ChrModelTextureTargetID"), resource, app::MaterialFileDataId(resource, 0));
            }
        }
        return 0;
    }
    if (argc != 2 && argc != 4) {
        std::fprintf(stderr, "Usage: RetailRaceProbe <RetailCasc.ini> [chrRaceId sex|audit chrRaceId sex]\n");
        return 2;
    }
    _putenv_s(forever ? "WOW_FOREVER_CONFIG" : "WOW_RETAIL_CONFIG", argv[1]);
    const app::StoreStatus& status = app::Status();
    std::printf("loaded=%d failed=%d models=%u options=%u choices=%u elements=%u layouts=%u\n",
        status.loaded, status.failed, status.chrModels, status.options, status.choices,
        status.elements, status.textureLayouts);
    if (!status.error.empty()) std::printf("note: %s\n", status.error.c_str());
    if (!status.loaded) return 1;
    for (unsigned race = 1; race < 120; ++race) {
        for (unsigned sex = 0; sex < 2; ++sex) {
            if (argc == 4 && (race != unsigned(std::atoi(argv[2])) || sex != unsigned(std::atoi(argv[3])))) continue;
            const uint32_t model = app::ChrModelForRace(race, sex);
            if (!model) continue;
            const uint32_t options = app::OptionCount(model);
            std::printf("race %u sex %u -> ChrModel %u layout %u, %u options\n", race, sex, model,
                app::LayoutForModel(model), options);
            if (argc != 4) continue;
            std::vector<uint32_t> first;
            for (uint32_t i = 0; i < options; ++i) {
                app::ChrOption option{};
                if (!app::OptionAt(model, i, option)) continue;
                const uint32_t choices = app::ChoiceCount(option.id);
                std::printf("  option %u \"%s\" type %u: %u choices\n", option.id, app::OptionName(option.id).c_str(), option.optionType, choices);
                app::ChrChoice choice{};
                if (choices && app::ChoiceAt(option.id, 0, choice)) first.push_back(choice.id);
            }
            app::Recipe recipe;
            const bool ok = app::BuildForCharacter(race, sex, first.data(), uint32_t(first.size()), recipe);
            std::printf("recipe(first choices) ok=%d model=%u skeleton=%u layout=%u geosets=%zu layers=%zu attached=%zu\n",
                ok, recipe.modelFileDataId, recipe.skeletonFileDataId, recipe.textureLayoutId, recipe.geosets.size(),
                recipe.layers.size(), recipe.attached.size());
            for (const auto& layer : recipe.layers)
                std::printf("  layer type %u order %u blend %u mask %d material %u -> file %u (usage0) %u (usage1)\n",
                    layer.textureType, layer.layer, layer.blendMode, int(layer.sectionMask), layer.materialResourcesId,
                    app::MaterialFileDataId(layer.materialResourcesId, 0), app::MaterialFileDataId(layer.materialResourcesId, 1));
            for (uint16_t geoset : recipe.geosets) std::printf("  geoset %u\n", geoset);
            for (const auto& part : recipe.attached)
                std::printf("  collection file %u geoset %u%s\n", part.fileDataId, part.geoset, part.geoset % 100 ? "" : " (hidden)");
        }
    }
    return 0;
}
