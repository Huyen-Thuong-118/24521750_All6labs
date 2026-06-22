// test_aead.cpp — GCM and CCM AEAD-specific tests (tag, AAD binding)
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <string>
#include "aes_modes.h"

static std::vector<uint8_t> h(const char* hex) {
    std::string s(hex);
    std::vector<uint8_t> r(s.size() / 2);
    for (size_t i = 0; i < r.size(); i++)
        r[i] = (uint8_t)std::stoul(s.substr(2*i, 2), nullptr, 16);
    return r;
}

static std::vector<uint8_t> str_bytes(const std::string& s) {
    return {s.begin(), s.end()};
}

static const auto KEY = h("2b7e151628aed2a6abf7158809cf4f3c");
static const auto GCM_IV = h("cafebabefacedbaddecaf888"); // 12 B
static const auto CCM_IV = h("00000003020100a0a1a2a3a4a5");  // 13 B

// ── GCM — tag generation ─────────────────────────────────────────────────────

TEST_CASE("GCM produces 16-byte tag", "[aead][gcm]") {
    std::vector<uint8_t> pt(32, 0xAB);
    auto res = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt);
    REQUIRE(res.tag.size() == aes1::TAG_LEN);
}

TEST_CASE("GCM tag changes when plaintext changes", "[aead][gcm]") {
    std::vector<uint8_t> pt1(16, 0x11);
    std::vector<uint8_t> pt2(16, 0x22);
    auto t1 = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt1).tag;
    auto t2 = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt2).tag;
    REQUIRE(t1 != t2);
}

// ── GCM — AAD ────────────────────────────────────────────────────────────────

TEST_CASE("GCM round-trip with AAD", "[aead][gcm]") {
    auto pt  = str_bytes("plaintext data here");
    auto aad = str_bytes("authenticated-header");
    auto enc = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt, aad);
    auto dec = aes1::decrypt(aes1::Mode::GCM, KEY, GCM_IV, enc.ciphertext, enc.tag, aad);
    REQUIRE(dec == pt);
}

TEST_CASE("GCM AAD is authenticated but not encrypted (CT length unchanged)", "[aead][gcm]") {
    auto pt  = std::vector<uint8_t>(32, 0x55);
    auto aad = str_bytes("header");
    auto enc_no_aad  = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt);
    auto enc_with_aad = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt, aad);
    // Ciphertext size is the same (AAD not encrypted)
    REQUIRE(enc_no_aad.ciphertext.size() == enc_with_aad.ciphertext.size());
    // But tags differ (AAD is part of auth)
    REQUIRE(enc_no_aad.tag != enc_with_aad.tag);
}

TEST_CASE("GCM empty plaintext with AAD: tag still valid", "[aead][gcm]") {
    std::vector<uint8_t> pt;
    auto aad = str_bytes("only-authenticated");
    auto enc = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt, aad);
    REQUIRE(enc.ciphertext.empty());
    REQUIRE(enc.tag.size() == 16);
    auto dec = aes1::decrypt(aes1::Mode::GCM, KEY, GCM_IV, enc.ciphertext, enc.tag, aad);
    REQUIRE(dec.empty());
}

// ── GCM — authentication failures (fail-closed) ──────────────────────────────

TEST_CASE("GCM tampered ciphertext → authentication fail, no plaintext output", "[aead][gcm][negative]") {
    auto pt  = std::vector<uint8_t>(16, 0xAA);
    auto enc = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt);
    enc.ciphertext[0] ^= 0xFF;
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::GCM, KEY, GCM_IV, enc.ciphertext, enc.tag));
}

TEST_CASE("GCM tampered tag → authentication fail", "[aead][gcm][negative]") {
    auto pt  = std::vector<uint8_t>(16, 0xBB);
    auto enc = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt);
    enc.tag[0] ^= 0xFF;
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::GCM, KEY, GCM_IV, enc.ciphertext, enc.tag));
}

TEST_CASE("GCM wrong AAD on decrypt → authentication fail", "[aead][gcm][negative]") {
    auto pt      = str_bytes("secret message");
    auto correct = str_bytes("correct-header");
    auto wrong   = str_bytes("wrong-header");
    auto enc = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt, correct);
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::GCM, KEY, GCM_IV, enc.ciphertext, enc.tag, wrong));
}

TEST_CASE("GCM missing AAD on decrypt → authentication fail", "[aead][gcm][negative]") {
    auto pt  = str_bytes("data");
    auto aad = str_bytes("header");
    auto enc = aes1::encrypt(aes1::Mode::GCM, KEY, GCM_IV, pt, aad);
    // Decrypt without AAD must fail
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::GCM, KEY, GCM_IV, enc.ciphertext, enc.tag));
}

// ── CCM — tag generation ─────────────────────────────────────────────────────

TEST_CASE("CCM produces 16-byte tag", "[aead][ccm]") {
    std::vector<uint8_t> pt(32, 0xCC);
    auto res = aes1::encrypt(aes1::Mode::CCM, KEY, CCM_IV, pt);
    REQUIRE(res.tag.size() == aes1::TAG_LEN);
}

// ── CCM — AAD ────────────────────────────────────────────────────────────────

TEST_CASE("CCM round-trip with AAD", "[aead][ccm]") {
    auto pt  = str_bytes("ccm payload");
    auto aad = str_bytes("ccm-header");
    auto enc = aes1::encrypt(aes1::Mode::CCM, KEY, CCM_IV, pt, aad);
    auto dec = aes1::decrypt(aes1::Mode::CCM, KEY, CCM_IV, enc.ciphertext, enc.tag, aad);
    REQUIRE(dec == pt);
}

// ── CCM — authentication failures ────────────────────────────────────────────

TEST_CASE("CCM tampered ciphertext → authentication fail", "[aead][ccm][negative]") {
    auto pt  = std::vector<uint8_t>(16, 0xDD);
    auto enc = aes1::encrypt(aes1::Mode::CCM, KEY, CCM_IV, pt);
    enc.ciphertext[0] ^= 0x01;
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::CCM, KEY, CCM_IV, enc.ciphertext, enc.tag));
}

TEST_CASE("CCM tampered tag → authentication fail", "[aead][ccm][negative]") {
    auto pt  = std::vector<uint8_t>(16, 0xEE);
    auto enc = aes1::encrypt(aes1::Mode::CCM, KEY, CCM_IV, pt);
    enc.tag[0] ^= 0xFF;
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::CCM, KEY, CCM_IV, enc.ciphertext, enc.tag));
}

TEST_CASE("CCM wrong AAD on decrypt → authentication fail", "[aead][ccm][negative]") {
    auto pt      = str_bytes("payload");
    auto correct = str_bytes("aad-ok");
    auto wrong   = str_bytes("aad-xx");
    auto enc = aes1::encrypt(aes1::Mode::CCM, KEY, CCM_IV, pt, correct);
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::CCM, KEY, CCM_IV, enc.ciphertext, enc.tag, wrong));
}
