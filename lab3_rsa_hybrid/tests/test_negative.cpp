#include <catch2/catch_test_macros.hpp>
#include "hybrid.hpp"
#include "rsa_oaep.hpp"

// Negative / security tests

TEST_CASE("Altered RSA ciphertext (direct mode) throws on decrypt", "[negative]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain = {0x41, 0x42, 0x43};

    auto env = rsa3::encrypt(kp.pub_der, plain);
    REQUIRE(env.mode == "RSA-OAEP");

    // Flip bits in the RSA ciphertext
    auto tampered = env;
    for (size_t i = 0; i < tampered.rsa_ct.size(); i += 50)
        tampered.rsa_ct[i] ^= 0xFF;

    REQUIRE_THROWS(rsa3::decrypt(kp.priv_der, tampered));
}

TEST_CASE("Altered AES-GCM ciphertext throws (tag check)", "[negative]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain(1000, 0xDD);

    auto env = rsa3::encrypt(kp.pub_der, plain);
    REQUIRE(env.mode == "RSA-OAEP-AES-GCM");

    auto tampered = env;
    if (!tampered.ciphertext.empty())
        tampered.ciphertext[0] ^= 0x01;

    REQUIRE_THROWS(rsa3::decrypt(kp.priv_der, tampered));
}

TEST_CASE("Altered GCM tag throws", "[negative]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain(500, 0xEE);

    auto env     = rsa3::encrypt(kp.pub_der, plain);
    auto tampered = env;
    tampered.tag[0] ^= 0xFF; // corrupt first byte of tag

    REQUIRE_THROWS(rsa3::decrypt(kp.priv_der, tampered));
}

TEST_CASE("Altered GCM IV causes tag failure", "[negative]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain(500, 0xFF);

    auto env     = rsa3::encrypt(kp.pub_der, plain);
    auto tampered = env;
    tampered.iv[0] ^= 0xFF; // wrong IV → wrong keystream → wrong tag

    REQUIRE_THROWS(rsa3::decrypt(kp.priv_der, tampered));
}

TEST_CASE("Wrong private key fails on hybrid decrypt", "[negative]") {
    auto kp1 = rsa3::keygen(3072);
    auto kp2 = rsa3::keygen(3072);
    std::vector<uint8_t> plain(500, 0x11);

    auto env = rsa3::encrypt(kp1.pub_der, plain);
    REQUIRE_THROWS(rsa3::decrypt(kp2.priv_der, env));
}

TEST_CASE("Wrong OAEP label (stored in envelope) throws", "[negative]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain = {0x01};

    auto env = rsa3::encrypt(kp.pub_der, plain, "secret-label");
    REQUIRE_THROWS(rsa3::decrypt(kp.priv_der, env, "wrong-label"));
}

TEST_CASE("Tampered envelope JSON throws on deserialize", "[negative]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain(100, 0x22);

    auto env = rsa3::encrypt(kp.pub_der, plain);
    auto buf = rsa3::serialize(env);

    // Corrupt the JSON header (first 20 bytes)
    for (size_t i = 8; i < 20 && i < buf.size(); ++i)
        buf[i] ^= 0xFF;

    REQUIRE_THROWS(rsa3::deserialize(buf));
}

TEST_CASE("Missing separator throws on deserialize", "[negative]") {
    std::vector<uint8_t> garbage = {'n', 'o', 't', '-', 'v', 'a', 'l', 'i', 'd'};
    REQUIRE_THROWS(rsa3::deserialize(garbage));
}

TEST_CASE("Corrupt PEM key file throws on load", "[negative]") {
    std::string bad_pem =
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "dGhpcyBpcyBub3QgYSByZWFsIGtleQ==\n"
        "-----END RSA PRIVATE KEY-----\n";
    auto der = rsa3::pem_decode(bad_pem);
    // DER bytes are garbage — oaep_decrypt should throw
    std::vector<uint8_t> ct(384, 0x00);
    REQUIRE_THROWS(rsa3::oaep_decrypt(der, ct));
}
