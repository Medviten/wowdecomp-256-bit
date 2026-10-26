#ifndef CHARACTER_BODY_SHAPE_WIRE_H
#define CHARACTER_BODY_SHAPE_WIRE_H

#include <array>
#include <cstddef>
#include <cstdint>

// Shared verbatim between the client (Engine/Source/Retail/BodyShapeWire.h) and the server
// (src/server/shared/BodyShapeWire.h). Do not let the copies drift.
//
// A character's body sliders travel as one fixed payload: a version byte, then 48 signed bytes, one per slot.
// A slot holds round(value * 100) for its slider (-100..100; the two mesh sliders are 0..100). The slot -> slider
// assignment below is the contract: never reorder or reuse a slot, only append (and bump Version if the meaning of
// an existing slot changes). Unassigned slots must be zero.
//
// Framing (same scheme as the eye-lightning footer and the HXE character-list tail):
//   - client -> server: an optional footer of payload + 'BSH1' appended after the normal create / customize /
//     faction / race change payload and after any EYE1 footer, so it is the last thing in the packet.
//   - server -> client: a tail block after SMSG_CHAR_ENUM: per character guid (8) + payload, then count (u32) and
//     'BSE1' (u32), stripped by the native client before stock parsing.
namespace CharacterBodyShape
{
    inline constexpr uint8_t Version = 1;
    inline constexpr size_t Slots = 48;
    inline constexpr size_t PayloadSize = 1 + Slots;
    inline constexpr uint32_t FooterMagic = 0x31485342; // "BSH1"
    inline constexpr uint32_t EnumMagic = 0x31455342;   // "BSE1"
    inline constexpr size_t FooterSize = PayloadSize + 4;
    inline constexpr size_t EnumStride = 8 + PayloadSize;

    inline constexpr const char* SlotNames[] = {
        "BreastSize", "ButtSize", "Height", "HeadSize", "NeckLength", "ShoulderWidth", "ArmLength", "ArmGirth",
        "ChestSize", "WaistWidth", "TorsoLength", "HipWidth", "LegLength", "LegGirth", "JawSize", "JawLength",
        "FaceWidth", "FaceLength", "BrowHeight", "BrowDepth", "EyeSize", "EyeSpacing", "EyeHeight", "NoseSize",
        "NoseWidth", "NoseLength", "NoseHeight", "MouthWidth", "LipFullness", "MouthHeight", "ChinLength", "ChinDepth",
        "CheekWidth", "CheekHeight", "JawWidth", "EarSize", "EarHeight",
    };
    inline constexpr size_t AssignedSlots = sizeof(SlotNames) / sizeof(SlotNames[0]);

    using Values = std::array<int8_t, Slots>;

    constexpr bool MeshSlot(size_t slot)
    {
        return slot < 2; // BreastSize, ButtSize
    }

    inline bool Valid(Values const& values)
    {
        for (size_t i = 0; i < Slots; ++i)
        {
            int const v = values[i];
            if (v < -100 || v > 100 || (i >= AssignedSlots && v != 0) || (MeshSlot(i) && v < 0))
                return false;
        }
        return true;
    }

    inline bool AllDefault(Values const& values)
    {
        for (int8_t v : values)
            if (v != 0)
                return false;
        return true;
    }

    inline void WritePayload(uint8_t* out, Values const& values)
    {
        out[0] = Version;
        for (size_t i = 0; i < Slots; ++i)
            out[1 + i] = static_cast<uint8_t>(values[i]);
    }

    // False for a wrong version or out-of-range values; `values` is only written when the payload is good.
    inline bool ReadPayload(uint8_t const* in, size_t size, Values& values)
    {
        if (!in || size != PayloadSize || in[0] != Version)
            return false;
        Values read{};
        for (size_t i = 0; i < Slots; ++i)
            read[i] = static_cast<int8_t>(in[1 + i]);
        if (!Valid(read))
            return false;
        values = read;
        return true;
    }

    // Strip an optional footer after the already-defined payload. Absent footer: present stays false, true is
    // returned. A footer that is present but malformed returns false. ByteBuffer::resize resets its read cursor, so
    // explicitly preserve it.
    template <class Packet>
    bool ReadTail(Packet& packet, bool& present, Values& values)
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
        if (!ReadPayload(tail, PayloadSize, values))
            return false;
        packet.resize(packet.size() - FooterSize);
        packet.rpos(read);
        present = true;
        return true;
    }
}

#endif
