// RSA-PSS-3072 tests — keygen, sign/verify, parameter checks
#include <catch2/catch_test_macros.hpp>
#include "signer.hpp"
#include <vector>
#include <string>

static std::vector<uint8_t> bytes(const char* s) {
    return {reinterpret_cast<const uint8_t*>(s),
            reinterpret_cast<const uint8_t*>(s) + std::strlen(s)};
}

// ─── Key generation ──────────────────────────────────────────────────────────

TEST_CASE("RSA-PSS keygen produces non-empty PEM", "[rsa][keygen]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    REQUIRE(priv.find("PRIVATE KEY") != std::string::npos);
    REQUIRE(pub.find("PUBLIC KEY")   != std::string::npos);
}

TEST_CASE("RSA-PSS keygen: two calls produce different keys", "[rsa][keygen]") {
    auto [priv1, pub1] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto [priv2, pub2] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    REQUIRE(priv1 != priv2);
}

// ─── Sign / verify ───────────────────────────────────────────────────────────

TEST_CASE("RSA-PSS sign+verify roundtrip", "[rsa][sign]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto msg = bytes("hello, RSA-PSS-3072");
    auto sig = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv, msg);
    REQUIRE(!sig.empty());
    REQUIRE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub, msg, sig));
}

TEST_CASE("RSA-PSS sign+verify: empty message", "[rsa][sign]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    std::vector<uint8_t> msg;
    auto sig = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv, msg);
    REQUIRE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub, msg, sig));
}

TEST_CASE("RSA-PSS sign+verify: 64 KiB message", "[rsa][sign]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    std::vector<uint8_t> msg(65536, 0xCD);
    auto sig = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv, msg);
    REQUIRE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub, msg, sig));
}

// ─── Signature size ───────────────────────────────────────────────────────────

TEST_CASE("RSA-PSS: signature length is 3072/8 = 384 bytes", "[rsa][format]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto sig = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv, bytes("size test"));
    REQUIRE(sig.size() == 384);
}

// ─── Negative tests ───────────────────────────────────────────────────────────

TEST_CASE("RSA-PSS verify: tampered message fails", "[rsa][negative]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto msg = bytes("original rsa message");
    auto sig = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv, msg);
    msg[0] ^= 0xFF;
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub, msg, sig));
}

TEST_CASE("RSA-PSS verify: tampered signature fails", "[rsa][negative]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto msg = bytes("rsa tamper test");
    auto sig = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv, msg);
    sig[0] ^= 0xFF;
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub, msg, sig));
}

TEST_CASE("RSA-PSS verify: wrong key fails", "[rsa][negative]") {
    auto [priv1, pub1] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto [priv2, pub2] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto msg = bytes("cross key rsa");
    auto sig = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv1, msg);
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub2, msg, sig));
}

TEST_CASE("RSA-PSS verify: garbage sig returns false (no throw)", "[rsa][negative]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto msg = bytes("test");
    std::vector<uint8_t> garbage(384, 0xFF);
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub, msg, garbage));
}

// PSS is randomised — same msg+key gives different sigs each time
TEST_CASE("RSA-PSS: signatures are randomised (PSS salt)", "[rsa][format]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto msg = bytes("pss random test");
    auto sig1 = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv, msg);
    auto sig2 = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv, msg);
    // Both should verify
    REQUIRE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub, msg, sig1));
    REQUIRE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub, msg, sig2));
    // And differ (with overwhelming probability due to random salt)
    REQUIRE(sig1 != sig2);
}

// ─── Base64 encoding ─────────────────────────────────────────────────────────

TEST_CASE("RSA-PSS: base64 roundtrip", "[rsa][encoding]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    auto msg = bytes("rsa base64 test");
    auto sig = sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv, msg);
    auto b64 = sig5::sig_to_base64(sig);
    auto decoded = sig5::sig_from_base64(b64);
    REQUIRE(decoded == sig);
    REQUIRE(sig5::verify_msg(sig5::Algo::RSA_PSS_3072, pub, msg, decoded));
}
