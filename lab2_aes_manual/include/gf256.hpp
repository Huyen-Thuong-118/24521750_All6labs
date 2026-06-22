#pragma once
// gf256.hpp — Arithmetic in GF(2^8) = GF(2)[x] / (x^8+x^4+x^3+x+1)
// Used by MixColumns and InvMixColumns in AES.
// All functions are constexpr-friendly and branchless where possible.
#include <cstdint>

namespace aes2 {

// xtime: multiply by x (i.e., by 0x02) in GF(2^8)
// = left shift 1 bit; if bit 7 was set, XOR with 0x1B (reducing polynomial)
constexpr uint8_t xtime(uint8_t x) noexcept {
    return static_cast<uint8_t>((x << 1) ^ ((x >> 7) * 0x1Bu));
}

// GF(2^8) multiplication: repeated xtime + XOR
// Implemented as a 8-step Russian peasant multiply — constant-time for fixed b
constexpr uint8_t gf_mul(uint8_t a, uint8_t b) noexcept {
    uint8_t r = 0;
    for (int i = 0; i < 8; ++i) {
        if (b & 1u) r ^= a;
        uint8_t hi = a >> 7;
        a = static_cast<uint8_t>(a << 1);
        if (hi) a ^= 0x1Bu;
        b >>= 1;
    }
    return r;
}

// Convenience multipliers used in MixColumns / InvMixColumns
constexpr uint8_t mul2(uint8_t x)  noexcept { return xtime(x); }
constexpr uint8_t mul3(uint8_t x)  noexcept { return xtime(x) ^ x; }
constexpr uint8_t mul9(uint8_t x)  noexcept { return xtime(xtime(xtime(x))) ^ x; }
constexpr uint8_t mul11(uint8_t x) noexcept { return xtime(xtime(xtime(x))) ^ xtime(x) ^ x; }
constexpr uint8_t mul13(uint8_t x) noexcept { return xtime(xtime(xtime(x))) ^ xtime(xtime(x)) ^ x; }
constexpr uint8_t mul14(uint8_t x) noexcept { return xtime(xtime(xtime(x))) ^ xtime(xtime(x)) ^ xtime(x); }

} // namespace aes2
