// test_modes.cpp — round-trip tests for all 8 AES modes (aes1 namespace)
// NIST SP 800-38A vectors for block ciphers.
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

static const auto KEY16 = h("2b7e151628aed2a6abf7158809cf4f3c");  // AES-128
static const auto KEY32 = h("0000000000000000000000000000000000000000000000000000000000000000"); // XTS (32B = 2×AES-128)

// ── ECB ──────────────────────────────────────────────────────────────────────
TEST_CASE("ECB round-trip (16 B aligned)", "[modes][ecb]") {
    std::vector<uint8_t> pt(32, 0xAA);
    auto enc = aes1::encrypt(aes1::Mode::ECB, KEY16, {}, pt);
    auto dec = aes1::decrypt(aes1::Mode::ECB, KEY16, {}, enc.ciphertext);
    REQUIRE(dec == pt);
}

TEST_CASE("ECB identical blocks produce identical ciphertext (pattern leak)", "[modes][ecb][security]") {
    std::vector<uint8_t> pt(32, 0xBB); // two identical 16-byte blocks
    auto enc = aes1::encrypt(aes1::Mode::ECB, KEY16, {}, pt);
    REQUIRE(enc.ciphertext.size() == 32);
    std::vector<uint8_t> b1(enc.ciphertext.begin(),      enc.ciphertext.begin() + 16);
    std::vector<uint8_t> b2(enc.ciphertext.begin() + 16, enc.ciphertext.end());
    REQUIRE(b1 == b2); // proves ECB leaks structure
}

// ── CBC ──────────────────────────────────────────────────────────────────────
TEST_CASE("CBC round-trip", "[modes][cbc]") {
    auto iv = h("000102030405060708090a0b0c0d0e0f");
    std::vector<uint8_t> pt(32, 0x42);
    auto enc = aes1::encrypt(aes1::Mode::CBC, KEY16, iv, pt);
    auto dec = aes1::decrypt(aes1::Mode::CBC, KEY16, iv, enc.ciphertext);
    REQUIRE(dec == pt);
}

TEST_CASE("CBC IV auto-gen: IV stored in result", "[modes][cbc][iv]") {
    std::vector<uint8_t> pt(16, 0x55);
    auto enc = aes1::encrypt(aes1::Mode::CBC, KEY16, {}, pt);
    REQUIRE(enc.iv.size() == 16);
    auto dec = aes1::decrypt(aes1::Mode::CBC, KEY16, enc.iv, enc.ciphertext);
    REQUIRE(dec == pt);
}

TEST_CASE("CBC different IVs produce different ciphertexts", "[modes][cbc]") {
    auto iv1 = h("000102030405060708090a0b0c0d0e0f");
    auto iv2 = h("0f0e0d0c0b0a09080706050403020100");
    std::vector<uint8_t> pt(16, 0xCC);
    auto ct1 = aes1::encrypt(aes1::Mode::CBC, KEY16, iv1, pt).ciphertext;
    auto ct2 = aes1::encrypt(aes1::Mode::CBC, KEY16, iv2, pt).ciphertext;
    REQUIRE(ct1 != ct2);
}

// ── OFB ──────────────────────────────────────────────────────────────────────
TEST_CASE("OFB round-trip (arbitrary length)", "[modes][ofb]") {
    auto iv = h("000102030405060708090a0b0c0d0e0f");
    std::vector<uint8_t> pt = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // "Hello" (5 B)
    auto enc = aes1::encrypt(aes1::Mode::OFB, KEY16, iv, pt);
    auto dec = aes1::decrypt(aes1::Mode::OFB, KEY16, iv, enc.ciphertext);
    REQUIRE(dec == pt);
}

TEST_CASE("OFB round-trip (64 B)", "[modes][ofb]") {
    auto iv = h("000102030405060708090a0b0c0d0e0f");
    std::vector<uint8_t> pt(64, 0x77);
    auto enc = aes1::encrypt(aes1::Mode::OFB, KEY16, iv, pt);
    auto dec = aes1::decrypt(aes1::Mode::OFB, KEY16, iv, enc.ciphertext);
    REQUIRE(dec == pt);
}

// ── CFB ──────────────────────────────────────────────────────────────────────
TEST_CASE("CFB round-trip (arbitrary length)", "[modes][cfb]") {
    auto iv = h("000102030405060708090a0b0c0d0e0f");
    std::vector<uint8_t> pt = {0x73, 0x65, 0x63, 0x72, 0x65, 0x74}; // "secret"
    auto enc = aes1::encrypt(aes1::Mode::CFB, KEY16, iv, pt);
    auto dec = aes1::decrypt(aes1::Mode::CFB, KEY16, iv, enc.ciphertext);
    REQUIRE(dec == pt);
}

TEST_CASE("CFB round-trip (64 B)", "[modes][cfb]") {
    auto iv  = h("000102030405060708090a0b0c0d0e0f");
    std::vector<uint8_t> pt(64, 0x88);
    auto enc = aes1::encrypt(aes1::Mode::CFB, KEY16, iv, pt);
    auto dec = aes1::decrypt(aes1::Mode::CFB, KEY16, iv, enc.ciphertext);
    REQUIRE(dec == pt);
}

