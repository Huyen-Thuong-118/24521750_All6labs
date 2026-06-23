// RSA-PSS-3072 SHA-256, salt = hashLen = 32 bytes, e = 65537.
// Uses modern OpenSSL 3.x EVP API (non-deprecated).

#include "signer.hpp"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/sha.h>
#include <stdexcept>

namespace sig5 {
namespace {

static EVP_PKEY* load_priv(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    EVP_PKEY* p = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!p) throw std::runtime_error("Failed to load RSA private key");
    return p;
}

static EVP_PKEY* load_pub(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    EVP_PKEY* p = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!p) throw std::runtime_error("Failed to load RSA public key");
    return p;
}

// Configure EVP_PKEY_CTX for PSS: MGF1-SHA256, salt=32
static void set_pss_params(EVP_PKEY_CTX* ctx) {
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PSS_PADDING) != 1 ||
        EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256())           != 1 ||
        EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, 32)                 != 1)
        throw std::runtime_error("Failed to set PSS parameters");
}

} 

std::pair<std::string, std::string> keygen_rsa() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) throw std::runtime_error("EVP_PKEY_CTX_new_id failed");

    if (EVP_PKEY_keygen_init(ctx)                    != 1 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 3072) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("RSA keygen init failed");
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) != 1 || !pkey) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("RSA keygen failed");
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

std::vector<uint8_t> rsapss_sign(const std::string& priv_pem,
                                   const std::vector<uint8_t>& msg)
{
    EVP_PKEY* pkey = load_priv(priv_pem);

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX* pctx = nullptr;

    if (EVP_DigestSignInit(mdctx, &pctx, EVP_sha256(), nullptr, pkey) != 1) {
        EVP_MD_CTX_free(mdctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("DigestSignInit failed");
    }
    set_pss_params(pctx);

    if (EVP_DigestSignUpdate(mdctx, msg.data(), msg.size()) != 1) {
        EVP_MD_CTX_free(mdctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("DigestSignUpdate failed");
    }

    size_t siglen = 0;
    EVP_DigestSignFinal(mdctx, nullptr, &siglen);
    std::vector<uint8_t> sig(siglen);
    if (EVP_DigestSignFinal(mdctx, sig.data(), &siglen) != 1) {
        EVP_MD_CTX_free(mdctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("DigestSignFinal failed");
    }
    sig.resize(siglen);

    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    return sig;
}

bool rsapss_verify(const std::string& pub_pem,
                    const std::vector<uint8_t>& msg,
                    const std::vector<uint8_t>& sig)
{
    EVP_PKEY* pkey = nullptr;
    try { pkey = load_pub(pub_pem); } catch (...) { return false; }

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX* pctx = nullptr;
    bool ok = false;

    if (EVP_DigestVerifyInit(mdctx, &pctx, EVP_sha256(), nullptr, pkey) == 1) {
        try { set_pss_params(pctx); } catch (...) { goto cleanup; }
        if (EVP_DigestVerifyUpdate(mdctx, msg.data(), msg.size()) == 1)
            ok = (EVP_DigestVerifyFinal(mdctx, sig.data(),
                                         static_cast<size_t>(sig.size())) == 1);
    }
cleanup:
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    return ok;
}

} // namespace sig5
