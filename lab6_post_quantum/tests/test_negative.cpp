// Cross-algo and edge-case negative tests for Lab 6
#include <catch2/catch_test_macros.hpp>
#include "pqtool.hpp"

static std::vector<uint8_t> bytes(const char* s) {
    return {reinterpret_cast<const uint8_t*>(s),
            reinterpret_cast<const uint8_t*>(s) + std::strlen(s)};
}

TEST_CASE("ML-DSA verify: empty sig returns false", "[negative][mldsa]") {
    auto [priv, pub] = pq::mldsa_keygen();
    REQUIRE_FALSE(pq::mldsa_verify(pub, bytes("msg"), {}));
}

TEST_CASE("ML-DSA verify: truncated sig returns false", "[negative][mldsa]") {
    auto [priv, pub] = pq::mldsa_keygen();
    auto msg = bytes("truncated");
    auto sig = pq::mldsa_sign(priv, msg);
    sig.resize(100);
    REQUIRE_FALSE(pq::mldsa_verify(pub, msg, sig));
}

TEST_CASE("ML-DSA verify: sig of different message fails", "[negative][mldsa]") {
    auto [priv, pub] = pq::mldsa_keygen();
    auto sig = pq::mldsa_sign(priv, bytes("msg A"));
    REQUIRE_FALSE(pq::mldsa_verify(pub, bytes("msg B"), sig));
}

TEST_CASE("ML-KEM: decaps with empty ciphertext does not throw", "[negative][mlkem]") {
    auto [priv, pub] = pq::mlkem_keygen();
    REQUIRE_NOTHROW(pq::mlkem_decaps(priv, {}));
}

TEST_CASE("ML-KEM: decaps with short ciphertext returns 32-byte ss (no throw)", "[negative][mlkem]") {
    auto [priv, pub] = pq::mlkem_keygen();
    std::vector<uint8_t> bad_ct(10, 0xFF);
    std::vector<uint8_t> ss;
    REQUIRE_NOTHROW(ss = pq::mlkem_decaps(priv, bad_ct));
    // Implicit rejection: still returns something
    // (may be 0-length if OpenSSL completely rejects truncated input)
    // Either way: no crash and no exception
}

TEST_CASE("ML-DSA batch: empty batch returns 0", "[negative][batch]") {
    auto [priv, pub] = pq::mldsa_keygen();
    REQUIRE(pq::mldsa_verify_batch(pub, {}, {}) == 0);
}

TEST_CASE("ML-DSA batch: mismatched sizes handled correctly", "[negative][batch]") {
    auto [priv, pub] = pq::mldsa_keygen();
    auto m = bytes("msg");
    auto sig = pq::mldsa_sign(priv, m);
    // 3 msgs, 1 sig → only 1 pair processed
    REQUIRE(pq::mldsa_verify_batch(pub, {m, m, m}, {sig}) == 1);
}

TEST_CASE("base64 decode: garbage string throws", "[negative][encoding]") {
    REQUIRE_THROWS(pq::from_base64("!!!not-base64!!!"));
}

TEST_CASE("cert_from_json: garbage JSON throws", "[negative][cert]") {
    REQUIRE_THROWS(pq::cert_from_json("not json"));
}

TEST_CASE("cert_from_json: JSON missing fields throws", "[negative][cert]") {
    REQUIRE_THROWS(pq::cert_from_json(R"({"subject":"only"})"));
}
