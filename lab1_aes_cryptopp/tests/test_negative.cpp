// test_negative.cpp — negative / error-handling tests (aes1 namespace)
// Covers: wrong key size, wrong IV length, tampered CT (non-AEAD), wrong key on decrypt
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

static const auto KEY = h("2b7e151628aed2a6abf7158809cf4f3c");   // AES-128
static const auto IV  = h("000102030405060708090a0b0c0d0e0f");   // 16 B

// ── Key validation ────────────────────────────────────────────────────────────

TEST_CASE("Key too short (15 B) -rejected on encrypt", "[negative][key]") {
    std::vector<uint8_t> bad_key(15, 0x00);
    std::vector<uint8_t> pt(16, 0xAA);
    REQUIRE_THROWS(aes1::encrypt(aes1::Mode::CBC, bad_key, IV, pt));
}

TEST_CASE("Key too short (1 B) -rejected on decrypt", "[negative][key]") {
    std::vector<uint8_t> bad_key(1, 0x00);
    std::vector<uint8_t> ct(16, 0xAA);
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::CBC, bad_key, IV, ct));
}

TEST_CASE("XTS key wrong size (16 B, needs 32+) -rejected", "[negative][key][xts]") {
    std::vector<uint8_t> bad_key(16, 0x00);
    auto iv = h("00000000000000000000000000000000");
    std::vector<uint8_t> pt(16, 0xAA);
    REQUIRE_THROWS(aes1::encrypt(aes1::Mode::XTS, bad_key, iv, pt));
}

// ── IV length validation ──────────────────────────────────────────────────────

TEST_CASE("CBC IV 8 bytes (needs 16) -rejected on encrypt", "[negative][iv]") {
    std::vector<uint8_t> bad_iv(8, 0x00);
    std::vector<uint8_t> pt(16, 0xAA);
    REQUIRE_THROWS(aes1::encrypt(aes1::Mode::CBC, KEY, bad_iv, pt));
}

TEST_CASE("CBC IV 8 bytes (needs 16) -rejected on decrypt", "[negative][iv]") {
    std::vector<uint8_t> bad_iv(8, 0x00);
    std::vector<uint8_t> ct(16, 0xAA);
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::CBC, KEY, bad_iv, ct));
}

TEST_CASE("GCM IV 16 bytes (needs 12) -rejected on encrypt", "[negative][iv][gcm]") {
    std::vector<uint8_t> bad_iv(16, 0x00); // GCM needs 12
    std::vector<uint8_t> pt(16, 0xAA);
    REQUIRE_THROWS(aes1::encrypt(aes1::Mode::GCM, KEY, bad_iv, pt));
}

TEST_CASE("GCM IV 8 bytes (needs 12) -rejected on decrypt", "[negative][iv][gcm]") {
    std::vector<uint8_t> bad_iv(8, 0x00);
    std::vector<uint8_t> ct(16, 0xAA), tag(16, 0x00);
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::GCM, KEY, bad_iv, ct, tag));
}

TEST_CASE("CCM IV 16 bytes (needs 13) -rejected", "[negative][iv][ccm]") {
    std::vector<uint8_t> bad_iv(16, 0x00); // CCM needs 13
    std::vector<uint8_t> pt(16, 0xAA);
    REQUIRE_THROWS(aes1::encrypt(aes1::Mode::CCM, KEY, bad_iv, pt));
}

// ── Wrong key on decrypt (non-AEAD) ─────────────────────────────────────────

TEST_CASE("CBC decrypt with wrong key -output differs from original", "[negative][key]") {
    auto key1 = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto key2 = h("2b7e151628aed2a6abf7158809cf4f3d"); // 1 byte diff
    std::vector<uint8_t> pt(16, 0xAB);
    auto enc = aes1::encrypt(aes1::Mode::CBC, key1, IV, pt);
    auto dec = aes1::decrypt(aes1::Mode::CBC, key2, IV, enc.ciphertext);
    REQUIRE(dec != pt); // wrong key -wrong output
}

TEST_CASE("CTR decrypt with wrong key -output differs from original", "[negative][key]") {
    auto key1 = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto key2 = h("ffffffffffffffffffffffffffffffff");
    std::vector<uint8_t> pt = {0x41, 0x42, 0x43, 0x44, 0x45};
    auto enc = aes1::encrypt(aes1::Mode::CTR, key1, IV, pt);
    auto dec = aes1::decrypt(aes1::Mode::CTR, key2, IV, enc.ciphertext);
    REQUIRE(dec != pt);
}

// ── Wrong IV on decrypt (non-AEAD) ───────────────────────────────────────────

TEST_CASE("CBC decrypt with wrong IV -output differs from original", "[negative][iv]") {
    auto iv1 = h("000102030405060708090a0b0c0d0e0f");
    auto iv2 = h("010102030405060708090a0b0c0d0e0f"); // 1 byte diff
    std::vector<uint8_t> pt(16, 0x7A);
    auto enc = aes1::encrypt(aes1::Mode::CBC, KEY, iv1, pt);
    auto dec = aes1::decrypt(aes1::Mode::CBC, KEY, iv2, enc.ciphertext);
    REQUIRE(dec != pt);
}

// ── Tampered ciphertext (non-AEAD) ───────────────────────────────────────────

TEST_CASE("CBC tampered ciphertext -corrupted output, no crash", "[negative][tamper]") {
    std::vector<uint8_t> pt(32, 0x7B);
    auto enc = aes1::encrypt(aes1::Mode::CBC, KEY, IV, pt);
    enc.ciphertext[0] ^= 0xFF;
    REQUIRE_NOTHROW(aes1::decrypt(aes1::Mode::CBC, KEY, IV, enc.ciphertext));
    auto dec = aes1::decrypt(aes1::Mode::CBC, KEY, IV, enc.ciphertext);
    REQUIRE(dec != pt); // output is corrupted, but no exception (non-AEAD)
}

TEST_CASE("CTR tampered ciphertext -corrupted output, no crash", "[negative][tamper]") {
    std::vector<uint8_t> pt(16, 0x7C);
    auto enc = aes1::encrypt(aes1::Mode::CTR, KEY, IV, pt);
    enc.ciphertext[3] ^= 0xAA;
    REQUIRE_NOTHROW(aes1::decrypt(aes1::Mode::CTR, KEY, IV, enc.ciphertext));
    auto dec = aes1::decrypt(aes1::Mode::CTR, KEY, IV, enc.ciphertext);
    REQUIRE(dec != pt);
}

// ── AEAD: wrong key -fail-closed ────────────────────────────────────────────

TEST_CASE("GCM decrypt with wrong key -throws (fail-closed)", "[negative][key][gcm]") {
    auto key1 = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto key2 = h("ffffffffffffffffffffffffffffffff");
    auto iv   = h("000000000000000000000000");
    std::vector<uint8_t> pt(16, 0xAA);
    auto enc = aes1::encrypt(aes1::Mode::GCM, key1, iv, pt);
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::GCM, key2, iv, enc.ciphertext, enc.tag));
}

TEST_CASE("CCM decrypt with wrong key -throws (fail-closed)", "[negative][key][ccm]") {
    auto key1 = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto key2 = h("ffffffffffffffffffffffffffffffff");
    auto iv   = h("00000003020100a0a1a2a3a4a5");
    std::vector<uint8_t> pt(16, 0xBB);
    auto enc = aes1::encrypt(aes1::Mode::CCM, key1, iv, pt);
    REQUIRE_THROWS(aes1::decrypt(aes1::Mode::CCM, key2, iv, enc.ciphertext, enc.tag));
}
