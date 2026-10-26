#include "Engine/Source/Retail/RetailShader.h"
#include <cstring>

namespace Retail {
bool AddGlowLayer(const std::vector<uint8_t>& source, std::vector<uint8_t>& result, std::string& error) {
    result.clear();
    auto fail = [&] { error = "Unrecognized stock reflection shader permutation"; return false; };
    if (source.size() < 8 || source.size() % 4) return fail();
    std::vector<uint32_t> in(source.size() / 4), out;
    std::memcpy(in.data(), source.data(), source.size());
    if (in[0] != 0xffff0300 || in.back() != 0xffff) return fail();
    out.push_back(in[0]);
    bool sampler = false, glow = false;
    for (size_t p = 1; p < in.size();) {
        const uint32_t token = in[p], op = token & 0xffff;
        if (op == 0xffff) { if (p + 1 != in.size()) return fail(); out.push_back(token); break; }
        const size_t n = op == 0xfffe ? ((token >> 16) & 0x7fff) : ((token >> 24) & 15);
        if (n >= in.size() - p) return fail();
        // Reject collisions with the extension's temporary/constant registers.
        if (op != 0xfffe && op != 81) for (size_t j = p + 1; j <= p + n; ++j) {
            const auto v = in[j], type = ((v >> 28) & 7) | ((v >> 8) & 24), index = v & 2047;
            if ((v & 0x80000000) && ((type == 0 && index >= 30) || (type == 2 && index >= 16) || (type == 10 && index == 2)))
                return fail();
        }
        out.insert(out.end(), in.begin() + p, in.begin() + p + n + 1);
        if (op == 31 && n == 2 && in[p + 2] == 0xa00f0801) {
            out.insert(out.end(), {0x0200001f, 0x90000000, 0xa00f0802}); sampler = true;
        }
        // The final lighting MAD subtracts c2 before the existing fog blend.
        if (op == 4 && n == 4 && in[p + 4] == 0xa1e40002) {
            if (glow || !sampler || (in[p + 1] & 0xf0000) != 0x70000) return fail();
            const uint32_t destination = in[p + 1], reg = destination & 2047;
            out.insert(out.end(), {
                // UV3 = UV1.x * c16.xy + UV1.y * c17.xy + c18.xy
                0x03000005, 0x8003001e, 0x90000001, 0xa0e40010,
                0x04000004, 0x8003001e, 0x90550001, 0xa0e40011, 0x80e4001e,
                0x03000002, 0x8003001e, 0x80e4001e, 0xa0e40012,
                0x03000042, 0x800f001f, 0x80e4001e, 0xa0e40802,
                0x03000005, 0x8007001f, 0x80e4001f, 0xa0aa0012,
                0x04000004, destination, 0x80e4001f, 0x80ff001f, uint32_t(0x80e40000 | reg)
            });
            glow = true;
        }
        p += n + 1;
    }
    if (!sampler || !glow) return fail();
    result.resize(out.size() * 4); std::memcpy(result.data(), out.data(), result.size());
    return true;
}
}
