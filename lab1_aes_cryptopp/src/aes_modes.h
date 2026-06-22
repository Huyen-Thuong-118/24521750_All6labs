#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// aes_modes.h — AES với Crypto++, 8 modes: ECB CBC OFB CFB CTR XTS CCM GCM
// Lab 1 constraint: Crypto++ only.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <stdexcept>

namespace aes1 {

// ── Mode enum ────────────────────────────────────────────────────────────────
enum class Mode { ECB, CBC, OFB, CFB, CTR, XTS, CCM, GCM };

Mode parse_mode(const std::string& s);  // "gcm" → Mode::GCM; throws on unknown
std::string mode_name(Mode m);           // Mode::GCM → "AES-GCM"

// ── Constants ────────────────────────────────────────────────────────────────
constexpr size_t BLOCK   = 16;          // AES block = 16 bytes
constexpr size_t TAG_LEN = 16;          // GCM/CCM auth tag = 16 bytes
constexpr size_t GCM_IV  = 12;          // recommended GCM nonce = 96 bits
constexpr size_t CCM_IV  = 13;          // CCM nonce = 13 bytes (common)

// ── Result struct ─────────────────────────────────────────────────────────────
struct CipherResult {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> iv;    // generated IV (if caller didn't supply)
    std::vector<uint8_t> tag;   // populated for AEAD modes only
};

// ── Core encrypt/decrypt ──────────────────────────────────────────────────────
//
// key  : 16/24/32 bytes for AES-128/192/256
//        For XTS: 32/48/64 bytes (double key — two concatenated AES keys)
// iv   : may be empty → auto-generated (caller gets it back via result.iv)
//        For ECB: ignored
//        For XTS: 16-byte tweak
//        For GCM: recommend 12 bytes
//        For CCM: recommend 13 bytes
// aad  : Additional Authenticated Data (GCM/CCM only, may be empty)
//
// Encrypt returns CipherResult. Decrypt returns plaintext bytes.
// All failures throw std::runtime_error.

CipherResult encrypt(Mode m,
                     const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& iv,   // empty → auto-gen
                     const std::vector<uint8_t>& plain,
                     const std::vector<uint8_t>& aad = {});

std::vector<uint8_t> decrypt(Mode m,
                              const std::vector<uint8_t>& key,
                              const std::vector<uint8_t>& iv,
                              const std::vector<uint8_t>& cipher,
                              const std::vector<uint8_t>& tag = {},
                              const std::vector<uint8_t>& aad = {});

// ── IV generation ─────────────────────────────────────────────────────────────
std::vector<uint8_t> generate_iv(size_t len);   // uses AutoSeededRandomPool

// ── Correct IV length for each mode ──────────────────────────────────────────
size_t required_iv_len(Mode m);  // 0 = no IV (ECB)

} // namespace aes1
