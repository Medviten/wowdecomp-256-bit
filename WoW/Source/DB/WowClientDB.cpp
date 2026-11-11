#include "WoW/Source/DB/WowClientDB.h"
#include <algorithm>
#include <cstdarg>
#include <map>
#include <string>
#include <vector>
#include "Engine/Source/Os/OsFile.h"

// Custom: continuation DBCs (see DBContinuations in WowClientDB.h)
DBContinuations* DBContinuations::s_current;

struct DBContinuationName {
    int32_t m_tier;
    std::string m_name;
};

// Lowercase base path ("dbfilesclient\soundentries.dbc") to its continuation files, found when the archives opened
static std::map<std::string, std::vector<DBContinuationName>> s_continuationIndex;
static bool s_continuationsEnabled = true;
static HSLOG s_continuationLog;

static void ContinuationLog(const char* format, ...) {
    if (!s_continuationLog && !SLogCreate("Logs\\ExtendedDBC.log", 0, &s_continuationLog)) {
        return;
    }

    va_list args;
    va_start(args, format);
    SLogVWrite(s_continuationLog, format, args);
    va_end(args);
    SLogFlush(s_continuationLog);
}

static std::string Lowercase(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }

    return text;
}

// Adds "DBFilesClient\Foo.dbc<tier>-<name>" to the index. Anything else, such as "Foo.dbc1-mod.bak", is ignored.
static void AddContinuation(const char* relativePath) {
    std::string path = relativePath;

    for (char& c : path) {
        if (c == '/') {
            c = '\\';
        }
    }

    std::string lower = Lowercase(path);

    if (lower.compare(0, 14, "dbfilesclient\\") != 0) {
        return;
    }

    size_t dot = lower.rfind(".dbc");

    if (dot == std::string::npos || dot + 6 > lower.size() || lower[dot + 4] < '1' || lower[dot + 4] > '9' || lower[dot + 5] != '-') {
        return;
    }

    std::string name = path.substr(dot + 6);

    if (name.empty()) {
        return;
    }

    for (char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            return;
        }
    }

    int32_t tier = lower[dot + 4] - '0';
    std::vector<DBContinuationName>& names = s_continuationIndex[lower.substr(0, dot + 4)];

    for (const DBContinuationName& known : names) {
        if (known.m_tier == tier && Lowercase(known.m_name) == Lowercase(name)) {
            return;
        }
    }

    names.push_back({ tier, name });
}

static int32_t AddListedContinuation(const char* fileName, void* param) {
    AddContinuation(fileName);
    return 1;
}

static int32_t AddFolderContinuation(OS_FILE_DATA& data, void* param) {
    char path[260];
    SStrPrintf(path, sizeof(path), "DBFilesClient\\%s", data.fileName);
    AddContinuation(path);
    return 0;
}

void DBContinuations::SetEnabled(bool enabled) {
    s_continuationsEnabled = enabled;
}

void DBContinuations::ScanArchive(const char* path, SArchive* archive) {
    if (!s_continuationsEnabled) {
        return;
    }

    if (OsDirectoryExists(path)) {
        char folder[260];
        SStrPrintf(folder, sizeof(folder), "%s\\DBFilesClient\\", path);
        DBContinuations::ScanFolder(folder);
    } else if (archive) {
        SFile::List(archive, AddListedContinuation, nullptr);
    }
}

void DBContinuations::ScanFolder(const char* folder) {
    if (s_continuationsEnabled && OsDirectoryExists(folder)) {
        OsFileList(folder, "*.dbc?-*", AddFolderContinuation, nullptr, 0);
    }
}

DBContinuations::~DBContinuations() {
    for (int32_t i = 0; i < this->m_count; i++) {
        SFileCloseFile(this->m_files[i].m_file);
    }
}

