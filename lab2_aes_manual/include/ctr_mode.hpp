#pragma once
// ctr_mode.hpp — AES-128-CTR streaming cipher (NIST SP 800-38A)
//
// Counter block = IV || counter (big-endian, full 128 bits).
// S_i = AES_enc(K, counter_i);  C_i = P_i XOR S_i
// Increment: +1 to counter_i big-endian (byte 15 = least significant).
// Enc = Dec (keystream XOR), so one process() function handles both.
// No padding: partial last block XORs only the remaining bytes.
#include "aes_core.hpp"
#include <cstddef>
#include <cstdint>

namespace aes2 {

class CtrMode {
public:
    // aes must outlive this CtrMode instance
    explicit CtrMode(const AesCore& aes) : aes_(aes) {}

    // Process (encrypt or decrypt) len bytes.
    // iv: 16-byte initial counter block (the nonce IS the counter start).
    // in/out may alias (in-place) only if in == out.
    void process(const uint8_t* in, uint8_t* out, size_t len,
                 const uint8_t iv[16]) const;

    // Increment 128-bit big-endian counter by 1
    static void increment(uint8_t ctr[16]) noexcept;

private:
    const AesCore& aes_;
};

} // namespace aes2
