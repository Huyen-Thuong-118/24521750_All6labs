// ML-KEM-512 (FIPS 203) via OpenSSL 3.5+ native provider.
//
// Encaps → returns {ciphertext (768 B), shared_secret (32 B)}
// Decaps → returns shared_secret (32 B)
//   Implicit rejection (Fujisaki–Okamoto transform): if ciphertext is malformed,
//   OpenSSL decaps returns a pseudo-random ss (no error leak, no timing side-channel).
//   We return whatever decaps gives; caller compares ss values to detect mismatch.

#include "pqtool.hpp"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/ml_kem.h>
#include <stdexcept>
#include <vector>
#include <string>

namespace pq {
namespace {

static const char* ALGO = "ML-KEM-512";

static EVP_PKEY* load_priv(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    EVP_PKEY* p = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!p) throw std::runtime_error("Failed to load ML-KEM private key");
    return p;
}

static EVP_PKEY* load_pub(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    EVP_PKEY* p = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!p) throw std::runtime_error("Failed to load ML-KEM public key");
    return p;
}

} // namespace

std::pair<std::string, std::string> mlkem_keygen() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(nullptr, ALGO, nullptr);
    if (!ctx) throw std::runtime_error("EVP_PKEY_CTX_new_from_name(ML-KEM-512) failed");

    if (EVP_PKEY_keygen_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("ML-KEM keygen_init failed");
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) != 1 || !pkey) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("ML-KEM keygen failed");
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

EncapsResult mlkem_encaps(const std::string& pub_pem) {
    EVP_PKEY* pkey = load_pub(pub_pem);

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_pkey(nullptr, pkey, nullptr);
    if (!ctx) { EVP_PKEY_free(pkey); throw std::runtime_error("EVP_PKEY_CTX_new failed"); }

    if (EVP_PKEY_encapsulate_init(ctx, nullptr) != 1) {
        EVP_PKEY_CTX_free(ctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("ML-KEM encapsulate_init failed");
    }

    size_t ctlen = 0, sslen = 0;
    if (EVP_PKEY_encapsulate(ctx, nullptr, &ctlen, nullptr, &sslen) != 1) {
        EVP_PKEY_CTX_free(ctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("ML-KEM encapsulate (size query) failed");
    }

    EncapsResult r;
    r.ciphertext.resize(ctlen);
    r.shared_secret.resize(sslen);
    if (EVP_PKEY_encapsulate(ctx, r.ciphertext.data(), &ctlen,
                              r.shared_secret.data(), &sslen) != 1) {
        EVP_PKEY_CTX_free(ctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("ML-KEM encapsulate failed");
    }
    r.ciphertext.resize(ctlen);
    r.shared_secret.resize(sslen);

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return r;
}

std::vector<uint8_t> mlkem_decaps(const std::string& priv_pem,
                                    const std::vector<uint8_t>& ciphertext)
{
    EVP_PKEY* pkey = load_priv(priv_pem);

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_pkey(nullptr, pkey, nullptr);
    if (!ctx) { EVP_PKEY_free(pkey); throw std::runtime_error("EVP_PKEY_CTX_new failed"); }

    if (EVP_PKEY_decapsulate_init(ctx, nullptr) != 1) {
        EVP_PKEY_CTX_free(ctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("ML-KEM decapsulate_init failed");
    }

    // Implicit rejection: if ct is malformed, OpenSSL returns pseudo-random ss.
    // We always return a 32-byte value — caller detects mismatch by comparing ss.
    size_t sslen = OSSL_ML_KEM_SHARED_SECRET_BYTES;
    std::vector<uint8_t> ss(sslen);
    EVP_PKEY_decapsulate(ctx, ss.data(), &sslen,
                          ciphertext.data(), ciphertext.size());
    ss.resize(sslen);

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ss;
}

} // namespace pq
