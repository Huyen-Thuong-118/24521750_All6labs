// PQ Certificate — JSON-based mini-project.
//
// Cert = {subject, public_key (base64), issuer, signature (base64)}
// Signed data = UTF-8 bytes of: subject + "|" + public_key + "|" + issuer
// CA signs subject's public key with ML-DSA-44.

#include "pqtool.hpp"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace pq {
namespace {

// Canonical bytes for signing = subject + "|" + public_key + "|" + issuer
static std::vector<uint8_t> canonical_bytes(const PqCert& cert) {
    std::string s = cert.subject + "|" + cert.public_key + "|" + cert.issuer;
    return {s.begin(), s.end()};
}

// Convert a public key PEM to base64-encoded DER bytes
static std::string pub_pem_to_b64(const std::string& pub_pem) {
    BIO* bio = BIO_new_mem_buf(pub_pem.data(), static_cast<int>(pub_pem.size()));
    EVP_PKEY* p = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!p) throw std::runtime_error("Failed to load public key for cert");

    // DER encode
    int derlen = i2d_PUBKEY(p, nullptr);
    std::vector<uint8_t> der(derlen);
    uint8_t* ptr = der.data();
    i2d_PUBKEY(p, &ptr);
    EVP_PKEY_free(p);

    return to_base64(der);
}

}

PqCert cert_issue(const std::string& ca_priv_pem,
                   const std::string& subject,
                   const std::string& sub_pub_pem,
                   const std::string& issuer)
{
    PqCert cert;
    cert.subject    = subject;
    cert.public_key = pub_pem_to_b64(sub_pub_pem);
    cert.issuer     = issuer;
    auto data = canonical_bytes(cert);
    auto sig  = mldsa_sign(ca_priv_pem, data);
    cert.signature = to_base64(sig);

    return cert;
}

bool cert_verify(const std::string& ca_pub_pem, const PqCert& cert) {
    auto data = canonical_bytes(cert);
    std::vector<uint8_t> sig;
    try { sig = from_base64(cert.signature); }
    catch (...) { return false; } 
    return mldsa_verify(ca_pub_pem, data, sig);
}

std::string cert_to_json(const PqCert& cert) {
    nlohmann::json j;
    j["subject"]    = cert.subject;
    j["public_key"] = cert.public_key;
    j["issuer"]     = cert.issuer;
    j["signature"]  = cert.signature;
    return j.dump(2); // pretty-print, 2-space indent
}

PqCert cert_from_json(const std::string& json_str) {
    auto j = nlohmann::json::parse(json_str);
    PqCert cert;
    cert.subject    = j["subject"].get<std::string>();
    cert.public_key = j["public_key"].get<std::string>();
    cert.issuer     = j["issuer"].get<std::string>();
    cert.signature  = j["signature"].get<std::string>();
    return cert;
}

} // namespace pq
