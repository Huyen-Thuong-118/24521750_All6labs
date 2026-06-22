// ML-DSA-44 tests — keygen, sign/verify, determinism, size checks
#include <catch2/catch_test_macros.hpp>
#include "pqtool.hpp"
#include <string>
#include <vector>

static std::vector<uint8_t> bytes(const char* s) {
    return {reinterpret_cast<const uint8_t*>(s),
            reinterpret_cast<const uint8_t*>(s) + std::strlen(s)};
}

TEST_CASE("ML-DSA-44 keygen: PEM non-empty", "[mldsa][keygen]") {
    auto [priv, pub] = pq::mldsa_keygen();
    REQUIRE(priv.find("PRIVATE KEY") != std::string::npos);
    REQUIRE(pub.find("PUBLIC KEY")   != std::string::npos);
}

TEST_CASE("ML-DSA-44 keygen: two calls produce different keys", "[mldsa][keygen]") {
    auto [p1, u1] = pq::mldsa_keygen();
    auto [p2, u2] = pq::mldsa_keygen();
    REQUIRE(p1 != p2);
}

TEST_CASE("ML-DSA-44 sign+verify: short message", "[mldsa][sign]") {
    auto [priv, pub] = pq::mldsa_keygen();
    auto msg = bytes("hello ML-DSA-44");
    auto sig = pq::mldsa_sign(priv, msg);
    REQUIRE(!sig.empty());
    REQUIRE(pq::mldsa_verify(pub, msg, sig));
}

TEST_CASE("ML-DSA-44 sign+verify: empty message", "[mldsa][sign]") {
    auto [priv, pub] = pq::mldsa_keygen();
    std::vector<uint8_t> msg;
    auto sig = pq::mldsa_sign(priv, msg);
    REQUIRE(pq::mldsa_verify(pub, msg, sig));
}

TEST_CASE("ML-DSA-44 sign+verify: 64 KiB message", "[mldsa][sign]") {
    auto [priv, pub] = pq::mldsa_keygen();
    std::vector<uint8_t> msg(65536, 0xAB);
    auto sig = pq::mldsa_sign(priv, msg);
    REQUIRE(pq::mldsa_verify(pub, msg, sig));
}

TEST_CASE("ML-DSA-44: signature size = 2420 bytes", "[mldsa][format]") {
    auto [priv, pub] = pq::mldsa_keygen();
    auto sig = pq::mldsa_sign(priv, bytes("size test"));
    REQUIRE(sig.size() == 2420);
}

TEST_CASE("ML-DSA-44: verify tampered message fails", "[mldsa][negative]") {
    auto [priv, pub] = pq::mldsa_keygen();
    auto msg = bytes("original");
    auto sig = pq::mldsa_sign(priv, msg);
    msg[0] ^= 0xFF;
    REQUIRE_FALSE(pq::mldsa_verify(pub, msg, sig));
}

TEST_CASE("ML-DSA-44: verify tampered signature fails", "[mldsa][negative]") {
    auto [priv, pub] = pq::mldsa_keygen();
    auto msg = bytes("original");
    auto sig = pq::mldsa_sign(priv, msg);
    sig[0] ^= 0xFF;
    REQUIRE_FALSE(pq::mldsa_verify(pub, msg, sig));
}

TEST_CASE("ML-DSA-44: verify wrong key fails", "[mldsa][negative]") {
    auto [priv1, pub1] = pq::mldsa_keygen();
    auto [priv2, pub2] = pq::mldsa_keygen();
    auto msg = bytes("cross key");
    auto sig = pq::mldsa_sign(priv1, msg);
    REQUIRE_FALSE(pq::mldsa_verify(pub2, msg, sig));
}

TEST_CASE("ML-DSA-44: verify garbage sig returns false (no throw)", "[mldsa][negative]") {
    auto [priv, pub] = pq::mldsa_keygen();
    auto msg = bytes("test");
    std::vector<uint8_t> garbage(2420, 0x42);
    REQUIRE_FALSE(pq::mldsa_verify(pub, msg, garbage));
}

TEST_CASE("ML-DSA-44: verify garbage pub PEM returns false", "[mldsa][negative]") {
    auto [priv, pub] = pq::mldsa_keygen();
    auto msg = bytes("test");
    auto sig = pq::mldsa_sign(priv, msg);
    std::string bad = "-----BEGIN PUBLIC KEY-----\naGVsbG8=\n-----END PUBLIC KEY-----\n";
    REQUIRE_FALSE(pq::mldsa_verify(bad, msg, sig));
}

TEST_CASE("ML-DSA-44: deterministic hedged signing (different sign calls may differ)", "[mldsa][det]") {
    // FIPS 204 uses hedged variant (with fresh randomness per sign call by default).
    // Both sigs must verify correctly.
    auto [priv, pub] = pq::mldsa_keygen();
    auto msg  = bytes("hedged test");
    auto sig1 = pq::mldsa_sign(priv, msg);
    auto sig2 = pq::mldsa_sign(priv, msg);
    // Both must be valid regardless of determinism
    REQUIRE(pq::mldsa_verify(pub, msg, sig1));
    REQUIRE(pq::mldsa_verify(pub, msg, sig2));
}

TEST_CASE("ML-DSA-44: batch verify 50 messages all pass", "[mldsa][batch]") {
    auto [priv, pub] = pq::mldsa_keygen();
    std::vector<std::vector<uint8_t>> msgs, sigs;
    for (int i = 0; i < 50; i++) {
        std::string s = "batch-mldsa-" + std::to_string(i);
        std::vector<uint8_t> m(s.begin(), s.end());
        msgs.push_back(m);
        sigs.push_back(pq::mldsa_sign(priv, m));
    }
    REQUIRE(pq::mldsa_verify_batch(pub, msgs, sigs) == 50);
}

TEST_CASE("ML-DSA-44: batch verify with 1 tampered sig", "[mldsa][batch]") {
    auto [priv, pub] = pq::mldsa_keygen();
    std::vector<std::vector<uint8_t>> msgs, sigs;
    for (int i = 0; i < 10; i++) {
        std::string s = "batch-" + std::to_string(i);
        std::vector<uint8_t> m(s.begin(), s.end());
        msgs.push_back(m);
        sigs.push_back(pq::mldsa_sign(priv, m));
    }
    sigs[4][0] ^= 0xFF;
    REQUIRE(pq::mldsa_verify_batch(pub, msgs, sigs) == 9);
}

TEST_CASE("ML-DSA-44: base64 roundtrip of signature", "[mldsa][encoding]") {
    auto [priv, pub] = pq::mldsa_keygen();
    auto msg = bytes("base64 ml-dsa");
    auto sig = pq::mldsa_sign(priv, msg);
    auto b64 = pq::to_base64(sig);
    auto decoded = pq::from_base64(b64);
    REQUIRE(decoded == sig);
    REQUIRE(pq::mldsa_verify(pub, msg, decoded));
}
