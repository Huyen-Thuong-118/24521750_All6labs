#include "sha256_ext.hpp"
#include <stdexcept>
#include <cstring>

// Manual SHA-256 implementation for length-extension attack.
// Lets us initialize from an arbitrary internal state (the stolen hash).

namespace sha256_ext {

// SHA-256 round constants (first 64 primes, cube-root fractional parts)
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

// One SHA-256 block compression
static void compress(uint32_t h[8], const uint8_t blk[64]) {
    uint32_t W[64];
    for (int i = 0; i < 16; i++)
        W[i] = ((uint32_t)blk[4*i]   << 24) | ((uint32_t)blk[4*i+1] << 16)
             | ((uint32_t)blk[4*i+2] <<  8) |  (uint32_t)blk[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(W[i-15], 7) ^ rotr(W[i-15], 18) ^ (W[i-15] >> 3);
        uint32_t s1 = rotr(W[i-2], 17) ^ rotr(W[i-2],  19) ^ (W[i-2]  >> 10);
        W[i] = s1 + W[i-7] + s0 + W[i-16];
    }
    uint32_t a=h[0], b=h[1], c=h[2], d=h[3],
             e=h[4], f=h[5], g=h[6], hv=h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1  = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        uint32_t ch  = (e & f) ^ (~e & g);
        uint32_t T1  = hv + S1 + ch + K[i] + W[i];
        uint32_t S0  = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t T2  = S0 + maj;
        hv=g; g=f; f=e; e=d+T1; d=c; c=b; b=a; a=T1+T2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
    h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hv;
}

// Full SHA-256 from a given initial state h[8], over arbitrary byte sequence
static std::vector<uint8_t> sha256_from_state(uint32_t h[8],
                                               const uint8_t* data, size_t data_len,
                                               uint64_t processed_before)
{
    // Build: data || sha256_padding(processed_before + data_len)
    uint64_t total_bits = (processed_before + data_len) * 8;
    size_t pad_start = (processed_before + data_len) % 64;
    size_t zero_bytes = (pad_start < 56) ? (55 - pad_start) : (55 + 64 - pad_start);

    std::vector<uint8_t> msg(data, data + data_len);
    msg.push_back(0x80);
    msg.insert(msg.end(), zero_bytes, 0x00);
    for (int i = 7; i >= 0; --i)
        msg.push_back((total_bits >> (i * 8)) & 0xFF);

    for (size_t i = 0; i < msg.size(); i += 64)
        compress(h, msg.data() + i);

    std::vector<uint8_t> out(32);
    for (int i = 0; i < 8; i++) {
        out[4*i]   = (h[i] >> 24) & 0xFF;
        out[4*i+1] = (h[i] >> 16) & 0xFF;
        out[4*i+2] = (h[i] >>  8) & 0xFF;
        out[4*i+3] = (h[i]      ) & 0xFF;
    }
    return out;
}

std::vector<uint8_t> sha256_padding(size_t total_len) {
    size_t pad_start = total_len % 64;
    size_t zero_bytes = (pad_start < 56) ? (55 - pad_start) : (55 + 64 - pad_start);
    std::vector<uint8_t> pad;
    pad.push_back(0x80);
    pad.insert(pad.end(), zero_bytes, 0x00);
    uint64_t bitlen = (uint64_t)total_len * 8;
    for (int i = 7; i >= 0; --i)
        pad.push_back((bitlen >> (i * 8)) & 0xFF);
    return pad;
}

ExtResult extend(const std::vector<uint8_t>& hash_h,
                 size_t key_len,
                 const std::vector<uint8_t>& msg,
                 const std::vector<uint8_t>& extension)
{
    if (hash_h.size() != 32)
        throw std::runtime_error("hash_h must be exactly 32 bytes");

    uint32_t h[8];
    for (int i = 0; i < 8; i++)
        h[i] = ((uint32_t)hash_h[4*i]   << 24) | ((uint32_t)hash_h[4*i+1] << 16)
             | ((uint32_t)hash_h[4*i+2] <<  8) |  (uint32_t)hash_h[4*i+3];

    size_t inner_len = key_len + msg.size();
    auto pad = sha256_padding(inner_len);

    size_t processed = inner_len + pad.size(); 

    std::vector<uint8_t> forged_message;
    forged_message.insert(forged_message.end(), msg.begin(), msg.end());
    forged_message.insert(forged_message.end(), pad.begin(), pad.end());
    forged_message.insert(forged_message.end(), extension.begin(), extension.end());

    auto forged_hash = sha256_from_state(h,
                                         extension.data(), extension.size(),
                                         processed);

    return {forged_hash, forged_message};
}

bool verify_attack(const std::vector<uint8_t>& key,
                   const std::vector<uint8_t>& forged_message,
                   const std::vector<uint8_t>& forged_hash)
{
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    std::vector<uint8_t> data;
    data.insert(data.end(), key.begin(), key.end());
    data.insert(data.end(), forged_message.begin(), forged_message.end());

    auto computed = sha256_from_state(h, data.data(), data.size(), 0);
    return computed == forged_hash;
}

} // namespace sha256_ext
