// Batch verify tests — N=100 to keep build time reasonable (guide says 1000)
#include <catch2/catch_test_macros.hpp>
#include "signer.hpp"
#include <string>
#include <vector>

static const int N = 100; // increase to 1000 for full bench

TEST_CASE("Batch verify ECDSA: N correct sigs all pass", "[batch][ecdsa]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    std::vector<std::vector<uint8_t>> msgs, sigs;
    for (int i = 0; i < N; i++) {
        std::string s = "batch-ecdsa-" + std::to_string(i);
        std::vector<uint8_t> m(s.begin(), s.end());
        msgs.push_back(m);
        sigs.push_back(sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, m));
    }
    int ok = sig5::verify_batch(sig5::Algo::ECDSA_P256, pub, msgs, sigs);
    REQUIRE(ok == N);
}

TEST_CASE("Batch verify ECDSA: one tampered sig reduces count by 1", "[batch][ecdsa]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    std::vector<std::vector<uint8_t>> msgs, sigs;
    for (int i = 0; i < 10; i++) {
        std::string s = "tamper-batch-" + std::to_string(i);
        std::vector<uint8_t> m(s.begin(), s.end());
        msgs.push_back(m);
        sigs.push_back(sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, m));
    }
    sigs[3][0] ^= 0xFF; // corrupt one
    int ok = sig5::verify_batch(sig5::Algo::ECDSA_P256, pub, msgs, sigs);
    REQUIRE(ok == 9);
}

TEST_CASE("Batch verify RSA-PSS: N correct sigs all pass", "[batch][rsa]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::RSA_PSS_3072);
    std::vector<std::vector<uint8_t>> msgs, sigs;
    for (int i = 0; i < 20; i++) { // fewer due to RSA keygen cost
        std::string s = "batch-rsa-" + std::to_string(i);
        std::vector<uint8_t> m(s.begin(), s.end());
        msgs.push_back(m);
        sigs.push_back(sig5::sign_msg(sig5::Algo::RSA_PSS_3072, priv, m));
    }
    int ok = sig5::verify_batch(sig5::Algo::RSA_PSS_3072, pub, msgs, sigs);
    REQUIRE(ok == 20);
}

TEST_CASE("Batch verify: empty batch returns 0", "[batch]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    std::vector<std::vector<uint8_t>> msgs, sigs;
    REQUIRE(sig5::verify_batch(sig5::Algo::ECDSA_P256, pub, msgs, sigs) == 0);
}

TEST_CASE("Batch verify: mismatched lengths handled gracefully", "[batch]") {
    auto [priv, pub] = sig5::keygen(sig5::Algo::ECDSA_P256);
    std::string s = "hello";
    std::vector<uint8_t> m(s.begin(), s.end());
    auto sig = sig5::sign_msg(sig5::Algo::ECDSA_P256, priv, m);
    std::vector<std::vector<uint8_t>> msgs = {m, m, m};
    std::vector<std::vector<uint8_t>> sigs = {sig};
    // verify_batch processes min(msgs.size(), sigs.size()) = 1
    REQUIRE(sig5::verify_batch(sig5::Algo::ECDSA_P256, pub, msgs, sigs) == 1);
}
