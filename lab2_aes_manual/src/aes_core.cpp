// aes_core.cpp — AES-128 implementation (FIPS-197)
// Pure C++17, no crypto libraries. Verified against FIPS-197 Appendix B/C.
//
// State: 16-byte flat array, column-major layout.
//   state[row + 4*col], row ∈ {0..3}, col ∈ {0..3}
//   → state[0..3] = column 0 (top to bottom)
//   → state[4..7] = column 1, etc.
//
// This matches FIPS-197 §3.4 byte ordering:
//   s_{r,c} = in[r + 4c]
#include "aes_core.hpp"
#include "aes_tables.hpp"
#include "gf256.hpp"
#include <cstring>
#include <stdexcept>

namespace aes2 {

// ── Constructors ─────────────────────────────────────────────────────────────

AesCore::AesCore(const uint8_t key[16])       { keyExpansion(key); }
AesCore::AesCore(const std::array<uint8_t,16>& key) { keyExpansion(key.data()); }

// ── KeyExpansion (FIPS-197 §5.2) ─────────────────────────────────────────────
// Generates 44 words (11 round keys × 4 words × 4 bytes = 176 bytes).
void AesCore::keyExpansion(const uint8_t key[16]) {
    // Copy key into w[0..3]
    std::memcpy(rk_, key, 16);

    for (int i = 4; i < 44; ++i) {
        uint8_t temp[4];
        // temp = w[i-1]
        std::memcpy(temp, rk_ + (i-1)*4, 4);

        if (i % 4 == 0) {
            // RotWord: [t0,t1,t2,t3] → [t1,t2,t3,t0]
            uint8_t t = temp[0];
            temp[0] = temp[1]; temp[1] = temp[2]; temp[2] = temp[3]; temp[3] = t;
            // SubWord: apply S-box to each byte
            temp[0] = SBOX[temp[0]]; temp[1] = SBOX[temp[1]];
            temp[2] = SBOX[temp[2]]; temp[3] = SBOX[temp[3]];
            // XOR with Rcon[i/4]
            temp[0] ^= RCON[i / 4];
        }

        // w[i] = w[i-4] XOR temp
        for (int b = 0; b < 4; ++b)
            rk_[i*4 + b] = rk_[(i-4)*4 + b] ^ temp[b];
    }
}

// ── AddRoundKey ───────────────────────────────────────────────────────────────
// XOR state with round key (same for enc and dec — XOR is self-inverse)
void AesCore::addRoundKey(uint8_t state[16], const uint8_t rk[16]) {
    for (int i = 0; i < 16; ++i) state[i] ^= rk[i];
}

// ── SubBytes / InvSubBytes ────────────────────────────────────────────────────
void AesCore::subBytes(uint8_t state[16]) {
    for (int i = 0; i < 16; ++i) state[i] = SBOX[state[i]];
}
void AesCore::invSubBytes(uint8_t state[16]) {
    for (int i = 0; i < 16; ++i) state[i] = INV_SBOX[state[i]];
}

// ── ShiftRows ─────────────────────────────────────────────────────────────────
// Row r (indices r, r+4, r+8, r+12) shifts LEFT by r positions.
//   Row 0: no change
//   Row 1: [s1,s5,s9,s13] → [s5,s9,s13,s1]
//   Row 2: [s2,s6,s10,s14] → [s10,s14,s2,s6]
//   Row 3: [s3,s7,s11,s15] → [s15,s3,s7,s11]   (left 3 = right 1)
void AesCore::shiftRows(uint8_t s[16]) {
    uint8_t t;
    // Row 1 — left 1
    t=s[1]; s[1]=s[5]; s[5]=s[9]; s[9]=s[13]; s[13]=t;
    // Row 2 — left 2
    t=s[2];  s[2]=s[10];  s[10]=t;
    t=s[6];  s[6]=s[14];  s[14]=t;
    // Row 3 — left 3 (= right 1)
    t=s[15]; s[15]=s[11]; s[11]=s[7]; s[7]=s[3]; s[3]=t;
}

// ── InvShiftRows ──────────────────────────────────────────────────────────────
// Row r shifts RIGHT by r positions.
//   Row 1: right 1 → [s1,s5,s9,s13] ← [s13,s1,s5,s9]
void AesCore::invShiftRows(uint8_t s[16]) {
    uint8_t t;
    // Row 1 — right 1
    t=s[13]; s[13]=s[9]; s[9]=s[5]; s[5]=s[1]; s[1]=t;
    // Row 2 — right 2
    t=s[2];  s[2]=s[10];  s[10]=t;
    t=s[6];  s[6]=s[14];  s[14]=t;
    // Row 3 — right 3 (= left 1)
    t=s[3]; s[3]=s[7]; s[7]=s[11]; s[11]=s[15]; s[15]=t;
}

// ── MixColumns ────────────────────────────────────────────────────────────────
// Each column (s[4c..4c+3]) multiplied by circulant matrix [2,3,1,1] in GF(2^8).
// FIPS-197 §5.1.3 matrix:
//   s0' = 2*s0 ^ 3*s1 ^ 1*s2 ^ 1*s3
//   s1' = 1*s0 ^ 2*s1 ^ 3*s2 ^ 1*s3
//   s2' = 1*s0 ^ 1*s1 ^ 2*s2 ^ 3*s3
//   s3' = 3*s0 ^ 1*s1 ^ 1*s2 ^ 2*s3
void AesCore::mixColumns(uint8_t s[16]) {
    for (int c = 0; c < 4; ++c) {
        uint8_t s0=s[4*c], s1=s[4*c+1], s2=s[4*c+2], s3=s[4*c+3];
        s[4*c  ] = mul2(s0) ^ mul3(s1) ^     s2  ^     s3;
        s[4*c+1] =     s0  ^ mul2(s1) ^ mul3(s2) ^     s3;
        s[4*c+2] =     s0  ^     s1   ^ mul2(s2) ^ mul3(s3);
        s[4*c+3] = mul3(s0) ^     s1  ^     s2   ^ mul2(s3);
    }
}

// ── InvMixColumns ─────────────────────────────────────────────────────────────
// Inverse matrix [14,11,13,9] in GF(2^8).
//   s0' = 14*s0 ^ 11*s1 ^ 13*s2 ^  9*s3
//   s1' =  9*s0 ^ 14*s1 ^ 11*s2 ^ 13*s3
//   s2' = 13*s0 ^  9*s1 ^ 14*s2 ^ 11*s3
//   s3' = 11*s0 ^ 13*s1 ^  9*s2 ^ 14*s3
void AesCore::invMixColumns(uint8_t s[16]) {
    for (int c = 0; c < 4; ++c) {
        uint8_t s0=s[4*c], s1=s[4*c+1], s2=s[4*c+2], s3=s[4*c+3];
        s[4*c  ] = mul14(s0) ^ mul11(s1) ^ mul13(s2) ^  mul9(s3);
        s[4*c+1] =  mul9(s0) ^ mul14(s1) ^ mul11(s2) ^ mul13(s3);
        s[4*c+2] = mul13(s0) ^  mul9(s1) ^ mul14(s2) ^ mul11(s3);
        s[4*c+3] = mul11(s0) ^ mul13(s1) ^  mul9(s2) ^ mul14(s3);
    }
}

// ── encryptBlock (10 rounds) ──────────────────────────────────────────────────
// Structure (FIPS-197 §5.1):
//   AddRoundKey(state, w[0..3])
//   for r = 1..9: SubBytes → ShiftRows → MixColumns → AddRoundKey(w[4r..4r+3])
//   SubBytes → ShiftRows → AddRoundKey(w[40..43])   (no MixColumns in round 10!)
void AesCore::encryptBlock(const uint8_t in[16], uint8_t out[16]) const {
    uint8_t state[16];
    std::memcpy(state, in, 16);

    addRoundKey(state, rk_);             // Initial round key (round 0)

    for (int r = 1; r <= 9; ++r) {
        subBytes(state);
        shiftRows(state);
        mixColumns(state);               // ← present in rounds 1..9
        addRoundKey(state, rk_ + r*16);
    }

    // Round 10: no MixColumns
    subBytes(state);
    shiftRows(state);
    addRoundKey(state, rk_ + 10*16);

    std::memcpy(out, state, 16);
}

// ── decryptBlock (10 rounds, equivalent-inverse cipher) ─────────────────────
// Structure (FIPS-197 §5.3):
//   AddRoundKey(state, w[40..43])
//   for r = 9..1: InvShiftRows → InvSubBytes → AddRoundKey → InvMixColumns
//   InvShiftRows → InvSubBytes → AddRoundKey(w[0..3])
void AesCore::decryptBlock(const uint8_t in[16], uint8_t out[16]) const {
    uint8_t state[16];
    std::memcpy(state, in, 16);

    addRoundKey(state, rk_ + 10*16);    // Start with round key 10

    for (int r = 9; r >= 1; --r) {
        invShiftRows(state);
        invSubBytes(state);
        addRoundKey(state, rk_ + r*16);
        invMixColumns(state);           // ← present in rounds 9..1
    }

    // Final round (round 0): no InvMixColumns
    invShiftRows(state);
    invSubBytes(state);
    addRoundKey(state, rk_);

    std::memcpy(out, state, 16);
}

} // namespace aes2
