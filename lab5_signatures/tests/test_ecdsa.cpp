// ECDSA-P256 tests -keygen, sign/verify, RFC 6979 determinism, KAT
#include <catch2/catch_test_macros.hpp>
#include "signer.hpp"
#include <vector>
#include <string>

// Helper: ASCII message as bytes
static std::vector<uint8_t> bytes(const char* s) {
    return {reinterpret_cast<const uint8_t*>(s),
            reinterpret_cast<const uint8_t*>(s) + std::strlen(s)};
}

// ─── Key generation ──────────────────────────────────────────────────────────

TEST_CASE("ECDSA keygen produces non-empty PEM", "[ecdsa][keygen]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    REQUIRE(priv.find("EC PRIVATE KEY") != std::string::npos);
    REQUIRE(pub.find("PUBLIC KEY")      != std::string::npos);
}

TEST_CASE("ECDSA keygen: two calls produce different keys", "[ecdsa][keygen]") {
    auto [priv1, pub1] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto [priv2, pub2] = sig5::keygen(sig5::Algo::ECDSA_P256);
    REQUIRE(priv1 != priv2);
}

// ─── Sign / verify ───────────────────────────────────────────────────────────

TEST_CASE("ECDSA sign+verify roundtrip", "[ecdsa][sign]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("hello, ECDSA-P256");
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, msg);
    REQUIRE(!sig.empty());
    REQUIRE(sig5::verify_msg(sig5::Algo::ECDSA_P256, pub, msg, sig));
}

TEST_CASE("ECDSA sign+verify: empty message", "[ecdsa][sign]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    std::vector<uint8_t> msg;
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, msg);
    REQUIRE(sig5::verify_msg(sig5::Algo::ECDSA_P256, pub, msg, sig));
}

TEST_CASE("ECDSA sign+verify: 64 KiB message", "[ecdsa][sign]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    std::vector<uint8_t> msg(65536, 0xAB);
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, msg);
    REQUIRE(sig5::verify_msg(sig5::Algo::ECDSA_P256, pub, msg, sig));
}

// ─── Negative tests ───────────────────────────────────────────────────────────

TEST_CASE("ECDSA verify: tampered message fails", "[ecdsa][negative]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("original message");
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, msg);
    msg[0] ^= 0xFF;  // flip a byte
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::ECDSA_P256, pub, msg, sig));
}

TEST_CASE("ECDSA verify: tampered signature fails", "[ecdsa][negative]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("original message");
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, msg);
    if (!sig.empty()) sig[0] ^= 0xFF;
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::ECDSA_P256, pub, msg, sig));
}

TEST_CASE("ECDSA verify: wrong key fails", "[ecdsa][negative]") {
    auto [priv1, pub1] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto [priv2, pub2] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("cross-key test");
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv1, msg);
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::ECDSA_P256, pub2, msg, sig));
}

TEST_CASE("ECDSA verify: garbage signature returns false (no throw)", "[ecdsa][negative]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("test");
    std::vector<uint8_t> garbage = {0x30, 0x06, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00};
    REQUIRE_FALSE(sig5::verify_msg(sig5::Algo::ECDSA_P256, pub, msg, garbage));
}

// ─── RFC 6979 determinism ────────────────────────────────────────────────────

TEST_CASE("ECDSA: RFC 6979 -same key+msg produces identical signatures", "[ecdsa][rfc6979]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("deterministic ECDSA test");
    auto sig1 = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, msg);
    auto sig2 = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, msg);
    REQUIRE(sig1 == sig2);  // byte-for-byte identical: RFC 6979
}

TEST_CASE("ECDSA: RFC 6979 -different messages produce different sigs", "[ecdsa][rfc6979]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto sig1 = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, bytes("msg A"));
    auto sig2 = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, bytes("msg B"));
    REQUIRE(sig1 != sig2);
}

TEST_CASE("ECDSA: RFC 6979 -different keys produce different sigs", "[ecdsa][rfc6979]") {
    auto [priv1, pub1] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto [priv2, pub2] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("same message");
    auto sig1 = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv1, msg);
    auto sig2 = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv2, msg);
    REQUIRE(sig1 != sig2);
}

// ─── DER format ──────────────────────────────────────────────────────────────

TEST_CASE("ECDSA: signature is DER-encoded (starts with 0x30)", "[ecdsa][format]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, bytes("der test"));
    REQUIRE(sig.size() >= 2);
    REQUIRE(sig[0] == 0x30);  // DER SEQUENCE tag
}

// ─── Base64 encoding ─────────────────────────────────────────────────────────

TEST_CASE("ECDSA: base64 roundtrip", "[ecdsa][encoding]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    auto msg = bytes("base64 test");
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, msg);
    auto b64 = sig5::sig_to_base64(sig);
    REQUIRE(!b64.empty());
    auto decoded = sig5::sig_from_base64(b64);
    REQUIRE(decoded == sig);
    REQUIRE(sig5::verify_msg(sig5::Algo::ECDSA_P256, pub, msg, decoded));
}
