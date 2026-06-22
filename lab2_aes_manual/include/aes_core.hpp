#pragma once
// aes_core.hpp — AES-128 block cipher, pure C++ (FIPS-197)
// No crypto library dependency; only STL + aes_tables.hpp + gf256.hpp
#include <cstdint>
#include <array>

namespace aes2 {

// AES-128: 128-bit block, 128-bit key, 10 rounds.
//
// State layout: column-major 4×4 byte matrix.
//   state[r + 4*c] = byte at row r, column c   (r,c ∈ {0..3})
//   Input byte order: in[0]→state[0], in[1]→state[1], ..., in[15]→state[15]
//   i.e. in[0..3] fill column 0, in[4..7] fill column 1, etc.
class AesCore {
public:
    // Construct from 16-byte key; runs KeyExpansion immediately.
    explicit AesCore(const uint8_t key[16]);
    explicit AesCore(const std::array<uint8_t,16>& key);

    // Encrypt one 16-byte block (in-place variant for CtrMode efficiency)
    void encryptBlock(const uint8_t in[16], uint8_t out[16]) const;

    // Decrypt one 16-byte block (needed for FIPS-197 test vectors & block-cipher modes)
    void decryptBlock(const uint8_t in[16], uint8_t out[16]) const;

    // Expose round keys for debugging / test (const view)
    const uint8_t* roundKey(int r) const { return rk_ + r * 16; }

private:
    uint8_t rk_[176]; // 11 round keys × 16 bytes = 176 bytes

    void keyExpansion(const uint8_t key[16]);

    // Round transforms (operate on 16-byte state array)
    static void addRoundKey(uint8_t state[16], const uint8_t rk[16]);
    static void subBytes(uint8_t state[16]);
    static void invSubBytes(uint8_t state[16]);
    static void shiftRows(uint8_t state[16]);
    static void invShiftRows(uint8_t state[16]);
    static void mixColumns(uint8_t state[16]);
    static void invMixColumns(uint8_t state[16]);
};

} // namespace aes2
