// ctr_mode.cpp — AES-128-CTR mode (NIST SP 800-38A §6.5)
#include "ctr_mode.hpp"
#include <cstring>

namespace aes2 {

void CtrMode::increment(uint8_t ctr[16]) noexcept {
    for (int i = 15; i >= 0; --i) {
        if (++ctr[i] != 0) break; // no carry → stop
    }
}

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
