// ctr_mode.cpp — AES-128-CTR mode (NIST SP 800-38A §6.5)
#include "ctr_mode.hpp"
#include <cstring>

namespace aes2 {

// Increment 128-bit big-endian counter: byte 15 is least significant.
void CtrMode::increment(uint8_t ctr[16]) noexcept {
    for (int i = 15; i >= 0; --i) {
        if (++ctr[i] != 0) break; // no carry → stop
    }
    // If all bytes wrapped (extremely rare — 2^128 blocks), counter silently
    // wraps to 0. The caller must ensure this never happens in practice
    // (= never process more than 2^128 × 16 bytes with one IV).
}

// process: encrypt or decrypt len bytes using CTR mode.
// S_i = AES_encrypt(K, ctr_i);  output[i*16 .. +16] = input[i*16 ..] XOR S_i
// Partial final block: XOR only the remaining (len % 16) bytes of keystream.
void CtrMode::process(const uint8_t* in, uint8_t* out, size_t len,
                      const uint8_t iv[16]) const
{
    uint8_t ctr[16];
    uint8_t ks[16];   // keystream block
    std::memcpy(ctr, iv, 16);

    size_t offset = 0;
    while (offset < len) {
        // Generate keystream block for current counter
        aes_.encryptBlock(ctr, ks);
        increment(ctr);

        // XOR as many bytes as remain (up to 16)
        size_t block_bytes = len - offset;
        if (block_bytes > 16) block_bytes = 16;

        for (size_t b = 0; b < block_bytes; ++b)
            out[offset + b] = in[offset + b] ^ ks[b];

        offset += block_bytes;
    }
}

} // namespace aes2
