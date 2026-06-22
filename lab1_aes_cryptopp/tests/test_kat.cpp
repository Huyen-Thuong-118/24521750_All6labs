// test_kat.cpp — NIST Known-Answer Tests hardcoded from SP 800-38A and SP 800-38D
// Sources: NIST SP 800-38A Appendix F; NIST SP 800-38D Appendix B
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "aes_modes.h"

static std::vector<uint8_t> h(const char* hex) {
    std::string s(hex);
    std::vector<uint8_t> r(s.size() / 2);
    for (size_t i = 0; i < r.size(); i++)
        r[i] = (uint8_t)std::stoul(s.substr(2*i, 2), nullptr, 16);
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// NIST SP 800-38A — CBC
// ─────────────────────────────────────────────────────────────────────────────

// F.2.1: CBC-AES-128, Block 1
TEST_CASE("KAT CBC-AES128 NIST F.2.1 block-1", "[kat][cbc]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("000102030405060708090a0b0c0d0e0f");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");
    auto exp = h("7649abac8119b246cee98e9b12e9197d");
    auto res = aes1::encrypt(aes1::Mode::CBC, key, iv, pt);
    REQUIRE(res.ciphertext == exp);
}

// F.2.1: CBC-AES-128, Block 2
TEST_CASE("KAT CBC-AES128 NIST F.2.1 block-2", "[kat][cbc]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("7649abac8119b246cee98e9b12e9197d"); // previous CT as IV
    auto pt  = h("ae2d8a571e03ac9c9eb76fac45af8e51");
    auto exp = h("5086cb9b507219ee95db113a917678b2");
    auto res = aes1::encrypt(aes1::Mode::CBC, key, iv, pt);
    REQUIRE(res.ciphertext == exp);
}

// F.2.5: CBC-AES-256, Block 1
TEST_CASE("KAT CBC-AES256 NIST F.2.5 block-1", "[kat][cbc]") {
    auto key = h("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    auto iv  = h("000102030405060708090a0b0c0d0e0f");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");
    auto exp = h("f58c4c04d6e5f1ba779eabfb5f7bfbd6");
    auto res = aes1::encrypt(aes1::Mode::CBC, key, iv, pt);
    REQUIRE(res.ciphertext == exp);
}

// ─────────────────────────────────────────────────────────────────────────────
// NIST SP 800-38A — OFB
// ─────────────────────────────────────────────────────────────────────────────

// F.4.1: OFB-AES-128, Block 1
TEST_CASE("KAT OFB-AES128 NIST F.4.1 block-1", "[kat][ofb]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("000102030405060708090a0b0c0d0e0f");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");
    auto exp = h("3b3fd92eb72dad20333449f8e83cfb4a");
    auto res = aes1::encrypt(aes1::Mode::OFB, key, iv, pt);
    REQUIRE(res.ciphertext == exp);
}

// F.4.1: OFB-AES-128, Block 2
TEST_CASE("KAT OFB-AES128 NIST F.4.1 block-2", "[kat][ofb]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("50fe67cc996d32b6da0937e99bafec60"); // OFB stream evolved
    auto pt  = h("ae2d8a571e03ac9c9eb76fac45af8e51");
    auto exp = h("7789508d16918f03f53c52dac54ed825");
    auto res = aes1::encrypt(aes1::Mode::OFB, key, iv, pt);
    REQUIRE(res.ciphertext == exp);
}

// ─────────────────────────────────────────────────────────────────────────────
// NIST SP 800-38A — CFB (CFB128)
// ─────────────────────────────────────────────────────────────────────────────

// F.3.13: CFB128-AES-128, Block 1
TEST_CASE("KAT CFB128-AES128 NIST F.3.13 block-1", "[kat][cfb]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("000102030405060708090a0b0c0d0e0f");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");
    auto exp = h("3b3fd92eb72dad20333449f8e83cfb4a");
    auto res = aes1::encrypt(aes1::Mode::CFB, key, iv, pt);
    REQUIRE(res.ciphertext == exp);
}

// ─────────────────────────────────────────────────────────────────────────────
// NIST SP 800-38A — CTR
// ─────────────────────────────────────────────────────────────────────────────

// F.5.1: CTR-AES-128, Block 1
TEST_CASE("KAT CTR-AES128 NIST F.5.1 block-1", "[kat][ctr]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");
    auto exp = h("874d6191b620e3261bef6864990db6ce");
    auto res = aes1::encrypt(aes1::Mode::CTR, key, iv, pt);
    REQUIRE(res.ciphertext == exp);
}

