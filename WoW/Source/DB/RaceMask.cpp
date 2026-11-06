#include "WoW/Source/DB/RaceMask.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"

// Custom: 32-bit / 256-bit race mask formats (see RaceMask.h). None of this exists in the original client.

// Words per mask in the DBC currently being loaded
static int32_t s_dbcReadWords = RaceMask::s_numWords;

// Set once any race-mask DBC is loaded in the 256-bit layout
static bool s_dbcWide = false;

// raceMaskBits CVar: 32, 256, or 0 to follow the DBCs
static int32_t s_packetBits = 0;

void RaceMask::SetDBCWords(int32_t words) {
    s_dbcReadWords = words == 1 ? 1 : RaceMask::s_numWords;

    if (s_dbcReadWords == RaceMask::s_numWords) {
        s_dbcWide = true;
    }
}

int32_t RaceMask::GetPacketWords() {
    if (s_packetBits == 32) {
        return 1;
    }

    if (s_packetBits == 256) {
        return RaceMask::s_numWords;
    }

    return s_dbcWide ? RaceMask::s_numWords : 1;
}

void RaceMask::SetPacketBits(int32_t bits) {
    s_packetBits = bits == 32 || bits == 256 ? bits : 0;
}

bool RaceMask::ReadDBC(SFileImpl* file, RaceMask* masks, int32_t count) {
    bool ok = true;

    for (int32_t i = 0; i < count; i++) {
        masks[i] = RaceMask::None();

        if (!SFileReadFile(file, masks[i].m_words, sizeof(uint32_t) * s_dbcReadWords, nullptr, nullptr, 0)) {
            ok = false;
        }
    }

    return ok;
}

void RaceMask::GetPacket(CDataStore* msg) {
    *this = RaceMask::None();
    int32_t words = RaceMask::GetPacketWords();

    for (int32_t i = 0; i < words; i++) {
        msg->Get(this->m_words[i]);
    }
}

void RaceMask::PutPacket(CDataStore* msg) const {
    int32_t words = RaceMask::GetPacketWords();

    for (int32_t i = 0; i < words; i++) {
        msg->Put(this->m_words[i]);
    }
}
