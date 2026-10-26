#ifndef ENGINE_SOURCE_RETAIL_SOURCECONTEXT_H
#define ENGINE_SOURCE_RETAIL_SOURCECONTEXT_H
#include <initializer_list>

namespace Retail {
enum class SourceKind { Retail, Forever };
inline thread_local SourceKind currentSource = SourceKind::Retail;
inline const char* SourcePrefix() { return currentSource == SourceKind::Forever ? "forever/" : "retail/"; }
class SourceScope {
    SourceKind previous;
public:
    explicit SourceScope(SourceKind kind) : previous(currentSource) { currentSource = kind; }
    ~SourceScope() { currentSource = previous; }
    SourceScope(const SourceScope&) = delete;
    SourceScope& operator=(const SourceScope&) = delete;
};
inline bool IsCdnPath(const char* name) {
    if (!name) return false;
    for (const char* prefix : {"retail/", "forever/"}) {
        const char* p = name;
        const char* q = prefix;
        while (*q && *p) {
            char c = *p++;
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            if (c == '\\') c = '/';
            if (c != *q) break;
            ++q;
        }
        if (!*q) return true;
    }
    return false;
}
}
#endif
