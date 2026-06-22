// rsa_oaep.cpp — RSA-OAEP SHA-256 with Crypto++
#include "rsa_oaep.hpp"

#include <cryptopp/rsa.h>
#include <cryptopp/sha.h>
#include <cryptopp/oaep.h>
#include <cryptopp/osrng.h>
#include <cryptopp/filters.h>
#include <cryptopp/base64.h>
#include <cryptopp/queue.h>

#include <sstream>
#include <stdexcept>

namespace rsa3 {

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::vector<uint8_t> bq_to_vec(CryptoPP::ByteQueue& bq) {
    std::vector<uint8_t> v(bq.CurrentSize());
    bq.Get(v.data(), v.size());
    return v;
}

static CryptoPP::RSA::PrivateKey load_priv(const std::vector<uint8_t>& der) {
    CryptoPP::ByteQueue bq;
    bq.Put(der.data(), der.size());
    CryptoPP::RSA::PrivateKey key;
    key.Load(bq);
    return key;
}

static CryptoPP::RSA::PublicKey load_pub(const std::vector<uint8_t>& der) {
    CryptoPP::ByteQueue bq;
    bq.Put(der.data(), der.size());
    CryptoPP::RSA::PublicKey key;
    key.Load(bq);
    return key;
}

// ── Key generation ────────────────────────────────────────────────────────────

RsaKeyPair keygen(int bits) {
    if (bits != 3072 && bits != 4096 && bits != 2048)
        throw std::runtime_error("keygen: unsupported bit size " + std::to_string(bits));

    CryptoPP::AutoSeededRandomPool rng;
    CryptoPP::RSA::PrivateKey priv;
    priv.GenerateRandomWithKeySize(rng, static_cast<unsigned int>(bits));

    CryptoPP::RSA::PublicKey pub(priv);

    CryptoPP::ByteQueue bq_priv, bq_pub;
    priv.Save(bq_priv);
    pub.Save(bq_pub);

    RsaKeyPair kp;
    kp.priv_der    = bq_to_vec(bq_priv);
    kp.pub_der     = bq_to_vec(bq_pub);
    kp.modulus_bits = bits;
    return kp;
}

// ── OAEP size limit ───────────────────────────────────────────────────────────

size_t oaep_max_len(int modulus_bits) {
    // OAEP-SHA256: max = k - 2*hLen - 2, where hLen=32 for SHA-256
    size_t k = static_cast<size_t>(modulus_bits) / 8;
    return (k > 66) ? (k - 66) : 0;
}

// ── RSA-OAEP SHA-256 encrypt ──────────────────────────────────────────────────

std::vector<uint8_t> oaep_encrypt(const std::vector<uint8_t>& pub_der,
                                   const std::vector<uint8_t>& plain) {
    auto pub = load_pub(pub_der);
    int bits = static_cast<int>(pub.GetModulus().BitCount());
    if (plain.size() > oaep_max_len(bits))
        throw std::runtime_error("oaep_encrypt: plaintext too large ("
            + std::to_string(plain.size()) + " > "
            + std::to_string(oaep_max_len(bits)) + ")");

    CryptoPP::AutoSeededRandomPool rng;
    typedef CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA256>>::Encryptor Enc;
    Enc enc(pub);

    std::string ct;
    CryptoPP::StringSource ss(plain.data(), plain.size(), true,
        new CryptoPP::PK_EncryptorFilter(rng, enc, new CryptoPP::StringSink(ct)));
    return {ct.begin(), ct.end()};
}

// ── RSA-OAEP SHA-256 decrypt ──────────────────────────────────────────────────

std::vector<uint8_t> oaep_decrypt(const std::vector<uint8_t>& priv_der,
                                   const std::vector<uint8_t>& ciphertext) {
    auto priv = load_priv(priv_der);

    CryptoPP::AutoSeededRandomPool rng;
    typedef CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA256>>::Decryptor Dec;
    Dec dec(priv);

    try {
        std::string pt;
        CryptoPP::StringSource ss(ciphertext.data(), ciphertext.size(), true,
            new CryptoPP::PK_DecryptorFilter(rng, dec, new CryptoPP::StringSink(pt)));
        return {pt.begin(), pt.end()};
    } catch (const CryptoPP::Exception& e) {
        throw std::runtime_error("RSA-OAEP decryption failed: " + std::string(e.what()));
    }
}

// ── PEM encode/decode ─────────────────────────────────────────────────────────

static std::string b64_wrap(const std::vector<uint8_t>& der) {
    std::string b64;
    CryptoPP::Base64Encoder enc(new CryptoPP::StringSink(b64), true, 64);
    enc.Put(der.data(), der.size());
    enc.MessageEnd();
    return b64;
}

std::string pem_encode_private(const std::vector<uint8_t>& der) {
    return "-----BEGIN RSA PRIVATE KEY-----\n"
         + b64_wrap(der)
         + "-----END RSA PRIVATE KEY-----\n";
}

std::string pem_encode_public(const std::vector<uint8_t>& der) {
    return "-----BEGIN RSA PUBLIC KEY-----\n"
         + b64_wrap(der)
         + "-----END RSA PUBLIC KEY-----\n";
}

std::vector<uint8_t> pem_decode(const std::string& pem) {
    // Strip header/footer, collect base64 body
    std::string b64;
    std::istringstream ss(pem);
    std::string line;
    bool in_body = false;
    while (std::getline(ss, line)) {
        // Trim trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() >= 5 && line.substr(0, 5) == "-----") {
            if (!in_body) { in_body = true; }
            else          { break; }
        } else if (in_body) {
            b64 += line;
        }
    }
    if (b64.empty()) throw std::runtime_error("pem_decode: no body found");

    std::string der_str;
    CryptoPP::Base64Decoder dec(new CryptoPP::StringSink(der_str));
    dec.Put(reinterpret_cast<const CryptoPP::byte*>(b64.data()), b64.size());
    dec.MessageEnd();
    return {der_str.begin(), der_str.end()};
}

// ── Modulus bit count ─────────────────────────────────────────────────────────

int get_modulus_bits(const std::vector<uint8_t>& pub_der) {
    auto pub = load_pub(pub_der);
    return static_cast<int>(pub.GetModulus().BitCount());
}

} // namespace rsa3
