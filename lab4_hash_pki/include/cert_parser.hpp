#pragma once
#include <string>
#include <vector>

namespace pki {

struct CertInfo {
    std::string subject;
    std::string issuer;
    std::string not_before;
    std::string not_after;
    std::string sig_algorithm;       // e.g. "sha256WithRSAEncryption"
    std::string spki_algorithm;      // e.g. "rsaEncryption", "id-ecPublicKey"
    int         spki_bits   = 0;     // key size in bits
    std::vector<std::string> sans;   // Subject Alternative Names
    std::vector<std::string> key_usages;
    bool        is_ca       = false;
    long        serial      = 0;
};

// Parse a PEM or DER certificate file
CertInfo parse_cert(const std::string& path);

// Parse from PEM string directly
CertInfo parse_cert_pem(const std::string& pem);

// Verify signature of cert using issuer's public key (also PEM path)
// Returns true if signature is valid
bool verify_cert_signature(const std::string& cert_path,
                            const std::string& issuer_cert_path);

// Dump CertInfo as human-readable string
std::string to_string(const CertInfo& info);

} // namespace pki
