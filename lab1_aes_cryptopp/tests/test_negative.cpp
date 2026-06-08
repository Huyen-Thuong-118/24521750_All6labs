#include <catch2/catch_test_macros.hpp>
#include "aes_service.hpp"
#include "key_manager.hpp"
#include "nonce_manager.hpp"

using namespace lab1;

static std::vector<uint8_t> make_key() { return keygen_aes256(); }

// ── Key errors ───────────────────────────────────────────────────────────────

TEST_CASE("Wrong key size is rejected", "[negative][key]") {
    std::vector<uint8_t> bad_key(16, 0x00); // AES-128 key — service requires AES-256
    std::vector<uint8_t> pt(32, 0xAB);
    REQUIRE_THROWS_AS(aes_encrypt(AesMode::CBC, bad_key, pt), std::runtime_error);
}

// ── IV errors ────────────────────────────────────────────────────────────────

TEST_CASE("Wrong IV length is rejected on encrypt", "[negative][iv]") {
    auto key = make_key();
    std::vector<uint8_t> pt(32, 0xAB);
    std::vector<uint8_t> bad_iv(8, 0x00); // CBC needs 16 bytes
    REQUIRE_THROWS_AS(aes_encrypt(AesMode::CBC, key, pt, bad_iv), std::runtime_error);
}

TEST_CASE("Wrong IV length is rejected on decrypt", "[negative][iv]") {
    auto key = make_key();
    std::vector<uint8_t> ct(32, 0xAB);
    std::vector<uint8_t> bad_iv(8, 0x00);
    REQUIRE_THROWS_AS(aes_decrypt(AesMode::CBC, key, ct, bad_iv), std::runtime_error);
}

TEST_CASE("GCM: wrong IV length (16 bytes instead of 12) rejected", "[negative][iv][gcm]") {
    auto key = make_key();
    std::vector<uint8_t> pt(16, 0xAB);
    std::vector<uint8_t> bad_iv(16, 0x00); // GCM needs 12
    REQUIRE_THROWS_AS(aes_encrypt(AesMode::GCM, key, pt, bad_iv), std::runtime_error);
}

// ── Wrong key on decrypt (non-AEAD) ─────────────────────────────────────────

TEST_CASE("CBC decrypt with wrong key: bad PKCS7 padding throws", "[negative][key]") {
    auto key1 = make_key();
    auto key2 = make_key();
    std::vector<uint8_t> pt(32, 0xAB);

    auto res = aes_encrypt(AesMode::CBC, key1, pt);

    // Wrong key → decrypted last block is random → PKCS7 invalid → Crypto++ throws
    // This is the correct secure behavior (fail-closed)
    REQUIRE_THROWS(aes_decrypt(AesMode::CBC, key2, res.ciphertext, res.iv));
}

// ── AEAD: wrong key → auth failure ──────────────────────────────────────────

TEST_CASE("GCM decrypt with wrong key throws (fail-closed)", "[negative][key][gcm]") {
    auto key1 = make_key();
    auto key2 = make_key();
    std::vector<uint8_t> pt(32, 0xAB);

    auto res = aes_encrypt(AesMode::GCM, key1, pt);
    REQUIRE_THROWS(aes_decrypt(AesMode::GCM, key2, res.ciphertext, res.iv, res.tag));
}

// ── Nonce-reuse detection ────────────────────────────────────────────────────

TEST_CASE("Nonce-reuse detection throws on duplicate (key, nonce)", "[negative][nonce]") {
    std::string log = "test_nonce_log_tmp.json";
    // Remove any leftover from previous run
    std::remove(log.c_str());

    std::string key_hex   = "aabbccdd";
    std::string nonce_hex = "001122";

    // First use: OK
    REQUIRE_NOTHROW(check_and_record_nonce(key_hex, nonce_hex, log));

    // Second use: must throw
    REQUIRE_THROWS_AS(check_and_record_nonce(key_hex, nonce_hex, log), std::runtime_error);

    std::remove(log.c_str());
}

TEST_CASE("Different nonce with same key is allowed", "[negative][nonce]") {
    std::string log = "test_nonce_log_tmp2.json";
    std::remove(log.c_str());

    REQUIRE_NOTHROW(check_and_record_nonce("key1", "nonce1", log));
    REQUIRE_NOTHROW(check_and_record_nonce("key1", "nonce2", log)); // different nonce: OK

    std::remove(log.c_str());
}
