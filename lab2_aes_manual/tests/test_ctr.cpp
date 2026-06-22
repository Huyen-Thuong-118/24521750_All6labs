// test_ctr.cpp — NIST SP 800-38A CTR-AES128 Known-Answer Tests
// Vectors from NIST SP 800-38A Appendix F.5.1
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <cstring>
#include "aes_core.hpp"
#include "ctr_mode.hpp"

static std::vector<uint8_t> h(const char* hex) {
    std::string s(hex);
    std::vector<uint8_t> r(s.size()/2);
    for (size_t i=0;i<r.size();i++) r[i]=(uint8_t)std::stoul(s.substr(2*i,2),nullptr,16);
    return r;
}

static const auto KEY = h("2b7e151628aed2a6abf7158809cf4f3c");
static const auto IV  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");

// ── NIST SP 800-38A F.5.1: CTR-AES128, single blocks ─────────────────────────

TEST_CASE("CTR F.5.1 Block 1", "[ctr][kat]") {
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");
    auto exp = h("874d6191b620e3261bef6864990db6ce");
    aes2::AesCore aes(KEY.data());
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> out(16);
    ctr.process(pt.data(), out.data(), 16, IV.data());
    REQUIRE(out == exp);
}

TEST_CASE("CTR F.5.1 Block 2 (counter incremented once)", "[ctr][kat]") {
    // Counter = IV+1 = f0f1...ff00
    auto iv2 = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdff00");
    auto pt  = h("ae2d8a571e03ac9c9eb76fac45af8e51");
    auto exp = h("9806f66b7970fdff8617187bb9fffdff");
    aes2::AesCore aes(KEY.data());
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> out(16);
    ctr.process(pt.data(), out.data(), 16, iv2.data());
    REQUIRE(out == exp);
}

TEST_CASE("CTR F.5.1 Block 3", "[ctr][kat]") {
    auto iv3 = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdff01");
    auto pt  = h("30c81c46a35ce411e5fbc1191a0a52ef");
    auto exp = h("5ae4df3edbd5d35e5b4f09020db03eab");
    aes2::AesCore aes(KEY.data());
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> out(16);
    ctr.process(pt.data(), out.data(), 16, iv3.data());
    REQUIRE(out == exp);
}

TEST_CASE("CTR F.5.1 Block 4", "[ctr][kat]") {
    auto iv4 = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdff02");
    auto pt  = h("f69f2445df4f9b17ad2b417be66c3710");
    auto exp = h("1e031dda2fbe03d1792170a0f3009cee");
    aes2::AesCore aes(KEY.data());
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> out(16);
    ctr.process(pt.data(), out.data(), 16, iv4.data());
    REQUIRE(out == exp);
}

TEST_CASE("CTR F.5.1 4-block concatenated", "[ctr][kat]") {
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a"
                 "ae2d8a571e03ac9c9eb76fac45af8e51"
                 "30c81c46a35ce411e5fbc1191a0a52ef"
                 "f69f2445df4f9b17ad2b417be66c3710");
    auto exp = h("874d6191b620e3261bef6864990db6ce"
                 "9806f66b7970fdff8617187bb9fffdff"
                 "5ae4df3edbd5d35e5b4f09020db03eab"
                 "1e031dda2fbe03d1792170a0f3009cee");
    aes2::AesCore aes(KEY.data());
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> out(64);
    ctr.process(pt.data(), out.data(), 64, IV.data());
    REQUIRE(out == exp);
}

// ── Partial block (5 bytes) ───────────────────────────────────────────────────
TEST_CASE("CTR partial block (5 bytes)", "[ctr]") {
    auto pt  = h("6bc1bee22e");
    auto exp = h("874d6191b6");  // first 5 bytes of keystream XOR first 5 bytes of pt
    aes2::AesCore aes(KEY.data());
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> out(5);
    ctr.process(pt.data(), out.data(), 5, IV.data());
    REQUIRE(out == exp);
}

// ── Round-trip: encrypt then decrypt = identity ───────────────────────────────
TEST_CASE("CTR round-trip (32 bytes)", "[ctr]") {
    std::vector<uint8_t> pt(32, 0xAB);
    aes2::AesCore aes(KEY.data());
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> ct(32), dec(32);
    ctr.process(pt.data(), ct.data(), 32, IV.data());
    ctr.process(ct.data(), dec.data(), 32, IV.data()); // decrypt = same operation
    REQUIRE(dec == pt);
}

TEST_CASE("CTR round-trip arbitrary length (100 bytes)", "[ctr]") {
    std::vector<uint8_t> pt(100);
    for (size_t i=0;i<100;i++) pt[i]=(uint8_t)i;
    aes2::AesCore aes(KEY.data());
    aes2::CtrMode ctr(aes);
    std::vector<uint8_t> ct(100), dec(100);
    ctr.process(pt.data(), ct.data(), 100, IV.data());
    ctr.process(ct.data(), dec.data(), 100, IV.data());
    REQUIRE(dec == pt);
}

// ── Counter increment ─────────────────────────────────────────────────────────
TEST_CASE("CTR increment: ff...ff wraps to 00...00", "[ctr][increment]") {
    uint8_t ctr[16];
    memset(ctr, 0xFF, 16);
    aes2::CtrMode::increment(ctr);
    uint8_t exp[16] = {};
    REQUIRE(memcmp(ctr, exp, 16) == 0);
}

TEST_CASE("CTR increment: last byte rolls over to second-last", "[ctr][increment]") {
    uint8_t ctr[16] = {};
    ctr[15] = 0xFF;
    aes2::CtrMode::increment(ctr);
    REQUIRE(ctr[14] == 0x01);
    REQUIRE(ctr[15] == 0x00);
}

TEST_CASE("CTR increment simple", "[ctr][increment]") {
    uint8_t ctr[16] = {};
    ctr[15] = 0x05;
    aes2::CtrMode::increment(ctr);
    REQUIRE(ctr[15] == 0x06);
    REQUIRE(ctr[14] == 0x00);
}