// F.5.5: CTR-AES-256 (Crypto++ counter variant — blocks 1-3 match NIST)
TEST_CASE("KAT CTR-AES256 NIST F.5.5 block-1", "[kat][ctr]") {
    auto key = h("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    auto iv  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");
    auto exp = h("601ec313775789a5b7a7f504bbf3d228");
    auto res = aes1::encrypt(aes1::Mode::CTR, key, iv, pt);
    REQUIRE(res.ciphertext == exp);
}

// ─────────────────────────────────────────────────────────────────────────────
// NIST SP 800-38D — GCM
// ─────────────────────────────────────────────────────────────────────────────

// TC1: AES-128-GCM, empty plaintext, empty AAD
TEST_CASE("KAT GCM-AES128 NIST TC1 empty PT empty AAD", "[kat][gcm]") {
    auto key = h("00000000000000000000000000000000");
    auto iv  = h("000000000000000000000000");
    std::vector<uint8_t> pt;
    auto exp_tag = h("58e2fccefa7e3061367f1d57a4e7455a");
    auto res = aes1::encrypt(aes1::Mode::GCM, key, iv, pt);
    REQUIRE(res.ciphertext.empty());
    REQUIRE(res.tag == exp_tag);
}

// TC2: AES-128-GCM, 16-byte zero plaintext
TEST_CASE("KAT GCM-AES128 NIST TC2 16-byte zero PT", "[kat][gcm]") {
    auto key    = h("00000000000000000000000000000000");
    auto iv     = h("000000000000000000000000");
    auto pt     = h("00000000000000000000000000000000");
    auto exp_ct = h("0388dace60b6a392f328c2b971b2fe78");
    auto exp_tag= h("ab6e47d42cec13bdf53a67b21257bddf");
    auto res = aes1::encrypt(aes1::Mode::GCM, key, iv, pt);
    REQUIRE(res.ciphertext == exp_ct);
    REQUIRE(res.tag == exp_tag);
}

// TC3: AES-128-GCM with known key/nonce, 60-byte PT (truncated to 60 bytes in vector)
TEST_CASE("KAT GCM-AES128 NIST TC3 known key/nonce", "[kat][gcm]") {
    auto key = h("feffe9928665731c6d6a8f9467308308");
    auto iv  = h("cafebabefacedbaddecaf888");
    auto pt  = h("d9313225f88406e5a55909c5aff5269a"
                 "86a7a9531534f7da2e4c303d8a318a72"
                 "1c3c0c95956809532fcf0e2449a6b525"
                 "b16aedf5aa0de657ba637b391aafd255");
    auto exp_ct = h("42831ec2217774244b7221b784d0d49c"
                    "e3aa212f2c02a4e035c17e2329aca12e"
                    "21d514b25466931c7d8f6a5aac84aa05"
                    "1ba30b396a0aac973d58e091473f5985");
    auto exp_tag = h("4d5c2af327cd64a62cf35abd2ba6fab4");
    auto res = aes1::encrypt(aes1::Mode::GCM, key, iv, pt);
    REQUIRE(res.ciphertext == exp_ct);
    REQUIRE(res.tag == exp_tag);
}

// ─────────────────────────────────────────────────────────────────────────────
// CCM — roundtrip KAT (NIST SP 800-38C params, T=16 implementation default)
// Note: NIST 800-38C uses T=8; our impl uses T=16; no CT expected → roundtrip
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("KAT CCM NIST C.1 params roundtrip (T=16)", "[kat][ccm]") {
    auto key = h("c0c1c2c3c4c5c6c7c8c9cacbcccdcecf");
    auto iv  = h("00000003020100a0a1a2a3a4a5");
    auto aad = h("0001020304050607");
    auto pt  = h("08090a0b0c0d0e0f101112131415161718191a1b1c1d1e");
    auto enc = aes1::encrypt(aes1::Mode::CCM, key, iv, pt, aad);
    REQUIRE(enc.tag.size() == 16);
    auto dec = aes1::decrypt(aes1::Mode::CCM, key, iv, enc.ciphertext, enc.tag, aad);
    REQUIRE(dec == pt);
}

TEST_CASE("KAT CCM NIST C.2 params roundtrip (T=16)", "[kat][ccm]") {
    auto key = h("c0c1c2c3c4c5c6c7c8c9cacbcccdcecf");
    auto iv  = h("00000004030201a0a1a2a3a4a5");
    auto aad = h("0001020304050607");
    auto pt  = h("08090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    auto enc = aes1::encrypt(aes1::Mode::CCM, key, iv, pt, aad);
    REQUIRE(enc.tag.size() == 16);
    auto dec = aes1::decrypt(aes1::Mode::CCM, key, iv, enc.ciphertext, enc.tag, aad);
    REQUIRE(dec == pt);
}
