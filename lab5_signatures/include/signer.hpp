#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Lab 5 — ECDSA-P256 (RFC 6979 deterministic) + RSA-PSS-3072 (SHA-256, salt=32)
// All signatures returned/accepted in DER format by default.

namespace sig5 {

enum class Algo { ECDSA_P256, RSA_PSS_3072 };

// Generate a new keypair.  Returns {priv_pem, pub_pem}.
std::pair<std::string, std::string> keygen(Algo a);

// Load a private key from PEM and sign msg.
// Returns DER-encoded signature.
std::vector<uint8_t> sign_msg(Algo a,
                               const std::string& priv_pem,
                               const std::vector<uint8_t>& msg);

// Load a public key from PEM and verify a DER-encoded signature against msg.
// Returns true iff valid; never throws on invalid sig (fail closed).
bool verify_msg(Algo a,
                const std::string& pub_pem,
                const std::vector<uint8_t>& msg,
                const std::vector<uint8_t>& sig_der);

// Batch verify: returns number of valid signatures
int verify_batch(Algo a,
                  const std::string& pub_pem,
                  const std::vector<std::vector<uint8_t>>& msgs,
                  const std::vector<std::vector<uint8_t>>& sigs);

// Encoding helpers (for --encode flag in CLI)
std::string  sig_to_base64(const std::vector<uint8_t>& der);
std::vector<uint8_t> sig_from_base64(const std::string& b64);

} // namespace sig5
