// PQ Certificate tests — issue/verify, tamper detection
#include <catch2/catch_test_macros.hpp>
#include "pqtool.hpp"
#include <string>

TEST_CASE("PqCert: issue and verify valid cert", "[cert][issue]") {
    auto [ca_priv, ca_pub] = pq::mldsa_keygen();
    auto [sub_priv, sub_pub] = pq::mldsa_keygen();

    auto cert = pq::cert_issue(ca_priv, "CN=example.com", sub_pub, "CN=TestCA");
    REQUIRE(pq::cert_verify(ca_pub, cert));
}

TEST_CASE("PqCert: cert fields are set correctly", "[cert][issue]") {
    auto [ca_priv, ca_pub] = pq::mldsa_keygen();
    auto [sub_priv, sub_pub] = pq::mldsa_keygen();

    auto cert = pq::cert_issue(ca_priv, "CN=testhost", sub_pub, "CN=MyCA");
    REQUIRE(cert.subject    == "CN=testhost");
    REQUIRE(cert.issuer     == "CN=MyCA");
    REQUIRE(!cert.public_key.empty());
    REQUIRE(!cert.signature.empty());
}

TEST_CASE("PqCert: verify with wrong CA key fails", "[cert][negative]") {
    auto [ca_priv1, ca_pub1] = pq::mldsa_keygen();
    auto [ca_priv2, ca_pub2] = pq::mldsa_keygen();
    auto [sub_priv, sub_pub] = pq::mldsa_keygen();

    auto cert = pq::cert_issue(ca_priv1, "CN=test", sub_pub, "CN=CA1");
    REQUIRE_FALSE(pq::cert_verify(ca_pub2, cert));
}

TEST_CASE("PqCert: tampered subject -verify fails", "[cert][negative]") {
    auto [ca_priv, ca_pub] = pq::mldsa_keygen();
    auto [sub_priv, sub_pub] = pq::mldsa_keygen();

    auto cert = pq::cert_issue(ca_priv, "CN=real", sub_pub, "CN=CA");
    cert.subject = "CN=fake";
    REQUIRE_FALSE(pq::cert_verify(ca_pub, cert));
}

TEST_CASE("PqCert: tampered public_key -verify fails", "[cert][negative]") {
    auto [ca_priv, ca_pub] = pq::mldsa_keygen();
    auto [sub_priv, sub_pub] = pq::mldsa_keygen();

    auto cert = pq::cert_issue(ca_priv, "CN=real", sub_pub, "CN=CA");
    // Modify base64 public key
    if (!cert.public_key.empty()) cert.public_key[0] ^= 'A' ^ 'B';
    REQUIRE_FALSE(pq::cert_verify(ca_pub, cert));
}

TEST_CASE("PqCert: tampered issuer -verify fails", "[cert][negative]") {
    auto [ca_priv, ca_pub] = pq::mldsa_keygen();
    auto [sub_priv, sub_pub] = pq::mldsa_keygen();

    auto cert = pq::cert_issue(ca_priv, "CN=test", sub_pub, "CN=RealCA");
    cert.issuer = "CN=FakeCA";
    REQUIRE_FALSE(pq::cert_verify(ca_pub, cert));
}

TEST_CASE("PqCert: tampered signature -verify fails", "[cert][negative]") {
    auto [ca_priv, ca_pub] = pq::mldsa_keygen();
    auto [sub_priv, sub_pub] = pq::mldsa_keygen();

    auto cert = pq::cert_issue(ca_priv, "CN=test", sub_pub, "CN=CA");
    if (!cert.signature.empty()) cert.signature[0] ^= 'A' ^ 'Z';
    REQUIRE_FALSE(pq::cert_verify(ca_pub, cert));
}

TEST_CASE("PqCert: JSON serialise + deserialise roundtrip", "[cert][json]") {
    auto [ca_priv, ca_pub] = pq::mldsa_keygen();
    auto [sub_priv, sub_pub] = pq::mldsa_keygen();

    auto cert = pq::cert_issue(ca_priv, "CN=roundtrip.example", sub_pub, "CN=CA");
    auto json_str = pq::cert_to_json(cert);
    REQUIRE(!json_str.empty());
    REQUIRE(json_str.find("subject") != std::string::npos);

    auto cert2 = pq::cert_from_json(json_str);
    REQUIRE(cert2.subject    == cert.subject);
    REQUIRE(cert2.issuer     == cert.issuer);
    REQUIRE(cert2.public_key == cert.public_key);
    REQUIRE(cert2.signature  == cert.signature);

    REQUIRE(pq::cert_verify(ca_pub, cert2));
}

TEST_CASE("PqCert: ML-KEM key cannot sign (only ML-DSA can issue certs)", "[cert][mlkem]") {
    // Trying to use ML-KEM private key for cert_issue should throw or fail
    auto [kem_priv, kem_pub] = pq::mlkem_keygen();
    auto [sub_priv, sub_pub] = pq::mldsa_keygen();
    // ML-KEM key is NOT a signing key — signing with it should throw
    REQUIRE_THROWS(pq::cert_issue(kem_priv, "CN=test", sub_pub, "CN=CA"));
}
