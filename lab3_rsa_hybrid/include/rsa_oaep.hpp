#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rsa3 {

struct RsaKeyPair {
    std::vector<uint8_t> priv_der;   // PKCS#1 RSAPrivateKey DER
    std::vector<uint8_t> pub_der;    // PKCS#1 RSAPublicKey DER
    int modulus_bits;
};

// Generate RSA key pair (bits = 3072 or 4096)
RsaKeyPair keygen(int bits = 3072);

// Maximum plaintext bytes for RSA-OAEP-SHA256: modulus_bytes - 2*32 - 2
size_t oaep_max_len(int modulus_bits);

// RSA-OAEP SHA-256 encrypt/decrypt
// label is stored in envelope but not passed into OAEP hash (standard empty-label OAEP)
std::vector<uint8_t> oaep_encrypt(const std::vector<uint8_t>& pub_der,
                                   const std::vector<uint8_t>& plain);

std::vector<uint8_t> oaep_decrypt(const std::vector<uint8_t>& priv_der,
                                   const std::vector<uint8_t>& ciphertext);

// PEM encode/decode
std::string          pem_encode_private(const std::vector<uint8_t>& der);
std::string          pem_encode_public (const std::vector<uint8_t>& der);
std::vector<uint8_t> pem_decode        (const std::string& pem);

// Get modulus bit count from public key DER
int get_modulus_bits(const std::vector<uint8_t>& pub_der);

} // namespace rsa3
