// test_fips197.cpp — FIPS-197 AES-128 block cipher Known-Answer Tests
// Vectors from FIPS-197 Appendix B and C.1
// Also tests KeyExpansion against FIPS-197 Appendix A.1
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <array>
#include <cstring>
#include "aes_core.hpp"
#include "aes_tables.hpp"

static std::vector<uint8_t> h(const char* hex) {
    std::string s(hex);
    std::vector<uint8_t> r(s.size()/2);
    for (size_t i=0;i<r.size();i++) r[i]=(uint8_t)std::stoul(s.substr(2*i,2),nullptr,16);
    return r;
}

// ── FIPS-197 Appendix B: main encrypt example ─────────────────────────────────
TEST_CASE("FIPS-197 Appendix B: AES-128 encryptBlock", "[fips197][encrypt]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto pt  = h("3243f6a8885a308d313198a2e0370734");
    auto exp = h("3925841d02dc09fbdc118597196a0b32");

    aes2::AesCore aes(key.data());
    uint8_t out[16];
    aes.encryptBlock(pt.data(), out);

    REQUIRE(std::vector<uint8_t>(out,out+16) == exp);
}

// ── FIPS-197 Appendix B: main decrypt example ────────────────────────────────
TEST_CASE("FIPS-197 Appendix B: AES-128 decryptBlock", "[fips197][decrypt]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto ct  = h("3925841d02dc09fbdc118597196a0b32");
    auto exp = h("3243f6a8885a308d313198a2e0370734");

    aes2::AesCore aes(key.data());
    uint8_t out[16];
    aes.decryptBlock(ct.data(), out);

    REQUIRE(std::vector<uint8_t>(out,out+16) == exp);
}

// ── FIPS-197 Appendix C.1: AES-128 all-zero test ─────────────────────────────
TEST_CASE("FIPS-197 C.1: AES-128 all-zero key/plaintext", "[fips197][encrypt]") {
    auto key = h("00000000000000000000000000000000");
    auto pt  = h("00000000000000000000000000000000");
    auto exp = h("66e94bd4ef8a2c3b884cfa59ca342b2e");

    aes2::AesCore aes(key.data());
    uint8_t out[16];
    aes.encryptBlock(pt.data(), out);

    REQUIRE(std::vector<uint8_t>(out,out+16) == exp);
}

// ── NIST AES-128 key/pt vector ────────────────────────────────────────────────
TEST_CASE("NIST AES-128 sequential key/pt", "[fips197][encrypt]") {
    auto key = h("000102030405060708090a0b0c0d0e0f");
    auto pt  = h("00112233445566778899aabbccddeeff");
    auto exp = h("69c4e0d86a7b04300d8a8b41d0a58f54");

    aes2::AesCore aes(key.data());
    uint8_t out[16];
    aes.encryptBlock(pt.data(), out);

    REQUIRE(std::vector<uint8_t>(out,out+16) == exp);
}

// ── Round-trip: encrypt then decrypt = identity ───────────────────────────────
TEST_CASE("AES-128 encrypt-decrypt round-trip", "[fips197]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");

    aes2::AesCore aes(key.data());
    uint8_t enc[16], dec[16];
    aes.encryptBlock(pt.data(), enc);
    aes.decryptBlock(enc, dec);

    REQUIRE(std::vector<uint8_t>(dec,dec+16) == pt);
}

// ── KeyExpansion: verify round key 1 against FIPS-197 Appendix A.1 ───────────
// FIPS-197 Appendix A.1: key = 2b7e151628aed2a6abf7158809cf4f3c
// Round key 1 (words w[4..7]) = a0fafe17 88542cb123a339392a6c7605
TEST_CASE("KeyExpansion round key 1 matches FIPS-197 Appendix A.1", "[fips197][keyexp]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    aes2::AesCore aes(key.data());

    // round key 1 = bytes 16..31 of expanded key = w[4..7]
    auto rk1_exp = h("a0fafe1788542cb123a339392a6c7605");
    std::vector<uint8_t> rk1(aes.roundKey(1), aes.roundKey(1) + 16);
    REQUIRE(rk1 == rk1_exp);
}

// Round key 2 = a0fafe17 88542cb1 23a33939 2a6c7605 ⊕ ...
// FIPS-197 Appendix A.1: w[8..11] = f2c295f27a96b9435935807a7359f67f
TEST_CASE("KeyExpansion round key 2 matches FIPS-197 Appendix A.1", "[fips197][keyexp]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    aes2::AesCore aes(key.data());

    auto rk2_exp = h("f2c295f27a96b9435935807a7359f67f");
    std::vector<uint8_t> rk2(aes.roundKey(2), aes.roundKey(2) + 16);
    REQUIRE(rk2 == rk2_exp);
}

// Round key 10 (final) = w[40..43]
// FIPS-197 Appendix A.1: 13111d7fe3944a17f307a78b4d2b30c5
TEST_CASE("KeyExpansion round key 10 (final) matches FIPS-197 Appendix A.1", "[fips197][keyexp]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    aes2::AesCore aes(key.data());

    auto rk10_exp = h("13111d7fe3944a17f307a78b4d2b30c5");
    std::vector<uint8_t> rk10(aes.roundKey(10), aes.roundKey(10) + 16);
    REQUIRE(rk10 == rk10_exp);
}

// ── GF(2^8) xtime sanity ─────────────────────────────────────────────────────
TEST_CASE("GF xtime: 0x53 * 2 = 0xa6", "[gf256]") {
    REQUIRE(aes2::xtime(0x53) == 0xa6);
}
TEST_CASE("GF xtime overflow: 0x80 * 2 = 0x1b (mod 0x11b)", "[gf256]") {
    REQUIRE(aes2::xtime(0x80) == 0x1b);
}
TEST_CASE("GF mul3: 0x53 * 3 = 0xf5", "[gf256]") {
    // mul3(0x53) = xtime(0x53) ^ 0x53 = 0xa6 ^ 0x53 = 0xf5
    REQUIRE(aes2::mul3(0x53) == 0xf5);
}
TEST_CASE("GF gf_mul: 0x57 * 0x13 = 0xfe (FIPS-197 §4.2 example)", "[gf256]") {
    REQUIRE(aes2::gf_mul(0x57, 0x13) == 0xfe);
}

// ── S-box spot checks (FIPS-197 Table 4) ─────────────────────────────────────
TEST_CASE("S-box: SBOX[0x53] = 0xed", "[sbox]") {
    REQUIRE(aes2::SBOX[0x53] == 0xed);
}
TEST_CASE("S-box: SBOX[0x00] = 0x63", "[sbox]") {
    REQUIRE(aes2::SBOX[0x00] == 0x63);
}
TEST_CASE("S-box: SBOX[0xff] = 0x16", "[sbox]") {
    REQUIRE(aes2::SBOX[0xff] == 0x16);
}
TEST_CASE("Inverse S-box: INV_SBOX[SBOX[b]] == b for all b", "[sbox]") {
    for (int b = 0; b < 256; ++b)
        REQUIRE(aes2::INV_SBOX[aes2::SBOX[b]] == (uint8_t)b);
}
