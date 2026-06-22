// ECDSA-P256 with RFC 6979 deterministic nonce (FIPS 186-4, RFC 6979).
// Uses OpenSSL low-level EC/ECDSA/BN API (deprecated in 3.x but functional).
//
// RFC 6979 nonce derivation:
//   K₀=0×00…, V₀=0×01…
//   K₁ = HMAC_K₀(V₀ ‖ 0x00 ‖ int2octets(x) ‖ bits2octets(h₁))
//   V₁ = HMAC_K₁(V₀)
//   K₂ = HMAC_K₁(V₁ ‖ 0x01 ‖ int2octets(x) ‖ bits2octets(h₁))
//   V₂ = HMAC_K₂(V₁)
//   k  = bits2int(HMAC_K₂(V₂));  retry if k ∉ [1, q-1]

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include "signer.hpp"
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/hmac.h>
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/obj_mac.h>
#include <memory>
#include <stdexcept>
#include <cstring>

namespace sig5 {
namespace {

// ─── HMAC-SHA256 one-shot over concatenated parts ────────────────────────────
static void hmac_sha256(const uint8_t* K, size_t Klen,
                         std::initializer_list<std::pair<const uint8_t*, size_t>> parts,
                         uint8_t out[32])
{
    unsigned char buf[4096]; size_t total = 0;
    for (auto& [d, n] : parts) { memcpy(buf + total, d, n); total += n; }
    unsigned int outlen = 32;
    HMAC(EVP_sha256(), K, static_cast<int>(Klen), buf, total, out, &outlen);
}

// ─── RFC 6979 k-generation for P-256 + SHA-256 ──────────────────────────────
static BIGNUM* rfc6979_k(const BIGNUM* priv, const uint8_t h[32], const BIGNUM* q)
{
    // int2octets(x)
    uint8_t bx[32] = {};
    BN_bn2binpad(priv, bx, 32);

    // bits2octets(h1): h1 mod q, then pad to 32 bytes
    BIGNUM* h1 = BN_bin2bn(h, 32, nullptr);
    if (BN_cmp(h1, q) >= 0) BN_sub(h1, h1, q);
    uint8_t bh[32] = {};
    BN_bn2binpad(h1, bh, 32);
    BN_free(h1);

    uint8_t V[32], K[32];
    memset(V, 0x01, 32);
    memset(K, 0x00, 32);

    static const uint8_t b00 = 0x00, b01 = 0x01;

    hmac_sha256(K, 32, {{V,32},{&b00,1},{bx,32},{bh,32}}, K);
    hmac_sha256(K, 32, {{V,32}}, V);
    hmac_sha256(K, 32, {{V,32},{&b01,1},{bx,32},{bh,32}}, K);
    hmac_sha256(K, 32, {{V,32}}, V);

    for (;;) {
        hmac_sha256(K, 32, {{V,32}}, V);
        BIGNUM* k = BN_bin2bn(V, 32, nullptr);
        if (!BN_is_zero(k) && BN_cmp(k, q) < 0)
            return k;
        BN_free(k);
        hmac_sha256(K, 32, {{V,32},{&b00,1}}, K);
        hmac_sha256(K, 32, {{V,32}}, V);
    }
}

// ─── ECDSA sign with custom k ────────────────────────────────────────────────
static std::vector<uint8_t> do_sign(EC_KEY* key, const uint8_t hash[32])
{
    const EC_GROUP* grp   = EC_KEY_get0_group(key);
    const BIGNUM*   order = EC_GROUP_get0_order(grp);
    const BIGNUM*   x     = EC_KEY_get0_private_key(key);

    BIGNUM* k = rfc6979_k(x, hash, order);

    auto ctx = std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)>(BN_CTX_new(), BN_CTX_free);

    // R = k·G
    EC_POINT* R = EC_POINT_new(grp);
    EC_POINT_mul(grp, R, k, nullptr, nullptr, ctx.get());

    // r = R.x mod q
    BIGNUM* r = BN_new();
    EC_POINT_get_affine_coordinates(grp, R, r, nullptr, ctx.get());
    BN_mod(r, r, order, ctx.get());
    EC_POINT_free(R);

