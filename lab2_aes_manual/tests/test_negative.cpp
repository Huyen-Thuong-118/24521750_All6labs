// test_negative.cpp — Negative/security property tests for AES-128-CTR
// Tests: wrong key decryption, bit-flip propagation, CTR keystream reuse demo
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

// ── Wrong key: decryption produces garbage, not original plaintext ─────────────
TEST_CASE("CTR wrong key: decrypt yields different plaintext", "[negative][key]") {
    auto key_enc = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto key_dec = h("deadbeefcafebabedeadbeefcafebabe");  // different key
    auto iv      = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    auto pt      = h("6bc1bee22e409f96e93d7e117393172a");

    aes2::AesCore aes_e(key_enc.data()), aes_d(key_dec.data());
    aes2::CtrMode ctr_e(aes_e), ctr_d(aes_d);

    std::vector<uint8_t> ct(16), dec(16);
    ctr_e.process(pt.data(), ct.data(), 16, iv.data());
    ctr_d.process(ct.data(), dec.data(), 16, iv.data());

    REQUIRE(dec != pt);  // wrong key → wrong plaintext
}

// ── Wrong IV: decrypt with different IV also yields garbage ───────────────────
TEST_CASE("CTR wrong IV: decrypt yields different plaintext", "[negative][iv]") {
    auto key  = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv1  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    auto iv2  = h("aabbccddeeff00112233445566778899");
    auto pt   = h("6bc1bee22e409f96e93d7e117393172a");

    aes2::AesCore aes(key.data());
    aes2::CtrMode ctr(aes);

    std::vector<uint8_t> ct(16), dec(16);
    ctr.process(pt.data(), ct.data(), 16, iv1.data());
    ctr.process(ct.data(), dec.data(), 16, iv2.data());  // wrong IV

    REQUIRE(dec != pt);
}

// ── CTR has NO integrity: 1-bit flip in ciphertext flips exactly 1 bit in PT ──
// This demonstrates CTR's lack of authentication/integrity protection.
// The test is intentionally checking this "broken" property — not a bug.
TEST_CASE("CTR no integrity: 1-bit flip in CT flips exactly 1 bit in decrypted PT", "[negative][integrity]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");

    aes2::AesCore aes(key.data());
    aes2::CtrMode ctr(aes);

    std::vector<uint8_t> ct(16), dec(16);
    ctr.process(pt.data(), ct.data(), 16, iv.data());

    // Flip bit 0 of byte 0 in ciphertext
    std::vector<uint8_t> tampered_ct = ct;
    tampered_ct[0] ^= 0x01;

    ctr.process(tampered_ct.data(), dec.data(), 16, iv.data());

    // Only byte 0 should differ, and by exactly the XOR mask
    REQUIRE(dec[0] == (pt[0] ^ 0x01));  // bit flip propagates 1:1
    for (size_t i = 1; i < 16; ++i)
        REQUIRE(dec[i] == pt[i]);        // all other bytes intact

    // CTR mode does NOT detect tampering — no error, no exception
    // This is the intended behavior that makes CTR malleable
}

TEST_CASE("CTR no integrity: multi-byte tamper flips only those bytes", "[negative][integrity]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    std::vector<uint8_t> pt(32, 0x00);

    aes2::AesCore aes(key.data());
    aes2::CtrMode ctr(aes);

    std::vector<uint8_t> ct(32), dec(32);
    ctr.process(pt.data(), ct.data(), 32, iv.data());

    // Tamper bytes 5, 10, 15
    ct[5]  ^= 0xFF;
    ct[10] ^= 0xAA;
    ct[15] ^= 0x55;

    ctr.process(ct.data(), dec.data(), 32, iv.data());

    // Tampered bytes have predictable bit-flip
    REQUIRE(dec[5]  == (pt[5]  ^ 0xFF));
    REQUIRE(dec[10] == (pt[10] ^ 0xAA));
    REQUIRE(dec[15] == (pt[15] ^ 0x55));

    // All other bytes unchanged
    for (size_t i = 0; i < 32; ++i) {
        if (i == 5 || i == 10 || i == 15) continue;
        REQUIRE(dec[i] == pt[i]);
    }
}

// ── CTR keystream reuse: two-time pad attack ──────────────────────────────────
// Encrypting two different plaintexts with the same key+IV reveals XOR of plaintexts.
TEST_CASE("CTR keystream reuse: XOR of CTs reveals XOR of PTs", "[negative][reuse]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");  // SAME iv!

    std::vector<uint8_t> pt1 = h("4141414141414141414141414141414141414141414141414141414141414141"); // 32xA
    std::vector<uint8_t> pt2 = h("4242424242424242424242424242424242424242424242424242424242424242"); // 32xB

    aes2::AesCore aes(key.data());
    aes2::CtrMode ctr(aes);

    std::vector<uint8_t> ct1(32), ct2(32);
    ctr.process(pt1.data(), ct1.data(), 32, iv.data());
    ctr.process(pt2.data(), ct2.data(), 32, iv.data());  // SAME key+IV

    // XOR of ciphertexts = XOR of plaintexts (keystream cancels out)
    for (size_t i = 0; i < 32; ++i)
        REQUIRE((uint8_t)(ct1[i] ^ ct2[i]) == (uint8_t)(pt1[i] ^ pt2[i]));
}

// ── Determinism: same key+IV+PT always gives same CT ─────────────────────────
TEST_CASE("CTR determinism: same inputs always produce same output", "[negative]") {
    auto key = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto iv  = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    auto pt  = h("6bc1bee22e409f96e93d7e117393172a");

    aes2::AesCore aes(key.data());
    aes2::CtrMode ctr(aes);

    std::vector<uint8_t> ct1(16), ct2(16);
    ctr.process(pt.data(), ct1.data(), 16, iv.data());
    ctr.process(pt.data(), ct2.data(), 16, iv.data());

    REQUIRE(ct1 == ct2);
}

// ── Different keys produce different ciphertexts (sanity) ─────────────────────
TEST_CASE("CTR different keys: same PT+IV gives different CT", "[negative]") {
    auto key1 = h("2b7e151628aed2a6abf7158809cf4f3c");
    auto key2 = h("000102030405060708090a0b0c0d0e0f");
    auto iv   = h("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    auto pt   = h("6bc1bee22e409f96e93d7e117393172a");

    aes2::AesCore aes1(key1.data()), aes2(key2.data());
    aes2::CtrMode ctr1(aes1), ctr2(aes2);

    std::vector<uint8_t> ct1(16), ct2(16);
    ctr1.process(pt.data(), ct1.data(), 16, iv.data());
    ctr2.process(pt.data(), ct2.data(), 16, iv.data());

    REQUIRE(ct1 != ct2);
}
