// test_crossval.cpp — Cross-validation: our AES-128-CTR vs Crypto++
// This file is the ONLY test binary that links Crypto++.
// The main tool (lab2_aestool) must NOT link any crypto library.
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <cstring>
#include <random>

// Our implementation
#include "aes_core.hpp"
#include "ctr_mode.hpp"

// Crypto++ for reference
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/filters.h>

static std::vector<uint8_t> h(const char* hex) {
    std::string s(hex);
    std::vector<uint8_t> r(s.size()/2);
    for (size_t i=0;i<r.size();i++) r[i]=(uint8_t)std::stoul(s.substr(2*i,2),nullptr,16);
    return r;
}

static std::vector<uint8_t> cryptopp_ctr(
    const uint8_t* key, const uint8_t* iv, const uint8_t* pt, size_t len)
{
    std::vector<uint8_t> out(len);
    CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption enc;
    enc.SetKeyWithIV(key, 16, iv, 16);
    enc.ProcessData(out.data(), pt, len);
    return out;
}

// ── NIST SP 800-38A F.5.1 vectors against Crypto++ ────────────────────────────
TEST_CASE("Cross-val: F.5.1 Block 1 matches Crypto++", "[crossval]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");

    aes2::AesCore aes(key.data());
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> our_ct(16);
    ctr.process(pt.data(), our_ct.data(), 16, iv.data());

    auto ref_ct = cryptopp_ctr(key.data(), iv.data(), pt.data(), 16);
    REQUIRE(our_ct == ref_ct);
}

TEST_CASE("Cross-val: 4-block concatenated matches Crypto++", "[crossval]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a"
                 "ae2d8a571e03ac9c9eb76fac45af8e51"
                 "30c81c46a35ce411e5fbc1191a0a52ef"
                 "f69f2445df4f9b17ad2b417be66c3710");

    aes2::AesCore aes(key.data());
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> our_ct(64);
    ctr.process(pt.data(), our_ct.data(), 64, iv.data());

    auto ref_ct = cryptopp_ctr(key.data(), iv.data(), pt.data(), 64);
    REQUIRE(our_ct == ref_ct);
}

// ── Random key/IV/plaintext cross-validation ──────────────────────────────────
TEST_CASE("Cross-val: random 16-byte message", "[crossval][random]") {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);

    uint8_t key[16], iv[16], pt[16];
    for (auto& b : key) b = (uint8_t)dist(rng);
    for (auto& b : iv)  b = (uint8_t)dist(rng);
    for (auto& b : pt)  b = (uint8_t)dist(rng);

    aes2::AesCore aes(key);
    aes2::CtrMode ctr(aes);
    uint8_t our_ct[16];
    ctr.process(pt, our_ct, 16, iv);

    auto ref_ct = cryptopp_ctr(key, iv, pt, 16);
    REQUIRE(std::vector<uint8_t>(our_ct,our_ct+16) == ref_ct);
}

TEST_CASE("Cross-val: random 100-byte message (partial block)", "[crossval][random]") {
    std::mt19937 rng(99);
    std::uniform_int_distribution<int> dist(0, 255);

    uint8_t key[16], iv[16];
    std::vector<uint8_t> pt(100);
    for (auto& b : key) b = (uint8_t)dist(rng);
    for (auto& b : iv)  b = (uint8_t)dist(rng);
    for (auto& b : pt)  b = (uint8_t)dist(rng);

    aes2::AesCore aes(key);
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> our_ct(100);
    ctr.process(pt.data(), our_ct.data(), 100, iv);

    auto ref_ct = cryptopp_ctr(key, iv, pt.data(), 100);
    REQUIRE(our_ct == ref_ct);
}

TEST_CASE("Cross-val: random 1000-byte message", "[crossval][random]") {
    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> dist(0, 255);

    uint8_t key[16], iv[16];
    std::vector<uint8_t> pt(1000);
    for (auto& b : key) b = (uint8_t)dist(rng);
    for (auto& b : iv)  b = (uint8_t)dist(rng);
    for (auto& b : pt)  b = (uint8_t)dist(rng);

    aes2::AesCore aes(key);
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> our_ct(1000);
    ctr.process(pt.data(), our_ct.data(), 1000, iv);

    auto ref_ct = cryptopp_ctr(key, iv, pt.data(), 1000);
    REQUIRE(our_ct == ref_ct);
}

TEST_CASE("Cross-val: 5 random keys × 5 random messages", "[crossval][random]") {
    std::mt19937 rng(2024);
    std::uniform_int_distribution<int> dist(0, 255);
    std::uniform_int_distribution<int> len_dist(1, 256);

    for (int trial = 0; trial < 5; ++trial) {
        uint8_t key[16], iv[16];
        for (auto& b : key) b = (uint8_t)dist(rng);
        for (auto& b : iv)  b = (uint8_t)dist(rng);
        size_t len = (size_t)len_dist(rng);
        std::vector<uint8_t> pt(len);
        for (auto& b : pt) b = (uint8_t)dist(rng);

        aes2::AesCore aes(key);
        aes2::CtrMode ctr(aes);
        std::vector<uint8_t> our_ct(len);
        ctr.process(pt.data(), our_ct.data(), len, iv);

        auto ref_ct = cryptopp_ctr(key, iv, pt.data(), len);
        REQUIRE(our_ct == ref_ct);
    }
}