// ── CTR ──────────────────────────────────────────────────────────────────────
TEST_CASE("CTR round-trip (arbitrary length)", "[modes][ctr]") {
    auto iv = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    std::vector<uint8_t> pt(33, 0x99); // non-block-aligned
    auto enc = aes1::encrypt(aes1::Mode::CTR, KEY16, iv, pt);
    REQUIRE(enc.ciphertext.size() == 33);
    auto dec = aes1::decrypt(aes1::Mode::CTR, KEY16, iv, enc.ciphertext);
    REQUIRE(dec == pt);
}

TEST_CASE("CTR nonce-reuse: same keystream (two-time pad demo)", "[modes][ctr][security]") {
    auto iv  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    std::vector<uint8_t> pt1 = {0x41, 0x42, 0x43, 0x44}; // "ABCD"
    std::vector<uint8_t> pt2 = {0x11, 0x22, 0x33, 0x44};
    auto ct1 = aes1::encrypt(aes1::Mode::CTR, KEY16, iv, pt1).ciphertext;
    auto ct2 = aes1::encrypt(aes1::Mode::CTR, KEY16, iv, pt2).ciphertext;
    // CT1 XOR CT2 == PT1 XOR PT2 (keystream cancels)
    std::vector<uint8_t> ct_xor(4), pt_xor(4);
    for (int i = 0; i < 4; i++) { ct_xor[i] = ct1[i] ^ ct2[i]; pt_xor[i] = pt1[i] ^ pt2[i]; }
    REQUIRE(ct_xor == pt_xor);
}

// ── XTS ──────────────────────────────────────────────────────────────────────
TEST_CASE("XTS round-trip (32 B, 32-byte key)", "[modes][xts]") {
    auto iv = h("00000000000000000000000000000000"); // 16-byte sector tweak
    std::vector<uint8_t> pt(32, 0xAA);
    auto enc = aes1::encrypt(aes1::Mode::XTS, KEY32, iv, pt);
    auto dec = aes1::decrypt(aes1::Mode::XTS, KEY32, iv, enc.ciphertext);
    REQUIRE(dec == pt);
}

TEST_CASE("XTS round-trip (64 B)", "[modes][xts]") {
    auto iv = h("00000000000000000000000000000000");
    std::vector<uint8_t> pt(64, 0x5A);
    auto enc = aes1::encrypt(aes1::Mode::XTS, KEY32, iv, pt);
    auto dec = aes1::decrypt(aes1::Mode::XTS, KEY32, iv, enc.ciphertext);
    REQUIRE(dec == pt);
}

// ── GCM ──────────────────────────────────────────────────────────────────────
TEST_CASE("GCM round-trip", "[modes][gcm]") {
    auto iv = h("000000000000000000000000"); // 12-byte nonce
    std::vector<uint8_t> pt = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // "Hello"
    auto enc = aes1::encrypt(aes1::Mode::GCM, KEY16, iv, pt);
    REQUIRE(!enc.tag.empty());
    auto dec = aes1::decrypt(aes1::Mode::GCM, KEY16, iv, enc.ciphertext, enc.tag);
    REQUIRE(dec == pt);
}

TEST_CASE("GCM auto-generates 12-byte nonce", "[modes][gcm][iv]") {
    std::vector<uint8_t> pt = {0x41, 0x42, 0x43};
    auto enc = aes1::encrypt(aes1::Mode::GCM, KEY16, {}, pt);
    REQUIRE(enc.iv.size() == 12);
    REQUIRE(enc.iv != std::vector<uint8_t>(12, 0));
    auto dec = aes1::decrypt(aes1::Mode::GCM, KEY16, enc.iv, enc.ciphertext, enc.tag);
    REQUIRE(dec == pt);
}

// ── CCM ──────────────────────────────────────────────────────────────────────
TEST_CASE("CCM round-trip", "[modes][ccm]") {
    auto iv = h("00000003020100a0a1a2a3a4a5"); // 13-byte nonce
    std::vector<uint8_t> pt = {0x74, 0x65, 0x73, 0x74}; // "test"
    auto enc = aes1::encrypt(aes1::Mode::CCM, KEY16, iv, pt);
    REQUIRE(enc.tag.size() == 16);
    auto dec = aes1::decrypt(aes1::Mode::CCM, KEY16, iv, enc.ciphertext, enc.tag);
    REQUIRE(dec == pt);
}

TEST_CASE("CCM auto-generates 13-byte nonce", "[modes][ccm][iv]") {
    std::vector<uint8_t> pt(16, 0xFF);
    auto enc = aes1::encrypt(aes1::Mode::CCM, KEY16, {}, pt);
    REQUIRE(enc.iv.size() == 13);
    auto dec = aes1::decrypt(aes1::Mode::CCM, KEY16, enc.iv, enc.ciphertext, enc.tag);
    REQUIRE(dec == pt);
}
