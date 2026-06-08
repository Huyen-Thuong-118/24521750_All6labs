#include <catch2/catch_test_macros.hpp>
#include "aes_service.hpp"
#include "key_manager.hpp"
#include "nonce_manager.hpp"

using namespace lab1;

static std::vector<uint8_t> make_key() { return keygen_aes256(); }

static std::vector<uint8_t> make_pt(std::size_t n = 64) {
    std::vector<uint8_t> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(i & 0xFF);
    return v;
}

// Round-trip helper: encrypt then decrypt, result must equal original
static void check_roundtrip(AesMode mode) {
    auto key = make_key();
    auto pt  = make_pt(64);
    auto res = aes_encrypt(mode, key, pt);
    auto dec = aes_decrypt(mode, key, res.ciphertext, res.iv, res.tag);
    REQUIRE(dec == pt);
}

TEST_CASE("CBC round-trip", "[modes][cbc]") { check_roundtrip(AesMode::CBC); }
TEST_CASE("CFB round-trip", "[modes][cfb]") { check_roundtrip(AesMode::CFB); }
TEST_CASE("OFB round-trip", "[modes][ofb]") { check_roundtrip(AesMode::OFB); }
TEST_CASE("CTR round-trip", "[modes][ctr]") { check_roundtrip(AesMode::CTR); }
TEST_CASE("GCM round-trip", "[modes][gcm]") { check_roundtrip(AesMode::GCM); }
TEST_CASE("CCM round-trip", "[modes][ccm]") { check_roundtrip(AesMode::CCM); }

TEST_CASE("ECB round-trip (small payload)", "[modes][ecb]") {
    auto key = make_key();
    auto pt  = make_pt(32); // 2 blocks exactly
    auto res = aes_encrypt(AesMode::ECB, key, pt);
    auto dec = aes_decrypt(AesMode::ECB, key, res.ciphertext, {});
    REQUIRE(dec == pt);
}

TEST_CASE("Different plaintexts produce different ciphertexts (CBC)", "[modes]") {
    auto key = make_key();
    auto iv  = generate_iv("cbc");
    auto pt1 = make_pt(32);
    auto pt2 = make_pt(32);
    pt2[0] ^= 0xFF;
    auto ct1 = aes_encrypt(AesMode::CBC, key, pt1, iv).ciphertext;
    auto ct2 = aes_encrypt(AesMode::CBC, key, pt2, iv).ciphertext;
    REQUIRE(ct1 != ct2);
}

TEST_CASE("IV is persisted in result", "[modes]") {
    auto key = make_key();
    auto pt  = make_pt(16);
    auto res = aes_encrypt(AesMode::CBC, key, pt);
    REQUIRE(res.iv.size() == 16);

    // Decrypt with the returned IV succeeds
    auto dec = aes_decrypt(AesMode::CBC, key, res.ciphertext, res.iv);
    REQUIRE(dec == pt);
}

TEST_CASE("validate_iv rejects wrong length", "[modes][nonce]") {
    std::vector<uint8_t> bad_iv(8, 0x00); // CBC needs 16 bytes
    REQUIRE_THROWS_AS(validate_iv("cbc", bad_iv), std::runtime_error);

    std::vector<uint8_t> bad_gcm_iv(16, 0x00); // GCM needs 12 bytes
    REQUIRE_THROWS_AS(validate_iv("gcm", bad_gcm_iv), std::runtime_error);
}
