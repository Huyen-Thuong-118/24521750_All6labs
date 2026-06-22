#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rsa3 {

// Hybrid envelope: RSA-OAEP(AES-256 session key) + AES-256-GCM(data)
struct Envelope {
    std::string mode;                  // "RSA-OAEP" or "RSA-OAEP-AES-GCM"
    int         rsa_modulus;           // 3072 / 4096
    std::string label;                 // optional OAEP label (stored, verified on decrypt)
    // For direct RSA-OAEP mode (small messages):
    std::vector<uint8_t> rsa_ct;       // RSA-OAEP ciphertext of plaintext
    // For hybrid mode (large messages):
    std::vector<uint8_t> wrapped_key;  // RSA-OAEP encrypted AES-256 session key (32 B)
    std::vector<uint8_t> iv;           // 12-byte GCM nonce
    std::vector<uint8_t> tag;          // 16-byte GCM tag
    std::vector<uint8_t> ciphertext;   // AES-GCM ciphertext
};

// Auto-select direct RSA-OAEP or hybrid based on plaintext size
Envelope encrypt(const std::vector<uint8_t>& pub_der,
                 const std::vector<uint8_t>& plain,
                 const std::string& label = "");

std::vector<uint8_t> decrypt(const std::vector<uint8_t>& priv_der,
                              const Envelope& env,
                              const std::string& label = "");

// Serialize to bytes: JSON header + "\n---\n" + raw ciphertext
std::vector<uint8_t> serialize  (const Envelope& env);
Envelope             deserialize(const std::vector<uint8_t>& data);

} // namespace rsa3
