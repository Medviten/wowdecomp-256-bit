#ifndef CHARACTER_EYE_EFFECTS_H
#define CHARACTER_EYE_EFFECTS_H

#include <array>
#include <cstddef>
#include <cstdint>

// Shared verbatim with the server's EyeEffects.h. Existing appearance payloads
// use at most 48 extension bits; bit 63 is a separate, opt-in character cosmetic.
namespace CharacterEyeEffects
{
    inline constexpr uint64_t LightningMask = uint64_t{1} << 63;
    inline constexpr uint32_t LightningWordMask = uint32_t{1} << 31;
    inline constexpr uint32_t TrailerMagic = 0x31455945; // EYE1
    inline constexpr size_t TrailerSize = 5;

    constexpr uint64_t Appearance(uint64_t extra)
    {
        return extra & ~LightningMask;
    }
    constexpr bool Lightning(uint64_t extra)
    {
        return (extra & LightningMask) != 0;
    }
    constexpr uint64_t WithLightning(uint64_t extra, bool enabled)
    {
        return Appearance(extra) | (enabled ? LightningMask : 0);
    }
    constexpr std::array<uint8_t, TrailerSize> Trailer(bool enabled)
    {
        return {uint8_t(enabled), 0x45, 0x59, 0x45, 0x31};
    }
    inline bool ReadTrailer(uint8_t const* bytes, size_t size, bool& enabled)
    {
        if (!bytes || size != TrailerSize || bytes[0] > 1
            || bytes[1] != 0x45 || bytes[2] != 0x59 || bytes[3] != 0x45 || bytes[4] != 0x31)
            return false;
        enabled = bytes[0] != 0;
        return true;
    }

    // Strip only an optional footer after the already-defined profile payload.
    // ByteBuffer::resize resets its read cursor, so explicitly preserve that cursor.
    template <class Packet>
    bool ReadTail(Packet& packet, size_t appearanceBytes, bool& present, bool& enabled)
    {
        size_t const read = packet.rpos();
        if (read > packet.size())
            return false;
        size_t const remaining = packet.size() - read;
        if (remaining == appearanceBytes)
            return true;
        if (remaining != appearanceBytes + TrailerSize
            || !ReadTrailer(packet.contents() + packet.size() - TrailerSize, TrailerSize, enabled))
            return false;
        packet.resize(packet.size() - TrailerSize);
        packet.rpos(read);
        present = true;
        return true;
    }
}

#endif
