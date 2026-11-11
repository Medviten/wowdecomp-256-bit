#include <cstdint>
#include "WoW/Source/WowServices/PatchFiles.h"
#include "Engine/Source/Base/Sort.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Client.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"

// VERIFY: 204ba559 auto
// OFFSET: 0x00401200
static int32_t CompareStrings(const void* a, const void* b, void* context) {
    return -SStrCmpI(*static_cast<char* const*>(a), *static_cast<char* const*>(b), STORM_MAX_STR);
}

// Custom: patch archives may have names longer than one character, and a tag that limits them to one realm:
// "patch-<stem>[.<tag>].MPQ", or "patch-<locale>-<stem>[.<tag>].MPQ" in the locale folder. The stem is letters and
// digits. A tagged archive loads only while realmName (the last realm, also set by -realmname) equals its tag, so
// e.g. patch-A.war.MPQ and patch-A.peace.MPQ can stay side by side. Changing realms needs a restart to swap them.
struct PatchName {
    const char* m_stem;
    size_t m_stemLength;
    const char* m_tag;
    size_t m_tagLength;
};

static bool ParsePatchName(const char* path, PatchName& name) {
    const char* fileName = SStrChrR(path, '\\');
    fileName = fileName ? fileName + 1 : path;

    if (SStrCmpI(fileName, "patch-", 6)) {
        return false;
    }

    const char* firstDot = SStrChr(fileName, '.');
    const char* lastDot = SStrChrR(fileName, '.');

    if (!firstDot) {
        return false;
    }

    const char* stem = firstDot;

    while (stem > fileName && stem[-1] != '-') {
        stem--;
    }

    name.m_stem = stem;
    name.m_stemLength = static_cast<size_t>(firstDot - stem);
    name.m_tag = firstDot + 1;
    name.m_tagLength = firstDot == lastDot ? 0 : static_cast<size_t>(lastDot - firstDot - 1);

    if (!name.m_stemLength || (firstDot != lastDot && !name.m_tagLength)) {
        return false;
    }

    for (size_t i = 0; i < name.m_stemLength; i++) {
        char c = stem[i];

        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
            return false;
        }
    }

    return true;
}

static bool IsNumber(const char* text, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }

    return true;
}

// Ranks two patch names, > 0 when a takes priority. Letters beat numbers, as with one character. Numbers compare by
// value (patch-10 beats patch-9), letters by length and then alphabet (patch-AA beats patch-Z), and a tagged archive
// beats the untagged one of the same stem.
static int32_t ComparePatchNames(const PatchName& a, const PatchName& b) {
    bool aNumber = IsNumber(a.m_stem, a.m_stemLength);
    bool bNumber = IsNumber(b.m_stem, b.m_stemLength);

    if (aNumber != bNumber) {
        return aNumber ? -1 : 1;
    }

    const char* aStem = a.m_stem;
    size_t aLength = a.m_stemLength;
    const char* bStem = b.m_stem;
    size_t bLength = b.m_stemLength;

    if (aNumber) {
        while (aLength > 1 && *aStem == '0') {
            aStem++;
            aLength--;
        }

        while (bLength > 1 && *bStem == '0') {
            bStem++;
            bLength--;
        }
    }

    if (aLength != bLength) {
        return aLength > bLength ? 1 : -1;
    }

    int32_t order = SStrCmpI(aStem, bStem, static_cast<uint32_t>(aLength));

    if (order) {
        return order;
    }

    if (!a.m_tagLength != !b.m_tagLength) {
        return a.m_tagLength ? 1 : -1;
    }

    return SStrCmpI(a.m_tag, b.m_tag, static_cast<uint32_t>(a.m_tagLength > b.m_tagLength ? a.m_tagLength : b.m_tagLength));
}

// Sorts like CompareStrings (highest priority first), but by ComparePatchNames within one folder.
static int32_t ComparePatchPaths(const void* a, const void* b, void* context) {
    const char* pathA = *static_cast<char* const*>(a);
    const char* pathB = *static_cast<char* const*>(b);
    const char* slashA = SStrChrR(pathA, '\\');
    const char* slashB = SStrChrR(pathB, '\\');
    PatchName nameA;
    PatchName nameB;

    if (slashA && slashB && slashA - pathA == slashB - pathB && !SStrCmpI(pathA, pathB, static_cast<uint32_t>(slashA - pathA))
        && ParsePatchName(pathA, nameA) && ParsePatchName(pathB, nameB)) {
        return -ComparePatchNames(nameA, nameB);
    }

    return CompareStrings(a, b, context);
}

