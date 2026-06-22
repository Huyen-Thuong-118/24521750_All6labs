// ML-DSA-44 (FIPS 204) via OpenSSL 3.5+ native provider.
//
// OpenSSL 3.5+ uses the algorithm name "ML-DSA-44" (also "MLDSA44").
// Signing: pure ML-DSA (no external pre-hash) — pass NULL as MD to EVP_DigestSignInit.
// Signature: deterministic (no nonce vulnerability like ECDSA); no rejection-sampling
// loop that leaks timing in the signing step (hedged per FIPS 204 §5.2).

#include "pqtool.hpp"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace pq {
namespace {

static const char* ALGO = "ML-DSA-44";

static EVP_PKEY* load_priv(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    EVP_PKEY* p = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!p) throw std::runtime_error("Failed to load ML-DSA private key");
    return p;
}

static EVP_PKEY* load_pub(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    EVP_PKEY* p = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!p) throw std::runtime_error("Failed to load ML-DSA public key");
    return p;
}

} // namespace

std::pair<std::string, std::string> mldsa_keygen() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(nullptr, ALGO, nullptr);
    if (!ctx) throw std::runtime_error("EVP_PKEY_CTX_new_from_name(ML-DSA-44) failed");

    if (EVP_PKEY_keygen_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("ML-DSA keygen_init failed");
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) != 1 || !pkey) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("ML-DSA keygen failed");
    }
    EVP_PKEY_CTX_free(ctx);

    BIO* bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    BUF_MEM* m; BIO_get_mem_ptr(bio, &m);
    std::string priv(m->data, m->length);
    BIO_free(bio);

    bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, pkey);
    BIO_get_mem_ptr(bio, &m);
    std::string pub(m->data, m->length);
    BIO_free(bio);

    EVP_PKEY_free(pkey);
    return {priv, pub};
}

std::vector<uint8_t> mldsa_sign(const std::string& priv_pem,
                                  const std::vector<uint8_t>& msg)
{
    EVP_PKEY* pkey = load_priv(priv_pem);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();

    // NULL digest → pure ML-DSA (no external pre-hash, FIPS 204)
    if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, pkey) != 1) {
        EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("ML-DSA DigestSignInit failed");
    }

    size_t siglen = 0;
    if (EVP_DigestSign(ctx, nullptr, &siglen, msg.data(), msg.size()) != 1) {
        EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("ML-DSA DigestSign (size query) failed");
    }
    std::vector<uint8_t> sig(siglen);
    if (EVP_DigestSign(ctx, sig.data(), &siglen, msg.data(), msg.size()) != 1) {
        EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("ML-DSA DigestSign failed");
    }
    sig.resize(siglen);

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return sig;
}

bool mldsa_verify(const std::string& pub_pem,
                   const std::vector<uint8_t>& msg,
                   const std::vector<uint8_t>& sig)
{
    EVP_PKEY* pkey = nullptr;
    try { pkey = load_pub(pub_pem); } catch (...) { return false; }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool ok = false;

    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1) {
        ok = (EVP_DigestVerify(ctx,
                               sig.data(), sig.size(),
                               msg.data(), msg.size()) == 1);
    }

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
}

int mldsa_verify_batch(const std::string& pub_pem,
                        const std::vector<std::vector<uint8_t>>& msgs,
                        const std::vector<std::vector<uint8_t>>& sigs)
{
    int ok = 0;
    size_t n = std::min(msgs.size(), sigs.size());
    for (size_t i = 0; i < n; i++)
        if (mldsa_verify(pub_pem, msgs[i], sigs[i])) ok++;
    return ok;
}

} // namespace pq