    if (BN_is_zero(r)) {
        BN_free(r); BN_free(k);
        throw std::runtime_error("ECDSA sign: degenerate r");
    }

    // s = k⁻¹·(h + r·x) mod q
    BIGNUM* h_bn = BN_bin2bn(hash, 32, nullptr);
    BIGNUM* kinv = BN_mod_inverse(nullptr, k, order, ctx.get());
    BIGNUM* s    = BN_new();
    BN_mod_mul(s, r, x, order, ctx.get());
    BN_mod_add(s, s, h_bn, order, ctx.get());
    BN_mod_mul(s, s, kinv, order, ctx.get());
    BN_free(h_bn); BN_free(kinv); BN_free(k);

    if (BN_is_zero(s)) {
        BN_free(r); BN_free(s);
        throw std::runtime_error("ECDSA sign: degenerate s");
    }

    // Encode as DER
    ECDSA_SIG* sig = ECDSA_SIG_new();
    ECDSA_SIG_set0(sig, r, s); // takes ownership

    int der_len = i2d_ECDSA_SIG(sig, nullptr);
    std::vector<uint8_t> der(der_len);
    uint8_t* p = der.data();
    i2d_ECDSA_SIG(sig, &p);
    ECDSA_SIG_free(sig);
    return der;
}

// ─── Load EC key from PEM (private or public) ────────────────────────────────
static EC_KEY* load_ec_priv(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    EC_KEY* k = PEM_read_bio_ECPrivateKey(bio, nullptr, nullptr, nullptr);
    if (!k) {
        BIO_reset(bio);
        EVP_PKEY* p = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        if (p) { k = EVP_PKEY_get1_EC_KEY(p); EVP_PKEY_free(p); }
    }
    BIO_free(bio);
    if (!k) throw std::runtime_error("Failed to load EC private key");
    return k;
}

static EC_KEY* load_ec_pub(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    EVP_PKEY* p = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!p) throw std::runtime_error("Failed to load EC public key");
    EC_KEY* k = EVP_PKEY_get1_EC_KEY(p);
    EVP_PKEY_free(p);
    if (!k) throw std::runtime_error("Key is not EC");
    return k;
}

} // anonymous namespace

// ─── Public API ──────────────────────────────────────────────────────────────

std::pair<std::string, std::string> keygen_ecdsa() {
    EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!key || EC_KEY_generate_key(key) != 1)
        throw std::runtime_error("ECDSA keygen failed");

    BIO* bio = BIO_new(BIO_s_mem());
    PEM_write_bio_ECPrivateKey(bio, key, nullptr, nullptr, 0, nullptr, nullptr);
    BUF_MEM* m; BIO_get_mem_ptr(bio, &m);
    std::string priv(m->data, m->length);
    BIO_free(bio);

    bio = BIO_new(BIO_s_mem());
    PEM_write_bio_EC_PUBKEY(bio, key);
    BIO_get_mem_ptr(bio, &m);
    std::string pub(m->data, m->length);
    BIO_free(bio);

    EC_KEY_free(key);
    return {priv, pub};
}

std::vector<uint8_t> ecdsa_sign(const std::string& priv_pem,
                                  const std::vector<uint8_t>& msg)
{
    EC_KEY* key = load_ec_priv(priv_pem);

    uint8_t hash[32];
    SHA256(msg.data(), msg.size(), hash);

    auto sig = do_sign(key, hash);
    EC_KEY_free(key);
    return sig;
}

bool ecdsa_verify(const std::string& pub_pem,
                   const std::vector<uint8_t>& msg,
                   const std::vector<uint8_t>& sig_der)
{
    EC_KEY* key = nullptr;
    try { key = load_ec_pub(pub_pem); } catch (...) { return false; }

    uint8_t hash[32];
    SHA256(msg.data(), msg.size(), hash);

    int r = ECDSA_verify(0, hash, 32,
                          sig_der.data(), static_cast<int>(sig_der.size()),
                          key);
    EC_KEY_free(key);
    return r == 1;
}

} // namespace sig5