// Opens the continuation files of a table, by tier and then by name. A file whose layout differs from the base
// table's is skipped. Leaves each file positioned at its first row.
void DBContinuations::Find(const char* filename, int32_t numColumns, int32_t rowSize, uint32_t baseStringSize) {
    if (!s_continuationsEnabled) {
        return;
    }

    auto found = s_continuationIndex.find(Lowercase(filename));

    if (found == s_continuationIndex.end()) {
        return;
    }

    std::vector<DBContinuationName> names = found->second;
    std::sort(names.begin(), names.end(), [](const DBContinuationName& a, const DBContinuationName& b) {
        return a.m_tier != b.m_tier ? a.m_tier < b.m_tier : Lowercase(a.m_name) < Lowercase(b.m_name);
    });

    for (const DBContinuationName& name : names) {
        char path[260];
        SStrPrintf(path, sizeof(path), "%s%d-%s", filename, name.m_tier, name.m_name.c_str());

        if (this->m_count == s_maxFiles) {
            ContinuationLog("%s: skipped, more than %d continuation files for one table", path, s_maxFiles);
            continue;
        }

        SFileImpl* f;

        if (!SFile::OpenEx(nullptr, path, 0x20000, &f)) {
            ContinuationLog("%s: skipped, cannot open", path);
            continue;
        }

        uint32_t header[5];

        if (!SFileReadFile(f, header, sizeof(header), nullptr, nullptr, 0) || header[0] != 'CBDW') {
            ContinuationLog("%s: skipped, not a DBC file", path);
            SFileCloseFile(f);
            continue;
        }

        if (static_cast<int32_t>(header[2]) != numColumns || static_cast<int32_t>(header[3]) != rowSize) {
            ContinuationLog("%s: skipped, %u columns of %u bytes where the base table has %d of %d", path, header[2], header[3], numColumns, rowSize);
            SFileCloseFile(f);
            continue;
        }

        DBContinuationFile& entry = this->m_files[this->m_count++];
        entry.m_file = f;
        entry.m_numRecords = static_cast<int32_t>(header[1]);
        entry.m_stringSize = header[4];
        entry.m_stringOffset = baseStringSize + this->m_stringSize;
        this->m_numRecords += entry.m_numRecords;
        this->m_stringSize += entry.m_stringSize;
        ContinuationLog("%s: %u rows", path, header[1]);
    }
}

// INLINED
WowClientDB_Base::WowClientDB_Base() {
    this->m_loaded = 0;
    this->m_numRecords = 0;
    this->m_maxID = -1;
    this->m_minID = 0xFFFFFFF;
    this->m_strings = nullptr;
}

// VERIFY: b5021150 manual
// OFFSET: 0x008C6780
WowClientDB_Base::~WowClientDB_Base() {
}

// VERIFY: 60c92aa6 auto
// OFFSET: 0x004CFBB0
uint8_t* DecompressRow(const uint8_t* src, uint32_t size, uint8_t* dst) {
    uint8_t* end = &dst[size];
    *dst = *src;
    uint8_t* out = dst + 1;

    for (const uint8_t* p = src + 1; out < end; p++) {
        *out++ = *p;

        if (*p == p[-1]) {
            int32_t run = p[1];

            while (run) {
                run--;
                *out++ = *p;
            }

            p += 2;

            if (out < end) {
                *out++ = *p;
            }
        }
    }

    return out;
}

// VERIFY: bbb5632f manual
// OFFSET: 0x0095D980
uint32_t CompressRow(const uint8_t* src, uint32_t size, uint8_t* dst) {
    uint8_t* out = dst;
    const uint8_t* end = &src[size];

    if (dst) {
        *dst = *src;
        out = dst + 1;
    }

    uint32_t count = 1;

    for (const uint8_t* p = src + 1; p < end; p++) {
        if (out) {
            *out++ = *p;
        }

        uint8_t value = *p;
        count++;

        if (*p == p[-1]) {
            p++;

            uint32_t run;

            for (run = 0; p < end; run++) {
                if (*p != value || run >= 0xFF) {
                    break;
                }

                p++;
            }

            if (out) {
                *out++ = static_cast<uint8_t>(run);
            }

            count++;

            if (p == end) {
                return count;
            }

            if (out) {
                *out++ = *p;
            }

            count++;
        }
    }

    return count;
}
