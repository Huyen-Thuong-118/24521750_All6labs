#pragma once
// Lab 6 — Post-Quantum Signatures (ML-DSA-44, FIPS 204) + KEM (ML-KEM-512, FIPS 203)
// Backend: OpenSSL 3.5+ native provider (no liboqs needed).
//
// ML-DSA-44 params:  pubkey=1312 B · privkey=2560 B · sig=2420 B
// ML-KEM-512 params: pubkey=800 B  · privkey=1632 B · ct=768 B  · ss=32 B

#include <cstdint>
#include <string>
#include <vector>

namespace pq {

// ─── ML-DSA-44 (FIPS 204) ────────────────────────────────────────────────────

// Returns {priv_pem, pub_pem}
std::pair<std::string, std::string> mldsa_keygen();

// Sign msg using priv_pem; returns raw signature (2420 B for ML-DSA-44)
std::vector<uint8_t> mldsa_sign(const std::string& priv_pem,
                                  const std::vector<uint8_t>& msg);

// Verify raw signature against msg using pub_pem.  Returns true iff valid.
bool mldsa_verify(const std::string& pub_pem,
                   const std::vector<uint8_t>& msg,
                   const std::vector<uint8_t>& sig);

// Batch verify: returns number of valid sigs (out of min(msgs, sigs) pairs)
int mldsa_verify_batch(const std::string& pub_pem,
                        const std::vector<std::vector<uint8_t>>& msgs,
                        const std::vector<std::vector<uint8_t>>& sigs);

// ─── ML-KEM-512 (FIPS 203) ───────────────────────────────────────────────────

// Returns {priv_pem, pub_pem}
std::pair<std::string, std::string> mlkem_keygen();

struct EncapsResult {
    std::vector<uint8_t> ciphertext;    // 768 B
    std::vector<uint8_t> shared_secret; // 32 B
};

// Encapsulate against pub_pem: returns {ct, ss}
EncapsResult mlkem_encaps(const std::string& pub_pem);

// Decapsulate ct using priv_pem: returns shared_secret (32 B).
// On decaps failure (implicit rejection per FIPS 203): returns pseudo-random ss,
// does NOT throw, does NOT leak error timing.
std::vector<uint8_t> mlkem_decaps(const std::string& priv_pem,
                                    const std::vector<uint8_t>& ciphertext);

// ─── PQ Certificate (JSON mini-project) ─────────────────────────────────────
// Cert structure: {subject, public_key (base64), issuer, signature (base64)}
// Signature covers: subject ‖ "|" ‖ public_key ‖ "|" ‖ issuer  (UTF-8 bytes)

struct PqCert {
    std::string subject;
    std::string public_key; // base64-encoded DER public key bytes
    std::string issuer;
    std::string signature;  // base64-encoded ML-DSA signature
};

PqCert cert_issue(const std::string& ca_priv_pem,
                   const std::string& subject,
                   const std::string& sub_pub_pem,
                   const std::string& issuer);

bool cert_verify(const std::string& ca_pub_pem, const PqCert& cert);

std::string cert_to_json(const PqCert& cert);
PqCert      cert_from_json(const std::string& json_str);

// ─── Encoding helpers ────────────────────────────────────────────────────────

std::string              to_base64(const std::vector<uint8_t>& data);
std::vector<uint8_t>    from_base64(const std::string& b64);

} // namespace pq