static bool PatchTagMatches(const PatchName& name) {
    if (!name.m_tagLength) {
        return true;
    }

    CVar* realm = CVar::Lookup("realmName");
    const char* realmName = realm ? realm->m_stringValue.m_str : nullptr;

    return realmName && SStrLen(realmName) == name.m_tagLength && !SStrCmpI(realmName, name.m_tag, static_cast<uint32_t>(name.m_tagLength));
}

// VERIFY: b136b0d2 manual
// OFFSET: 0x00405A10
int32_t PatchFiles::EnumPatchArchives(OS_FILE_DATA& data, void* param) {
    PatchFiles* files = static_cast<PatchFiles*>(param);

    // Custom: only well-formed "patch-" names, and tagged archives only for their realm (see ParsePatchName).
    // patch.MPQ, which has no stem, is unaffected.
    PatchName name;

    if (!SStrCmpI(data.fileName, "patch-", 6) && (!ParsePatchName(data.fileName, name) || !PatchTagMatches(name))) {
        return 0;
    }

    char path[260];
    SStrPrintf(path, sizeof(path), "%s%s", files->m_directory, data.fileName);

    char** slot = files->m_files.New();
    *slot = SStrDupA(path, __FILE__, __LINE__);

    return 0;
}

// VERIFY: 4fd60074 manual
// OFFSET: 0x00405AB0
PatchFiles::PatchFiles(const char* locale, bool flag) {
    PATCHARCHIVESPEC specs[12] = {
        // Custom: "*" instead of "?" for longer names and tags (see ParsePatchName)
        { 0, 0, false, "Data\\", "patch-*.MPQ" },
        { 0, 0, false, "Data\\%s\\", "patch-%s-*.MPQ" },
        { 1, 0, false, "Data\\", "patch.MPQ" },
        { 1, 0, false, "Data\\%s\\", "patch-%s.MPQ" },
        { 1, 1, false, "Data\\", "patch-4.MPQ" },
        { 1, 1, false, "Data\\%s\\", "patch-%s-4.MPQ" },
        { 1, 1, false, "Data\\", "patch-3.MPQ" },
        { 1, 1, false, "Data\\%s\\", "patch-%s-3.MPQ" },
        { 1, 1, false, "..\\Data\\", "patch-2.MPQ" },
        { 1, 1, false, "..\\Data\\%s\\", "patch-%s-2.MPQ" },
        { 1, 1, false, "..\\Data\\", "patch.MPQ" },
        { 1, 1, false, "..\\Data\\%s\\", "patch-%s.MPQ" },
    };

    char directory[256];
    char pattern[256];
    char installed[256];

    this->m_directory = directory;
    const char* installPath = GetInstallLocationString();
    int32_t wanted = flag;

    for (int32_t pass = 0; pass <= 1; pass++) {
        for (uint32_t i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
            PATCHARCHIVESPEC& spec = specs[i];

            if (spec.m_pass != pass || spec.m_flag != wanted) {
                continue;
            }

            SStrPrintf(directory, sizeof(directory), spec.m_directory, locale);
            SStrPrintf(pattern, sizeof(pattern), spec.m_pattern, locale);

            if (spec.m_useInstallPath && *installPath) {
                SStrPrintf(installed, sizeof(installed), "%s%s", installPath, directory);
                SStrCopy(directory, installed, sizeof(directory));
            }

            OsFileList(this->m_directory, pattern, PatchFiles::EnumPatchArchives, this, 0);
        }

        if (pass == 0) {
            // Custom: ComparePatchPaths instead of CompareStrings, for longer names
            quicksort(this->m_files.Ptr(), this->m_files.Count(), sizeof(char*), ComparePatchPaths, nullptr);
        }
    }
}

// VERIFY: ff9d1c1e manual
// OFFSET: 0x00405D70
PatchFiles::~PatchFiles() {
    for (uint32_t i = 0; i < this->m_files.Count(); i++) {
        if (this->m_files[i]) {
            SMemFree(this->m_files[i], __FILE__, __LINE__, 0);
        }
    }
}
