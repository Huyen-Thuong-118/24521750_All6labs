// ML-KEM-512 tests — keygen, encaps/decaps, shared-secret match, negative
#include <catch2/catch_test_macros.hpp>
#include "pqtool.hpp"
#include <openssl/ml_kem.h>
#include <vector>
#include <string>

TEST_CASE("ML-KEM-512 keygen: PEM non-empty", "[mlkem][keygen]") {
    auto [priv, pub] = pq::mlkem_keygen();
    REQUIRE(priv.find("PRIVATE KEY") != std::string::npos);
    REQUIRE(pub.find("PUBLIC KEY")   != std::string::npos);
}

TEST_CASE("ML-KEM-512 keygen: two calls produce different keys", "[mlkem][keygen]") {
    auto [p1, u1] = pq::mlkem_keygen();
    auto [p2, u2] = pq::mlkem_keygen();
    REQUIRE(p1 != p2);
}

TEST_CASE("ML-KEM-512: encaps/decaps shared secret match", "[mlkem][kem]") {
    auto [priv, pub] = pq::mlkem_keygen();
    auto [ct, ss_encaps] = pq::mlkem_encaps(pub);
    auto ss_decaps = pq::mlkem_decaps(priv, ct);
    REQUIRE(!ct.empty());
    REQUIRE(!ss_encaps.empty());
    REQUIRE(ss_encaps == ss_decaps);
}

TEST_CASE("ML-KEM-512: ciphertext size = 768 bytes", "[mlkem][format]") {
    auto [priv, pub] = pq::mlkem_keygen();
    auto [ct, ss] = pq::mlkem_encaps(pub);
    REQUIRE(ct.size() == static_cast<size_t>(OSSL_ML_KEM_512_CIPHERTEXT_BYTES));
}

TEST_CASE("ML-KEM-512: shared secret size = 32 bytes", "[mlkem][format]") {
    auto [priv, pub] = pq::mlkem_keygen();
    auto [ct, ss] = pq::mlkem_encaps(pub);
    REQUIRE(ss.size() == static_cast<size_t>(OSSL_ML_KEM_SHARED_SECRET_BYTES));
    auto ss2 = pq::mlkem_decaps(priv, ct);
    REQUIRE(ss2.size() == static_cast<size_t>(OSSL_ML_KEM_SHARED_SECRET_BYTES));
}

TEST_CASE("ML-KEM-512: different encaps give different ct/ss", "[mlkem][format]") {
    auto [priv, pub] = pq::mlkem_keygen();
    auto [ct1, ss1] = pq::mlkem_encaps(pub);
    auto [ct2, ss2] = pq::mlkem_encaps(pub);
    REQUIRE(ct1 != ct2);
    REQUIRE(ss1 != ss2);
}

TEST_CASE("ML-KEM-512: wrong private key gives mismatched ss (implicit rejection)", "[mlkem][negative]") {
    auto [priv1, pub1] = pq::mlkem_keygen();
    auto [priv2, pub2] = pq::mlkem_keygen();
    auto [ct, ss_correct] = pq::mlkem_encaps(pub1);
    // Decaps with wrong key → implicit rejection (returns pseudo-random ss)
    auto ss_wrong = pq::mlkem_decaps(priv2, ct);
    // Both are 32 bytes but values should differ
    REQUIRE(ss_wrong.size() == 32);
    REQUIRE(ss_wrong != ss_correct);
}

TEST_CASE("ML-KEM-512: modified ciphertext gives different ss (implicit rejection, no throw)", "[mlkem][negative]") {
    auto [priv, pub] = pq::mlkem_keygen();
    auto [ct, ss_orig] = pq::mlkem_encaps(pub);
    ct[0] ^= 0xFF; // flip a byte
    // Must NOT throw; returns pseudo-random ss
    std::vector<uint8_t> ss_bad;
    REQUIRE_NOTHROW(ss_bad = pq::mlkem_decaps(priv, ct));
    REQUIRE(ss_bad.size() == 32);
    REQUIRE(ss_bad != ss_orig);
}

TEST_CASE("ML-KEM-512: encaps 5 times, all decaps give matching ss", "[mlkem][kem]") {
    auto [priv, pub] = pq::mlkem_keygen();
    for (int i = 0; i < 5; i++) {
        auto [ct, ss_e] = pq::mlkem_encaps(pub);
        auto ss_d = pq::mlkem_decaps(priv, ct);
        REQUIRE(ss_e == ss_d);
    }
}

TEST_CASE("ML-KEM-512: base64 roundtrip of ciphertext", "[mlkem][encoding]") {
    auto [priv, pub] = pq::mlkem_keygen();
    auto [ct, ss_e] = pq::mlkem_encaps(pub);
    auto b64 = pq::to_base64(ct);
    auto ct2 = pq::from_base64(b64);
    REQUIRE(ct == ct2);
    auto ss_d = pq::mlkem_decaps(priv, ct2);
    REQUIRE(ss_e == ss_d);
}
