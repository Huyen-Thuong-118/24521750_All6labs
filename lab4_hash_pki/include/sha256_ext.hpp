#pragma once
#include <cstdint>
#include <vector>

// SHA-256 Length-Extension Attack
// Attack on naive MAC = SHA256(key || message)
//
// Given  : H = SHA256(key || msg),  len(key),  msg
// Compute: SHA256(key || msg || sha256_padding || extension)
// WITHOUT knowing key.
//
// This works because SHA-256 uses Merkle-Damgård:
//   internal state after processing (key || msg || padding) = H
//   → restart compression with that state

namespace sha256_ext {

// Result of length-extension attack
struct ExtResult {
    std::vector<uint8_t> forged_hash;     // SHA256(key || msg || pad || ext)
    std::vector<uint8_t> forged_message;  // msg || pad(key||msg) || ext
                                           // (what verifier hashes with key prefix)
};

// Compute SHA-256 padding bytes appended for a message of total_len bytes
std::vector<uint8_t> sha256_padding(size_t total_len);

// Perform the length-extension
// hash_h     : 32-byte SHA-256 output of SHA256(key || msg)
// key_len    : byte length of key (secret, not needed for attack)
// msg        : original message bytes (not including key)
// extension  : bytes to append after the forged boundary
ExtResult extend(const std::vector<uint8_t>& hash_h,
                 size_t key_len,
                 const std::vector<uint8_t>& msg,
                 const std::vector<uint8_t>& extension);

// Verify by computing SHA256(key || forged_message) and comparing to forged_hash
// (used in tests to confirm the attack is correct)
bool verify_attack(const std::vector<uint8_t>& key,
                   const std::vector<uint8_t>& forged_message,
                   const std::vector<uint8_t>& forged_hash);

} // namespace sha256_ext
