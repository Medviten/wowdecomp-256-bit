// Optional integration gate against two prepared source caches; not part of offline ctest.
#include "Engine/Source/Retail/Appearance.h"
#include "Engine/Source/Retail/Db2.h"
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/Retail/RetailModel.h"
#include "Engine/Source/Retail/RaceGeometry.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <stdexcept>

namespace app = Retail::Db2::appearance;
void VerifyNightElfEyes(uint32_t sex) {
    Retail::SourceScope scope(Retail::SourceKind::Retail);
    app::Recipe recipe;
    if (!app::BuildForCharacter(4, sex, nullptr, 0, recipe)) throw std::runtime_error("Missing Night Elf model");
    std::vector<uint8_t> body, skin; std::string error; Retail::ModelFiles files;
    auto& source = Retail::Source();
    if (!source.Inspect(Retail::FileName(recipe.modelFileDataId, ".m2").c_str(), body, error)
        || !Retail::InspectModel(body, files, error) || files.skins.empty()
        || !source.Inspect(Retail::FileName(files.skins.front(), ".skin").c_str(), skin, error))
        throw std::runtime_error("Cannot inspect Night Elf eye fixture: " + error);
    const auto word = [&](size_t p) { uint32_t v; std::memcpy(&v, skin.data() + p, 4); return v; };
    if (skin.size() < 36 || word(32) > skin.size() || uint64_t(word(28)) * 48 > skin.size() - word(32))
        throw std::runtime_error("Invalid Night Elf skin fixture");
    struct Model {
        std::set<uint16_t> visible;
        void SetGeometryVisible(uint32_t first, uint32_t last, int show) {
            for (uint32_t id = first; id <= last; ++id)
                if (show) visible.insert(uint16_t(id)); else visible.erase(uint16_t(id));
        }
    } model;
    for (uint32_t i = 0; i < word(28); ++i) {
        uint16_t geo; std::memcpy(&geo, skin.data() + word(32) + size_t(i) * 48, 2);
        model.visible.insert(geo);
    }
    if (!model.visible.count(1701) || !model.visible.count(3301)) throw std::runtime_error("Missing Night Elf eye geometry fixture");
    Retail::ApplyRaceEyeGeometry(model);
    if (model.visible.count(1701) || !model.visible.count(3301)) throw std::runtime_error("Night Elf default eye lightning not Off");
    Retail::ApplyRaceEyeGeometry(model, true);
    if (!model.visible.count(1701) || !model.visible.count(3301)) throw std::runtime_error("Night Elf explicit eye lightning not On");
    Retail::ApplyRaceEyeGeometry(model, false);
    if (model.visible.count(1701) || source.Stats().acquired) throw std::runtime_error("Night Elf toggle did not return to Off or acquired data");
    std::printf("Night Elf sex %u: actual eye meshes toggle Off/On/Off; normal eyes preserved.\n", sex);
}
void VerifySkyborneEyes() {
    Retail::SourceScope scope(Retail::SourceKind::Forever);
    for (uint32_t race : {95u, 96u}) for (uint32_t sex : {0u, 1u}) {
        const auto model = app::ChrModelForRace(race, sex);
        std::vector<uint32_t> defaults;
        uint32_t eyeOption = 0, eyeColors = 0;
        std::vector<std::vector<uint32_t>> eligible;
        for (uint32_t i = 0; i < app::OptionCount(model); ++i) {
            app::ChrOption option{}; app::OptionAt(model, i, option);
            std::vector<uint32_t> choices;
            for (uint32_t j = 0; j < app::ChoiceCount(option.id); ++j) {
                app::ChrChoice choice{}; app::ChoiceAt(option.id, j, choice);
                if (app::CreationChoiceAllowed(model, choice.id)) choices.push_back(choice.id);
            }
            if (choices.empty()) continue;
            if (app::OptionName(option.id) == "Eye Color") { eyeOption = uint32_t(eligible.size()); eyeColors = uint32_t(choices.size()); }
            defaults.push_back(choices.front());
            eligible.push_back(std::move(choices));
        }
        if (eyeColors != 9) throw std::runtime_error("Missing authored Skyborne eye palettes");
        for (size_t i = 0; i < eligible.size(); ++i) for (uint32_t choice : eligible[i]) {
            auto selected = defaults; selected[i] = choice;
            app::Recipe recipe;
            if (!app::BuildForCharacter(race, sex, selected.data(), uint32_t(selected.size()), recipe))
                throw std::runtime_error("Skyborne creator recipe failed");
            for (uint16_t geo : recipe.geosets)
                if (geo / 100 == 17 && geo % 100)
                    throw std::runtime_error("Eye-effect repair would hide authored Skyborne choice "
                        + std::to_string(choice) + " geoset " + std::to_string(geo));
            if (i == eyeOption && std::none_of(recipe.layers.begin(), recipe.layers.end(),
                [](const auto& layer) { return layer.textureType == 19; }))
                throw std::runtime_error("Skyborne eye palette lost its normal eye material");
        }

        app::Recipe recipe; app::BuildForCharacter(race, sex, defaults.data(), uint32_t(defaults.size()), recipe);
        std::vector<uint8_t> body, skin; std::string error;
        auto& source = Retail::Source(); Retail::ModelFiles files;
        if (!source.Inspect(Retail::FileName(recipe.modelFileDataId, ".m2").c_str(), body, error)
            || !Retail::InspectModel(body, files, error) || files.skins.empty()
            || !source.Inspect(Retail::FileName(files.skins.front(), ".skin").c_str(), skin, error))
            throw std::runtime_error("Cannot inspect cached Skyborne skin: " + error);
        const auto word = [&](size_t p) { uint32_t v; std::memcpy(&v, skin.data() + p, 4); return v; };
        if (skin.size() < 36 || word(0) != 0x4e494b53 || word(32) > skin.size()
            || uint64_t(word(28)) * 48 > skin.size() - word(32)) throw std::runtime_error("Invalid Skyborne skin fixture");
        struct Model {
            std::set<uint16_t> visible;
            void SetGeometryVisible(uint32_t first, uint32_t last, int show) {
                for (uint32_t id = first; id <= last; ++id) {
                    if (show) visible.insert(uint16_t(id)); else visible.erase(uint16_t(id));
                }
            }
        } visibility;
        for (uint32_t i = 0; i < word(28); ++i) {
            uint16_t geo; std::memcpy(&geo, skin.data() + word(32) + size_t(i) * 48, 2);
            visibility.visible.insert(geo);
        }
        for (uint16_t geo : {1701, 1703, 3301, 5101})
            if (!visibility.visible.count(geo)) throw std::runtime_error("Missing inherited/authored eye mesh fixture");
        Retail::ApplyRaceEyeGeometry(visibility);
        for (uint16_t geo : visibility.visible)
            if (geo / 100 == 17) throw std::runtime_error("Inherited Skyborne eye effects remain visible");
        if (!visibility.visible.count(3301) || !visibility.visible.count(5101) || source.Stats().acquired)
            throw std::runtime_error("Eye repair hid authored eyes or acquired cached assets again");
        Retail::ApplyRaceEyeGeometry(visibility, true);
        if (!visibility.visible.count(1701) || visibility.visible.count(1703) || !visibility.visible.count(3301))
            throw std::runtime_error("Explicit eye-lightning toggle did not restore exactly the chosen effect");
        std::printf("Skyborne race %u sex %u: eye toggle Off/On passed; all %u authored eye colors preserved.\n", race, sex, eyeColors);
    }
}
void VerifyCreator(Retail::SourceKind kind) {
    Retail::SourceScope scope(kind);
    const auto* choices = app::Get("chrcustomizationchoice");
    const auto* options = app::Get("chrcustomizationoption");
    unsigned blocked = 0;
    for (const auto& row : choices->Rows()) {
        if (choices->Value(row, "ChrCustomizationReqID") != 10 && app::ChoiceName(row.id) != "Transmog") continue;
        const auto* option = options->Find(choices->Value(row, "ChrCustomizationOptionID"));
        if (option && app::CreationChoiceAllowed(options->Value(*option, "ChrModelID"), row.id))
            throw std::runtime_error("Transmog choice exposed in creator");
        ++blocked;
    }
    if (!blocked) throw std::runtime_error("Missing Transmog policy fixtures");
    if (kind == Retail::SourceKind::Forever) {
        for (const auto expected : {std::pair<uint32_t, uint32_t>{9020, 10}, {9021, 9}, {9022, 21}, {9023, 11},
            {9033, 10}, {9034, 9}, {9035, 25}, {9036, 10}, {9032, 0}, {9049, 0}}) {
            const auto* option = options->Find(expected.first);
            unsigned count = 0;
            for (uint32_t i = 0; i < app::ChoiceCount(expected.first); ++i) {
                app::ChrChoice choice{};
                app::ChoiceAt(expected.first, i, choice);
                if (app::CreationChoiceAllowed(options->Value(*option, "ChrModelID"), choice.id)) ++count;
            }
            if (count != expected.second) throw std::runtime_error("Skyborne creator eligibility/count mismatch");
            std::printf("Forever creator option %u: %u playable choices\n", expected.first, count);
        }
        if (app::ChoiceCount(9021) != 41) throw std::runtime_error("Creator filtering changed raw/saved choice enumeration");
    }
    std::printf("%s creator: %u Transmog placeholders excluded; raw choices preserved.\n",
        kind == Retail::SourceKind::Forever ? "Forever" : "Retail", blocked);
}
void Verify(Retail::SourceKind kind, uint32_t race, uint32_t sex, uint32_t expected) {
    Retail::SourceScope scope(kind);
    app::Recipe recipe;
    const uint32_t chrModel = app::ChrModelForRace(race, sex);
    if (!chrModel || !app::OptionCount(chrModel) ||
        !app::BuildForCharacter(race, sex, nullptr, 0, recipe) || recipe.modelFileDataId != expected)
        throw std::runtime_error("Wrong source/model or missing customization options");
    auto& source = Retail::Source();
    std::vector<uint8_t> bytes;
    std::string error;
    source.RegisterCharacterModel(expected);
    const auto name = Retail::FileName(expected, ".m2");
    if (!source.Read(name.c_str(), bytes, error)) throw std::runtime_error(error);
    source.ReleaseModel(name.c_str());
    if (!source.Stats().reused || source.Stats().acquired)
        throw std::runtime_error("Expected a fully warm source with zero acquisition");
}
int main(int argc, char** argv) {
    if (argc != 3) { std::fprintf(stderr, "Usage: CdnSourceSmoke <retail-config> <forever-config>\n"); return 2; }
    try {
        _putenv_s("WOW_RETAIL_CONFIG", argv[1]);
        _putenv_s("WOW_FOREVER_CONFIG", argv[2]);
        // Concurrent initialization must not share once_flags, DB2 rows or CASC handles.
        auto retail = std::async(std::launch::async, [] { Verify(Retail::SourceKind::Retail, 70, 0, 4395382); });
        auto forever = std::async(std::launch::async, [] { Verify(Retail::SourceKind::Forever, 95, 0, 7478487); });
        retail.get(); forever.get();
        Verify(Retail::SourceKind::Forever, 96, 1, 7478494);
        Verify(Retail::SourceKind::Retail, 70, 1, 4220448);
        Verify(Retail::SourceKind::Retail, 70, 0, 4395382);
        VerifyCreator(Retail::SourceKind::Retail);
        VerifyCreator(Retail::SourceKind::Forever);
        VerifySkyborneEyes();
        VerifyNightElfEyes(0);
        VerifyNightElfEyes(1);
        std::puts("Concurrent Retail/Forever stores and alternating models passed; zero acquisition.");
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
