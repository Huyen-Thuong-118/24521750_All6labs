// Negative and cross-algo tests
#include <catch2/catch_test_macros.hpp>
#include "signer.hpp"

static std::vector<uint8_t> bytes(const char* s) {
    return {reinterpret_cast<const uint8_t*>(s),
            reinterpret_cast<const uint8_t*>(s) + std::strlen(s)};
}

TEST_CASE("Cross-algo: ECDSA sig rejected by RSA verify", "[negative][cross]") {
    auto [ec_priv, ec_pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto [rsa_priv, rsa_pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto msg = bytes("cross algo test");
    auto ec_sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, ec_priv, msg);
    // RSA-PSS expects a 384-byte sig — ECDSA DER will be ~72 bytes → always fails
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, rsa_pub, msg, ec_sig));
}

TEST_CASE("Cross-algo: RSA sig rejected by ECDSA verify", "[negative][cross]") {
    auto [ec_priv, ec_pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto [rsa_priv, rsa_pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto msg = bytes("cross algo test 2");
    auto rsa_sig = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, rsa_priv, msg);
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::ECDSA_P256, ec_pub, msg, rsa_sig));
}

TEST_CASE("ECDSA verify: wrong algo key (RSA pub key for EC verify) returns false", "[negative]") {
    auto [rsa_priv, rsa_pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto [ec_priv,  ec_pub]  = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("algo mismatch");
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, ec_priv, msg);
    // Passing RSA public key to EC verifier should fail gracefully
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::ECDSA_P256, rsa_pub, msg, sig));
}

TEST_CASE("ECDSA verify: empty signature returns false", "[negative]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("empty sig");
    std::vector<uint8_t> empty;
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::ECDSA_P256, pub, msg, empty));
}

TEST_CASE("RSA-PSS verify: empty signature returns false", "[negative]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto msg = bytes("empty sig");
    std::vector<uint8_t> empty;
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub, msg, empty));
}

TEST_CASE("ECDSA verify: truncated DER returns false", "[negative]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("truncated sig");
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, msg);
    sig.resize(sig.size() / 2);
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::ECDSA_P256, pub, msg, sig));
}

TEST_CASE("verify_msg: garbage public key PEM returns false", "[negative]") {
    auto msg = bytes("bad key test");
    std::vector<uint8_t> some_sig(72, 0x42);
    std::string garbage_pem = "-----BEGIN PUBLIC KEY-----\naGVsbG8=\n-----END PUBLIC KEY-----\n";
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::ECDSA_P256, garbage_pem, msg, some_sig));
}

TEST_CASE("base64 decode: garbage string throws", "[negative][encoding]") {
    REQUIRE_THROWS(sig5::sig_from_base64("!!!not-base64!!!"));
}
