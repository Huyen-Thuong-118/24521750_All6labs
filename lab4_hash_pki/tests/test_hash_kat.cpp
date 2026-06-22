// NIST FIPS 180-4 / FIPS 202 Known-Answer Tests for SHA-2, SHA-3, SHAKE
#include <catch2/catch_test_macros.hpp>
#include "hasher.hpp"
#include "common/codec.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

// Helper: hex string → bytes → hash → hex string
static std::string digest_hex(const std::string& algo,
                               const std::string& input_hex,
                               size_t outlen = 0)
{
    std::vector<uint8_t> data;
    if (!input_hex.empty()) data = common::hex_decode(input_hex);
    auto a = hash4::parse_algo(algo);
    auto h = hash4::hash_bytes(a, data, outlen);
    return common::hex_encode(h);
}

// ─── SHA-2 ──────────────────────────────────────────────────────────────────
TEST_CASE("KAT SHA-224 empty", "[sha2][sha224]") {
    REQUIRE(digest_hex("sha224", "") ==
        "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f");
}
TEST_CASE("KAT SHA-224 'abc'", "[sha2][sha224]") {
    REQUIRE(digest_hex("sha224", "616263") ==
        "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7");
}

TEST_CASE("KAT SHA-256 empty", "[sha2][sha256]") {
    REQUIRE(digest_hex("sha256", "") ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}
TEST_CASE("KAT SHA-256 'abc'", "[sha2][sha256]") {
    REQUIRE(digest_hex("sha256", "616263") ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
TEST_CASE("KAT SHA-256 448-bit message", "[sha2][sha256]") {
    // "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
    REQUIRE(digest_hex("sha256",
        "6162636462636465636465666465666765666768666768696768696a68696a6b"
        "696a6b6c6a6b6c6d6b6c6d6e6c6d6e6f6d6e6f706e6f7071") ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("KAT SHA-384 empty", "[sha2][sha384]") {
    REQUIRE(digest_hex("sha384", "") ==
        "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b");
}
TEST_CASE("KAT SHA-384 'abc'", "[sha2][sha384]") {
    REQUIRE(digest_hex("sha384", "616263") ==
        "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7");
}

TEST_CASE("KAT SHA-512 empty", "[sha2][sha512]") {
    REQUIRE(digest_hex("sha512", "") ==
        "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
}
TEST_CASE("KAT SHA-512 'abc'", "[sha2][sha512]") {
    REQUIRE(digest_hex("sha512", "616263") ==
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
}

// ─── SHA-3 ──────────────────────────────────────────────────────────────────
TEST_CASE("KAT SHA3-224 empty", "[sha3][sha3-224]") {
    REQUIRE(digest_hex("sha3-224", "") ==
        "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7");
}
TEST_CASE("KAT SHA3-224 'abc'", "[sha3][sha3-224]") {
    REQUIRE(digest_hex("sha3-224", "616263") ==
        "e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf");
}

TEST_CASE("KAT SHA3-256 empty", "[sha3][sha3-256]") {
    REQUIRE(digest_hex("sha3-256", "") ==
        "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
}
TEST_CASE("KAT SHA3-256 'abc'", "[sha3][sha3-256]") {
    REQUIRE(digest_hex("sha3-256", "616263") ==
        "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
}

TEST_CASE("KAT SHA3-384 empty", "[sha3][sha3-384]") {
    REQUIRE(digest_hex("sha3-384", "") ==
        "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004");
}
TEST_CASE("KAT SHA3-384 'abc'", "[sha3][sha3-384]") {
    REQUIRE(digest_hex("sha3-384", "616263") ==
        "ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25");
}

TEST_CASE("KAT SHA3-512 empty", "[sha3][sha3-512]") {
    REQUIRE(digest_hex("sha3-512", "") ==
        "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26");
}
TEST_CASE("KAT SHA3-512 'abc'", "[sha3][sha3-512]") {
    REQUIRE(digest_hex("sha3-512", "616263") ==
        "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0");
}

// ─── SHAKE ──────────────────────────────────────────────────────────────────
TEST_CASE("KAT SHAKE128 empty 16B", "[shake][shake128]") {
    REQUIRE(digest_hex("shake128", "", 16) ==
        "7f9c2ba4e88f827d616045507605853e");
}
TEST_CASE("KAT SHAKE128 empty 32B", "[shake][shake128]") {
    REQUIRE(digest_hex("shake128", "", 32) ==
        "7f9c2ba4e88f827d616045507605853ed73b8093f6efbc88eb1a6eacfa66ef26");
}
TEST_CASE("KAT SHAKE128 'abc' 32B", "[shake][shake128]") {
    REQUIRE(digest_hex("shake128", "616263", 32) ==
        "5881092dd818bf5cf8a3ddb793fbcba74097d5c526a6d35f97b83351940f2cc8");
}

TEST_CASE("KAT SHAKE256 empty 32B", "[shake][shake256]") {
    REQUIRE(digest_hex("shake256", "", 32) ==
        "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f");
}
TEST_CASE("KAT SHAKE256 empty 64B", "[shake][shake256]") {
    REQUIRE(digest_hex("shake256", "", 64) ==
        "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f"
        "d75dc4ddd8c0f200cb05019d67b592f6fc821c49479ab48640292eacb3b7c4be");
}
TEST_CASE("KAT SHAKE256 'abc' 32B", "[shake][shake256]") {
    REQUIRE(digest_hex("shake256", "616263", 32) ==
        "483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739");
}

// ─── Output-length sanity ───────────────────────────────────────────────────
TEST_CASE("SHA-256 digest is exactly 32 bytes", "[sha2]") {
    auto h = hash4::hash_bytes(hash4::Algo::SHA256, {});
    REQUIRE(h.size() == 32);
}
TEST_CASE("SHA3-512 digest is exactly 64 bytes", "[sha3]") {
    auto h = hash4::hash_bytes(hash4::Algo::SHA3_512, {});
    REQUIRE(h.size() == 64);
}
TEST_CASE("SHAKE256 custom outlen 7 bytes", "[shake]") {
    auto h = hash4::hash_bytes(hash4::Algo::SHAKE256, {}, 7);
    REQUIRE(h.size() == 7);
}

// ─── Streaming == one-shot ───────────────────────────────────────────────────
TEST_CASE("hash_file SHA-256 equals hash_bytes", "[streaming]") {
    // Write a small temp file and verify hash_file matches hash_bytes
    const char* path = "tmp_test_hash.bin";
    {
        std::ofstream f(path, std::ios::binary);
        for (int i = 0; i < 256; i++) f.put(static_cast<char>(i));
    }
    std::vector<uint8_t> data(256);
    for (int i = 0; i < 256; i++) data[i] = static_cast<uint8_t>(i);

    auto expected = hash4::hash_bytes(hash4::Algo::SHA256, data);
    auto got      = hash4::hash_file(hash4::Algo::SHA256, path);
    REQUIRE(got == expected);
    std::remove(path);
}

// ─── JSON-driven KAT (all three vector files) ───────────────────────────────
static void run_json_kat(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        WARN("KAT file not found (skipped): " + path);
        return;
    }
    json j; f >> j;
    for (auto& t : j["tests"]) {
        std::string name = t.value("name", "?");
        auto algo = hash4::parse_algo(t.value("algo", ""));
        std::string inp = t.value("input", "");
        size_t ol = t.value("outlen", (size_t)0);
        std::vector<uint8_t> data;
        if (!inp.empty()) data = common::hex_decode(inp);
        auto got = common::hex_encode(hash4::hash_bytes(algo, data, ol));
        INFO("Test: " << name);
        REQUIRE(got == t.value("expected", ""));
    }
}

TEST_CASE("JSON KAT sha2.json", "[kat][json]") {
    run_json_kat(LAB4_KAT_DIR "/sha2.json");
}
TEST_CASE("JSON KAT sha3.json", "[kat][json]") {
    run_json_kat(LAB4_KAT_DIR "/sha3.json");
}
TEST_CASE("JSON KAT shake.json", "[kat][json]") {
    run_json_kat(LAB4_KAT_DIR "/shake.json");
}
