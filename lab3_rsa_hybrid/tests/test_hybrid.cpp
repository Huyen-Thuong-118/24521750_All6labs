#include <catch2/catch_test_macros.hpp>
#include "hybrid.hpp"
#include "rsa_oaep.hpp"
#include <common/codec.hpp>

TEST_CASE("Hybrid encrypt/decrypt roundtrip (small, direct RSA-OAEP)", "[hybrid]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // 5 bytes < 318

    auto env       = rsa3::encrypt(kp.pub_der, plain);
    auto recovered = rsa3::decrypt(kp.priv_der, env);

    REQUIRE(recovered == plain);
    REQUIRE(env.mode == "RSA-OAEP");
}

TEST_CASE("Hybrid encrypt/decrypt roundtrip (large, AES-GCM)", "[hybrid]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain(5000, 0xBB); // 5000 bytes > 318 → hybrid

    auto env       = rsa3::encrypt(kp.pub_der, plain);
    auto recovered = rsa3::decrypt(kp.priv_der, env);

    REQUIRE(recovered == plain);
    REQUIRE(env.mode == "RSA-OAEP-AES-GCM");
    REQUIRE(env.iv.size()  == 12);
    REQUIRE(env.tag.size() == 16);
}

TEST_CASE("Hybrid serialize/deserialize roundtrip (hybrid mode)", "[hybrid][serial]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain(1000, 0x42);

    auto env  = rsa3::encrypt(kp.pub_der, plain);
    auto buf  = rsa3::serialize(env);
    auto env2 = rsa3::deserialize(buf);

    REQUIRE(env2.mode        == env.mode);
    REQUIRE(env2.rsa_modulus == env.rsa_modulus);
    REQUIRE(env2.iv          == env.iv);
    REQUIRE(env2.tag         == env.tag);
    REQUIRE(env2.ciphertext  == env.ciphertext);
    REQUIRE(env2.wrapped_key == env.wrapped_key);

    auto recovered = rsa3::decrypt(kp.priv_der, env2);
    REQUIRE(recovered == plain);
}

TEST_CASE("Hybrid serialize/deserialize roundtrip (direct RSA-OAEP mode)", "[hybrid][serial]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain = {1, 2, 3};

    auto env  = rsa3::encrypt(kp.pub_der, plain);
    auto buf  = rsa3::serialize(env);
    auto env2 = rsa3::deserialize(buf);

    REQUIRE(env2.mode   == "RSA-OAEP");
    REQUIRE(env2.rsa_ct == env.rsa_ct);

    auto recovered = rsa3::decrypt(kp.priv_der, env2);
    REQUIRE(recovered == plain);
}

TEST_CASE("Label stored in envelope and verified on decrypt", "[hybrid][label]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain = {0xAA, 0xBB};

    auto env       = rsa3::encrypt(kp.pub_der, plain, "my-label");
    REQUIRE(env.label == "my-label");

    auto recovered = rsa3::decrypt(kp.priv_der, env, "my-label");
    REQUIRE(recovered == plain);
}

TEST_CASE("Wrong label on decrypt throws", "[hybrid][label][negative]") {
    auto kp = rsa3::keygen(3072);
    std::vector<uint8_t> plain = {0xAA};

    auto env = rsa3::encrypt(kp.pub_der, plain, "correct-label");
    REQUIRE_THROWS(rsa3::decrypt(kp.priv_der, env, "wrong-label"));
}

TEST_CASE("Hybrid wrong private key throws", "[hybrid][negative]") {
    auto kp1 = rsa3::keygen(3072);
    auto kp2 = rsa3::keygen(3072);
    std::vector<uint8_t> plain(1000, 0xCC);

    auto env = rsa3::encrypt(kp1.pub_der, plain);
    REQUIRE_THROWS(rsa3::decrypt(kp2.priv_der, env));
}
