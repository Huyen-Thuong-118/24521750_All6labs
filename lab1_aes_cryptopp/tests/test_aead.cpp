#include <catch2/catch_test_macros.hpp>
#include "aes_service.hpp"
#include "key_manager.hpp"

using namespace lab1;

static std::vector<uint8_t> make_key() { return keygen_aes256(); }
static std::vector<uint8_t> make_aad(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

TEST_CASE("GCM: tag is produced", "[aead][gcm]") {
    auto key = make_key();
    std::vector<uint8_t> pt(32, 0xAB);
    auto res = aes_encrypt(AesMode::GCM, key, pt);
    REQUIRE(res.tag.size() == 16);
}

TEST_CASE("CCM: tag is produced", "[aead][ccm]") {
    auto key = make_key();
    std::vector<uint8_t> pt(32, 0xAB);
    auto res = aes_encrypt(AesMode::CCM, key, pt);
    REQUIRE(res.tag.size() == 16);
}

TEST_CASE("GCM round-trip with AAD", "[aead][gcm]") {
    auto key = make_key();
    auto pt  = std::vector<uint8_t>(48, 0x42);
    auto aad = make_aad("associated-data-header");

    auto res = aes_encrypt(AesMode::GCM, key, pt, {}, aad);
    auto dec = aes_decrypt(AesMode::GCM, key, res.ciphertext, res.iv, res.tag, aad);
    REQUIRE(dec == pt);
}

TEST_CASE("GCM: tampered ciphertext causes auth failure", "[aead][gcm][negative]") {
    auto key = make_key();
    std::vector<uint8_t> pt(32, 0x11);
    auto res = aes_encrypt(AesMode::GCM, key, pt);

    // Flip a bit in ciphertext
    auto bad_ct = res.ciphertext;
    bad_ct[0] ^= 0x01;

    REQUIRE_THROWS(aes_decrypt(AesMode::GCM, key, bad_ct, res.iv, res.tag));
}

TEST_CASE("GCM: tampered tag causes auth failure", "[aead][gcm][negative]") {
    auto key = make_key();
    std::vector<uint8_t> pt(32, 0x22);
    auto res = aes_encrypt(AesMode::GCM, key, pt);

    auto bad_tag = res.tag;
    bad_tag[0] ^= 0xFF;

    REQUIRE_THROWS(aes_decrypt(AesMode::GCM, key, res.ciphertext, res.iv, bad_tag));
}

TEST_CASE("GCM: wrong AAD causes auth failure", "[aead][gcm][negative]") {
    auto key = make_key();
    std::vector<uint8_t> pt(32, 0x33);
    auto aad = make_aad("correct-aad");

    auto res = aes_encrypt(AesMode::GCM, key, pt, {}, aad);

    auto wrong_aad = make_aad("wrong-aad");
    REQUIRE_THROWS(aes_decrypt(AesMode::GCM, key, res.ciphertext, res.iv, res.tag, wrong_aad));
}

TEST_CASE("CCM: tampered ciphertext causes auth failure", "[aead][ccm][negative]") {
    auto key = make_key();
    std::vector<uint8_t> pt(32, 0x44);
    auto res = aes_encrypt(AesMode::CCM, key, pt);

    auto bad_ct = res.ciphertext;
    bad_ct[0] ^= 0x01;

    REQUIRE_THROWS(aes_decrypt(AesMode::CCM, key, bad_ct, res.iv, res.tag));
}
