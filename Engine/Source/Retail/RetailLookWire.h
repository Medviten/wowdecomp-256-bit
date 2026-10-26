#ifndef CHARACTER_RETAIL_LOOK_WIRE_H
#define CHARACTER_RETAIL_LOOK_WIRE_H

#include <array>
#include <cstddef>
#include <cstdint>

// Shared verbatim between the client (Engine/Source/Retail/RetailLookWire.h) and the server
// (src/server/shared/RetailLookWire.h). Do not let the copies drift.
//
// A character's full Retail customization, as the creator holds it: one selected choice index per option of the
// race/sex model (the five stock bytes are included, so a remote client never has to infer them). The server treats it
// as an opaque, size-checked blob; only the client knows what the indices mean.
//
// Payload (132 bytes): version(1) race(1) sex(1) count(1), then 64 little-endian u16 indices. Indices past `count` are
// zero. The race/sex are the character's at the time of saving, so a stale look after a race change is ignored.
//
// Two Forms (Worgen Wolf/Human, Dracthyr Dragon/Visage): a look of the race's default model is version 1, as above.
// A look that names its form is version 2: the form (1..4, see MaxForm) sits in the high nibble of the sex byte, the
// size is unchanged. A reader that only knows version 1 rejects version 2 and falls back to the five stock bytes.
//
// Framing, same scheme as the body-slider and eye-lightning data:
//   - client -> server: optional footer = payload + 'RLK1', after everything else (so it is the last thing in the
//     create / customize / faction / race change packet, stripped before 'BSH1').
//   - server -> client: tail block after SMSG_CHAR_ENUM: per character guid(8) + payload, then count(u32) + 'RLE1'.
//   - server -> client in the world: SMSG_ESTERIA_RETAIL_LOOK = guid(8) + payload.
namespace CharacterRetailLook
{
    inline constexpr uint8_t Version = 1;
    inline constexpr uint8_t VersionForm = 2;
    inline constexpr uint8_t MaxForm = 4;
    inline constexpr size_t MaxOptions = 64;
    inline constexpr size_t PayloadSize = 4 + 2 * MaxOptions;
    inline constexpr uint32_t FooterMagic = 0x314B4C52; // "RLK1"
    inline constexpr uint32_t EnumMagic = 0x31454C52;   // "RLE1"
    inline constexpr uint32_t ParkedMagic = 0x31504C52; // "RLP1": same layout as RLE1, the Two Forms look that is not showing
    inline constexpr size_t FooterSize = PayloadSize + 4;
    inline constexpr size_t EnumStride = 8 + PayloadSize;

    struct Look
    {
        uint8_t race = 0;
        uint8_t sex = 0;
        uint8_t form = 0; // 0: the race's default model (version 1)
        uint8_t count = 0;
        std::array<uint16_t, MaxOptions> index{};
    };

    inline bool Valid(Look const& look)
    {
        if (!look.race || look.sex > 1 || look.form > MaxForm || look.count > MaxOptions)
            return false;
        for (size_t i = look.count; i < MaxOptions; ++i)
            if (look.index[i] != 0)
                return false;
        return true;
    }

    inline void WritePayload(uint8_t* out, Look const& look)
    {
        out[0] = look.form ? VersionForm : Version;
        out[1] = look.race;
        out[2] = static_cast<uint8_t>(look.sex | (look.form << 4));
        out[3] = look.count;
        for (size_t i = 0; i < MaxOptions; ++i)
        {
            out[4 + 2 * i] = static_cast<uint8_t>(look.index[i] & 0xFF);
            out[5 + 2 * i] = static_cast<uint8_t>(look.index[i] >> 8);
        }
    }

    // False for a wrong version or an invalid look; `look` is only written when the payload is good.
    inline bool ReadPayload(uint8_t const* in, size_t size, Look& look)
    {
        if (!in || size != PayloadSize || (in[0] != Version && in[0] != VersionForm))
            return false;
        Look read;
        read.race = in[1];
        read.sex = in[0] == VersionForm ? static_cast<uint8_t>(in[2] & 0x0F) : in[2];
        read.form = in[0] == VersionForm ? static_cast<uint8_t>(in[2] >> 4) : uint8_t{0};
        if (in[0] == VersionForm && read.form == 0)
            return false;
        read.count = in[3];
        for (size_t i = 0; i < MaxOptions; ++i)
            read.index[i] = static_cast<uint16_t>(in[4 + 2 * i] | (in[5 + 2 * i] << 8));
        if (!Valid(read))
            return false;
        look = read;
        return true;
    }

    // Strip an optional footer. Absent: present stays false and true is returned; present but malformed: false.
    // ByteBuffer::resize resets its read cursor, so explicitly preserve it.
    template <class Packet>
    bool ReadTail(Packet& packet, bool& present, Look& look)
    {
        size_t const read = packet.rpos();
        if (read > packet.size())
            return false;
        size_t const remaining = packet.size() - read;
        if (remaining < FooterSize)
            return true;
        uint8_t const* tail = packet.contents() + packet.size() - FooterSize;
        uint32_t const magic = uint32_t(tail[PayloadSize]) | uint32_t(tail[PayloadSize + 1]) << 8
            | uint32_t(tail[PayloadSize + 2]) << 16 | uint32_t(tail[PayloadSize + 3]) << 24;
        if (magic != FooterMagic)
            return true;
        if (!ReadPayload(tail, PayloadSize, look))
            return false;
        packet.resize(packet.size() - FooterSize);
        packet.rpos(read);
        present = true;
        return true;
    }
}

#endif
