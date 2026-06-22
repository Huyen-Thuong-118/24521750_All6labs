#include <catch2/catch_test_macros.hpp>
#include "rsa_oaep.hpp"
#include <common/codec.hpp>

// RSA-3072 keygen is slow (~200-500ms each), so generate once per section

TEST_CASE("RSA keygen produces valid pair", "[rsa][keygen]") {
    auto kp = rsa3::keygen(3072);
    REQUIRE(kp.modulus_bits == 3072);
    REQUIRE_FALSE(kp.priv_der.empty());
    REQUIRE_FALSE(kp.pub_der.empty());
    REQUIRE(rsa3::get_modulus_bits(kp.pub_der) == 3072);
}

TEST_CASE("RSA-3072 OAEP roundtrip", "[rsa][oaep]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> msg = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // "Hello"

    auto ct        = rsa3::oaep_encrypt(kp.pub_der, msg);
    auto recovered = rsa3::oaep_decrypt(kp.priv_der, ct);

    REQUIRE(recovered == msg);
    REQUIRE(ct != msg); // ciphertext != plaintext
}

TEST_CASE("RSA-4096 OAEP roundtrip", "[rsa][oaep]") {
    auto kp = rsa3::keygen(4096);
    std::vector<uint8_t> msg(100, 0xAA);

    auto ct        = rsa3::oaep_encrypt(kp.pub_der, msg);
    auto recovered = rsa3::oaep_decrypt(kp.priv_der, ct);

    REQUIRE(recovered == msg);
}

TEST_CASE("OAEP ciphertext is probabilistic (non-deterministic)", "[rsa][oaep]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> msg(16, 0xCC);

    auto ct1 = rsa3::oaep_encrypt(kp.pub_der, msg);
    auto ct2 = rsa3::oaep_encrypt(kp.pub_der, msg);

    REQUIRE(ct1 != ct2); // random OAEP seed each time
}

TEST_CASE("oaep_max_len is correct for SHA-256", "[rsa][oaep]") {
    // SHA-256 hLen = 32; max = k - 66
    REQUIRE(rsa3::oaep_max_len(3072) == 3072 / 8 - 66); // 384-66=318
    REQUIRE(rsa3::oaep_max_len(4096) == 4096 / 8 - 66); // 512-66=446
}

TEST_CASE("OAEP plaintext too large throws", "[rsa][oaep][negative]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> big(rsa3::oaep_max_len(3072) + 1, 0xFF);
    REQUIRE_THROWS(rsa3::oaep_encrypt(kp.pub_der, big));
}

TEST_CASE("OAEP wrong private key throws", "[rsa][oaep][negative]") {
    auto kp1 = rsa3::keygen(3072);
    auto kp2 = rsa3::keygen(3072);
    std::vector<uint8_t> msg = {1, 2, 3, 4};

    auto ct = rsa3::oaep_encrypt(kp1.pub_der, msg);
    REQUIRE_THROWS(rsa3::oaep_decrypt(kp2.priv_der, ct));
}

TEST_CASE("OAEP tampered ciphertext throws", "[rsa][oaep][negative]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> msg = {0xDE, 0xAD, 0xBE, 0xEF};

    auto ct = rsa3::oaep_encrypt(kp.pub_der, msg);
    ct[ct.size() / 2] ^= 0x55; // flip bits in middle of ciphertext
    REQUIRE_THROWS(rsa3::oaep_decrypt(kp.priv_der, ct));
}

TEST_CASE("PEM encode/decode roundtrip (private)", "[rsa][pem]") {
    auto kp  = rsa3::keygen(3072);
    auto pem = rsa3::pem_encode_private(kp.priv_der);
    REQUIRE(pem.find("-----BEGIN RSA PRIVATE KEY-----") != std::string::npos);
    auto der2 = rsa3::pem_decode(pem);
    REQUIRE(der2 == kp.priv_der);
}

TEST_CASE("PEM encode/decode roundtrip (public)", "[rsa][pem]") {
    auto kp  = rsa3::keygen(3072);
    auto pem = rsa3::pem_encode_public(kp.pub_der);
    REQUIRE(pem.find("-----BEGIN RSA PUBLIC KEY-----") != std::string::npos);
    auto der2 = rsa3::pem_decode(pem);
    REQUIRE(der2 == kp.pub_der);
}

TEST_CASE("Corrupt PEM throws on decode", "[rsa][pem][negative]") {
    REQUIRE_THROWS(rsa3::pem_decode("this is not PEM"));
}
