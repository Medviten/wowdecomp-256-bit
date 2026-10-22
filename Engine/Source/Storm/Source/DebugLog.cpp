#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <windows.h>
#include "Engine/Source/Storm/H/storm.h"

#if !defined(NDEBUG) || defined(WOWDECOMP_DEBUG_LOG) // Custom: see storm.h
void DebugLog(const char* format, ...) {
#if defined(WOWDECOMP_DEBUG_LOG)
    // Custom: the scene renderer logs every batch it draws; that would flood the output every frame
    if (!strncmp(format, "Batch", 5)) {
        return;
    }
#endif

    char buffer[1024];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    OutputDebugStringA(buffer);

    size_t length = strlen(buffer);

    if (length == 0 || buffer[length - 1] != '\n') {
        OutputDebugStringA("\n");
    }
}

void DebugLog(int32_t target, const char* format, ...) {
    char buffer[1024];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    DebugLog("%s", buffer);
}

void DebugValidate(const char* expression) {
    DebugLog("Validation failed: %s", expression);
}
#endif
