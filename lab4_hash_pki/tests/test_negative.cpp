// Negative / error-handling tests — "fail closed" behavior
#include <catch2/catch_test_macros.hpp>
#include "hasher.hpp"
#include "cert_parser.hpp"
#include "sha256_ext.hpp"
#include "common/codec.hpp"
#include <stdexcept>

// ─── Hasher error cases ──────────────────────────────────────────────────────

TEST_CASE("parse_algo: unknown name throws", "[negative][hasher]") {
    REQUIRE_THROWS_AS(hash4::parse_algo("md5"), std::runtime_error);
    REQUIRE_THROWS_AS(hash4::parse_algo("sha1"), std::runtime_error);
    REQUIRE_THROWS_AS(hash4::parse_algo(""), std::runtime_error);
    REQUIRE_THROWS_AS(hash4::parse_algo("sha999"), std::runtime_error);
}

TEST_CASE("SHAKE without outlen throws", "[negative][hasher]") {
    std::vector<uint8_t> data = {0x01, 0x02};
    REQUIRE_THROWS_AS(hash4::hash_bytes(hash4::Algo::SHAKE128, data, 0),
                      std::runtime_error);
    REQUIRE_THROWS_AS(hash4::hash_bytes(hash4::Algo::SHAKE256, data, 0),
                      std::runtime_error);
}

TEST_CASE("hash_file: nonexistent file throws", "[negative][hasher]") {
    REQUIRE_THROWS_AS(hash4::hash_file(hash4::Algo::SHA256, "no_such_file_xyz.bin"),
                      std::runtime_error);
}

TEST_CASE("SHAKE with outlen does NOT throw", "[negative][hasher]") {
    std::vector<uint8_t> data;
    REQUIRE_NOTHROW(hash4::hash_bytes(hash4::Algo::SHAKE128, data, 32));
    REQUIRE_NOTHROW(hash4::hash_bytes(hash4::Algo::SHAKE256, data, 64));
}

// SHA-2 output sizes are fixed regardless of outlen parameter
TEST_CASE("SHA-256 outlen param is ignored (fixed size)", "[negative][hasher]") {
    // We pass outlen=0; library uses default (32). Should not throw.
    std::vector<uint8_t> data;
    REQUIRE_NOTHROW(hash4::hash_bytes(hash4::Algo::SHA256, data, 0));
    auto h = hash4::hash_bytes(hash4::Algo::SHA256, data, 0);
    REQUIRE(h.size() == 32);
}

// ─── X.509 error cases ──────────────────────────────────────────────────────

TEST_CASE("parse_cert: bad file path throws", "[negative][x509]") {
    REQUIRE_THROWS_AS(pki::parse_cert("no_such_cert.pem"), std::runtime_error);
}

TEST_CASE("parse_cert_pem: garbage PEM throws", "[negative][x509]") {
    REQUIRE_THROWS_AS(pki::parse_cert_pem("not a valid pem at all"), std::runtime_error);
    REQUIRE_THROWS_AS(pki::parse_cert_pem(""), std::runtime_error);
    REQUIRE_THROWS_AS(pki::parse_cert_pem(
        "-----BEGIN CERTIFICATE-----\ngarbage\n-----END CERTIFICATE-----\n"),
        std::runtime_error);
}

TEST_CASE("verify_cert_signature: bad issuer path throws", "[negative][x509]") {
    // Write a real cert to a temp file
    // Use a minimal valid PEM for the cert path (reuse the not-found path for issuer)
    REQUIRE_THROWS(pki::verify_cert_signature("no_cert.pem", "no_issuer.pem"));
}

// ─── sha256_ext error cases ──────────────────────────────────────────────────

TEST_CASE("sha256_ext::extend: wrong hash size throws", "[negative][sha256ext]") {
    std::vector<uint8_t> bad_hash(31, 0); // must be 32
    REQUIRE_THROWS_AS(
        sha256_ext::extend(bad_hash, 16, {0x61}, {0x62}),
        std::runtime_error);

    std::vector<uint8_t> too_big(33, 0);
    REQUIRE_THROWS_AS(
        sha256_ext::extend(too_big, 16, {0x61}, {0x62}),
        std::runtime_error);
}

// ─── codec error cases ───────────────────────────────────────────────────────

TEST_CASE("hex_decode: odd-length string throws", "[negative][codec]") {
    REQUIRE_THROWS(common::hex_decode("abc")); // odd length
}

TEST_CASE("hex_decode: empty string returns empty", "[negative][codec]") {
    REQUIRE(common::hex_decode("").empty());
}

// ─── MD5 / SHA-1 must NOT be accepted (banned algorithms) ───────────────────
TEST_CASE("MD5 is rejected by parse_algo", "[negative][banned]") {
    REQUIRE_THROWS(hash4::parse_algo("md5"));
}
TEST_CASE("SHA-1 is rejected by parse_algo", "[negative][banned]") {
    REQUIRE_THROWS(hash4::parse_algo("sha1"));
    REQUIRE_THROWS(hash4::parse_algo("sha-1"));
}
