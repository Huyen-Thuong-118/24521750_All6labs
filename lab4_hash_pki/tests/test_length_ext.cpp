// SHA-256 Length-Extension Attack Tests (Bonus +5)
// Verifies that extend() produces a valid MAC forgery without knowing the key
#include <catch2/catch_test_macros.hpp>
#include "sha256_ext.hpp"
#include "hasher.hpp"
#include "common/codec.hpp"

// Compute SHA256(data) using our hasher
static std::vector<uint8_t> sha256(const std::vector<uint8_t>& data) {
    return hash4::hash_bytes(hash4::Algo::SHA256, data);
}

// Compute naive MAC = SHA256(key || msg)
static std::vector<uint8_t> naive_mac(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& msg)
{
    std::vector<uint8_t> km;
    km.insert(km.end(), key.begin(), key.end());
    km.insert(km.end(), msg.begin(), msg.end());
    return sha256(km);
}

TEST_CASE("sha256_padding: output size is multiple of 64", "[sha256ext]") {
    for (size_t len : {0, 1, 55, 56, 63, 64, 65, 127, 128}) {
        auto pad = sha256_ext::sha256_padding(len);
        REQUIRE((len + pad.size()) % 64 == 0);
    }
}

TEST_CASE("sha256_padding: starts with 0x80", "[sha256ext]") {
    auto pad = sha256_ext::sha256_padding(10);
    REQUIRE(pad[0] == 0x80);
}

TEST_CASE("sha256_padding: last 8 bytes encode bit-length big-endian", "[sha256ext]") {
    size_t len = 10;
    auto pad = sha256_ext::sha256_padding(len);
    uint64_t expected_bits = (uint64_t)len * 8;
    uint64_t got = 0;
    size_t n = pad.size();
    for (int i = 0; i < 8; i++)
        got = (got << 8) | pad[n - 8 + i];
    REQUIRE(got == expected_bits);
}

TEST_CASE("Length-extension: basic attack", "[sha256ext][attack]") {
    // Secret key (unknown to attacker)
    std::vector<uint8_t> key = {0xDE, 0xAD, 0xBE, 0xEF,
                                 0xCA, 0xFE, 0xBA, 0xBE,
                                 0x00, 0x11, 0x22, 0x33,
                                 0x44, 0x55, 0x66, 0x77};

    // Original message
    std::vector<uint8_t> msg = {'h', 'e', 'l', 'l', 'o'};

    // Attacker knows: H = SHA256(key||msg), len(key), msg
    auto H = naive_mac(key, msg);

    // Extension to append
    std::vector<uint8_t> ext = {'&', 'a', 'd', 'm', 'i', 'n', '=', '1'};

    // Perform the attack
    auto res = sha256_ext::extend(H, key.size(), msg, ext);

    // Verify: SHA256(key || forged_message) == forged_hash
    REQUIRE(sha256_ext::verify_attack(key, res.forged_message, res.forged_hash));
}

TEST_CASE("Length-extension: forged_message contains original msg", "[sha256ext]") {
    std::vector<uint8_t> key = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    std::vector<uint8_t> msg = {'d', 'a', 't', 'a'};
    std::vector<uint8_t> ext = {'e', 'x', 't'};

    auto H = naive_mac(key, msg);
    auto res = sha256_ext::extend(H, key.size(), msg, ext);

    // forged_message starts with original msg
    REQUIRE(res.forged_message.size() >= msg.size());
    std::vector<uint8_t> prefix(res.forged_message.begin(),
                                  res.forged_message.begin() + msg.size());
    REQUIRE(prefix == msg);

    // forged_message ends with extension
    std::vector<uint8_t> suffix(res.forged_message.end() - ext.size(),
                                  res.forged_message.end());
    REQUIRE(suffix == ext);
}

TEST_CASE("Length-extension: forged_hash is 32 bytes", "[sha256ext]") {
    std::vector<uint8_t> key(16, 0xAA);
    std::vector<uint8_t> msg = {'m', 's', 'g'};
    std::vector<uint8_t> ext = {'e', 'x', 't'};

    auto H = naive_mac(key, msg);
    auto res = sha256_ext::extend(H, key.size(), msg, ext);
    REQUIRE(res.forged_hash.size() == 32);
}

TEST_CASE("Length-extension: different keys produce different forged hashes", "[sha256ext]") {
    std::vector<uint8_t> msg = {'m'};
    std::vector<uint8_t> ext = {'e'};
    std::vector<uint8_t> key1(8, 0x11);
    std::vector<uint8_t> key2(8, 0x22);

    auto H1 = naive_mac(key1, msg);
    auto H2 = naive_mac(key2, msg);

    auto res1 = sha256_ext::extend(H1, key1.size(), msg, ext);
    auto res2 = sha256_ext::extend(H2, key2.size(), msg, ext);

    REQUIRE(res1.forged_hash != res2.forged_hash);
}

TEST_CASE("verify_attack: wrong key returns false", "[sha256ext]") {
    std::vector<uint8_t> key  = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> wrong_key = {0xFF, 0xFF, 0xFF, 0xFF};
    std::vector<uint8_t> msg  = {'x'};
    std::vector<uint8_t> ext  = {'y'};

    auto H = naive_mac(key, msg);
    auto res = sha256_ext::extend(H, key.size(), msg, ext);

    // verify with correct key must pass
    REQUIRE(sha256_ext::verify_attack(key, res.forged_message, res.forged_hash));
    // verify with wrong key must fail
    REQUIRE_FALSE(sha256_ext::verify_attack(wrong_key, res.forged_message, res.forged_hash));
}

TEST_CASE("Length-extension: attack works for different key lengths", "[sha256ext]") {
    std::vector<uint8_t> msg = {'a', 'b', 'c'};
    std::vector<uint8_t> ext = {'x', 'y', 'z'};

    for (size_t klen : {1, 7, 16, 32, 55, 64}) {
        std::vector<uint8_t> key(klen, 0x42);
        auto H = naive_mac(key, msg);
        auto res = sha256_ext::extend(H, klen, msg, ext);
        INFO("key_len = " << klen);
        REQUIRE(sha256_ext::verify_attack(key, res.forged_message, res.forged_hash));
    }
}
