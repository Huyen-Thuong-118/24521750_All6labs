// X.509 parse + verify tests — generate self-signed cert on the fly
#include <catch2/catch_test_macros.hpp>
#include "cert_parser.hpp"
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/x509v3.h>
#include <string>
#include <ctime>

// ─── Generate a self-signed RSA certificate as PEM string ───────────────────
static std::string make_self_signed_pem(const std::string& cn = "test.example.com",
                                         bool is_ca = false)
{
    // 2048-bit RSA key
    EVP_PKEY* pkey = EVP_RSA_gen(2048);
    if (!pkey) throw std::runtime_error("EVP_RSA_gen failed");

    X509* cert = X509_new();
    X509_set_version(cert, 2); // v3
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 42);

    // Validity: now → now + 365 days
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert),  365 * 24 * 3600L);

    X509_set_pubkey(cert, pkey);

    // Subject = Issuer (self-signed)
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>(cn.c_str()), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("TestOrg"), -1, -1, 0);
    X509_set_issuer_name(cert, name);

    // Extensions
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);

    if (is_ca) {
        // Use low-level API to reliably add BasicConstraints CA:TRUE
        BASIC_CONSTRAINTS* bc_val = BASIC_CONSTRAINTS_new();
        bc_val->ca = 0xFF;   // non-zero = TRUE
        X509_EXTENSION* bc_ext = X509V3_EXT_i2d(NID_basic_constraints, 1, bc_val);
        if (bc_ext) { X509_add_ext(cert, bc_ext, -1); X509_EXTENSION_free(bc_ext); }
        BASIC_CONSTRAINTS_free(bc_val);
    }

    // SAN
    X509_EXTENSION* san = X509V3_EXT_conf_nid(nullptr, &ctx,
        NID_subject_alt_name, ("DNS:" + cn).c_str());
    if (san) { X509_add_ext(cert, san, -1); X509_EXTENSION_free(san); }

    // Key usage
    X509_EXTENSION* ku = X509V3_EXT_conf_nid(nullptr, &ctx,
        NID_key_usage, "critical,digitalSignature,keyEncipherment");
    if (ku) { X509_add_ext(cert, ku, -1); X509_EXTENSION_free(ku); }

    X509_sign(cert, pkey, EVP_sha256());

    BIO* bio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(bio, cert);
    BUF_MEM* mem = nullptr;
    BIO_get_mem_ptr(bio, &mem);
    std::string pem(mem->data, mem->length);

    BIO_free(bio);
    X509_free(cert);
    EVP_PKEY_free(pkey);
    return pem;
}

// Write PEM to temp file and return path
static std::string write_temp(const std::string& pem, const std::string& name) {
    std::string path = "tmp_" + name + ".pem";
    FILE* f = fopen(path.c_str(), "wb");
    if (f) { fwrite(pem.data(), 1, pem.size(), f); fclose(f); }
    return path;
}

// ─── Tests ──────────────────────────────────────────────────────────────────
TEST_CASE("parse_cert_pem: subject contains CN", "[x509][parse]") {
    auto pem = make_self_signed_pem("mytest.example.com");
    auto info = pki::parse_cert_pem(pem);
    REQUIRE(info.subject.find("mytest.example.com") != std::string::npos);
}

TEST_CASE("parse_cert_pem: issuer equals subject (self-signed)", "[x509][parse]") {
    auto pem = make_self_signed_pem("self.example.com");
    auto info = pki::parse_cert_pem(pem);
    REQUIRE(info.subject == info.issuer);
}

TEST_CASE("parse_cert_pem: validity dates are set", "[x509][parse]") {
    auto pem = make_self_signed_pem();
    auto info = pki::parse_cert_pem(pem);
    REQUIRE(!info.not_before.empty());
    REQUIRE(!info.not_after.empty());
    REQUIRE(info.not_before != info.not_after);
}

TEST_CASE("parse_cert_pem: SPKI is RSA 2048 bits", "[x509][parse]") {
    auto pem = make_self_signed_pem();
    auto info = pki::parse_cert_pem(pem);
    REQUIRE(info.spki_bits == 2048);
    REQUIRE(info.spki_algorithm == "rsaEncryption");
}

TEST_CASE("parse_cert_pem: signature algo is sha256WithRSAEncryption", "[x509][parse]") {
    auto pem = make_self_signed_pem();
    auto info = pki::parse_cert_pem(pem);
    REQUIRE(info.sig_algorithm.find("sha256") != std::string::npos);
}

TEST_CASE("parse_cert_pem: SAN contains DNS entry", "[x509][parse]") {
    auto pem = make_self_signed_pem("san.example.com");
    auto info = pki::parse_cert_pem(pem);
    bool found = false;
    for (auto& s : info.sans)
        if (s.find("san.example.com") != std::string::npos) found = true;
    REQUIRE(found);
}

TEST_CASE("parse_cert_pem: CA flag", "[x509][parse]") {
    auto pem_ca   = make_self_signed_pem("ca.example.com", true);
    auto pem_leaf = make_self_signed_pem("leaf.example.com", false);
    REQUIRE(pki::parse_cert_pem(pem_ca).is_ca   == true);
    REQUIRE(pki::parse_cert_pem(pem_leaf).is_ca == false);
}

TEST_CASE("parse_cert_pem: serial number matches", "[x509][parse]") {
    auto pem = make_self_signed_pem();
    auto info = pki::parse_cert_pem(pem);
    REQUIRE(info.serial == 42);
}

TEST_CASE("parse_cert_pem: key usage Digital Signature present", "[x509][parse]") {
    auto pem = make_self_signed_pem();
    auto info = pki::parse_cert_pem(pem);
    bool found = false;
    for (auto& ku : info.key_usages)
        if (ku.find("Digital Signature") != std::string::npos) found = true;
    REQUIRE(found);
}

TEST_CASE("verify_cert_signature: self-signed cert verifies against itself", "[x509][verify]") {
    auto pem = make_self_signed_pem("verify.example.com");
    auto cert_path   = write_temp(pem, "cert");
    auto issuer_path = write_temp(pem, "issuer");

    bool ok = pki::verify_cert_signature(cert_path, issuer_path);
    REQUIRE(ok == true);

    std::remove(cert_path.c_str());
    std::remove(issuer_path.c_str());
}

TEST_CASE("verify_cert_signature: cert signed by different key fails", "[x509][verify]") {
    auto pem_cert   = make_self_signed_pem("cert.example.com");
    auto pem_other  = make_self_signed_pem("other.example.com");
    auto cert_path  = write_temp(pem_cert,  "cert2");
    auto other_path = write_temp(pem_other, "other");

    bool ok = pki::verify_cert_signature(cert_path, other_path);
    REQUIRE(ok == false);

    std::remove(cert_path.c_str());
    std::remove(other_path.c_str());
}

TEST_CASE("to_string: contains all 7 required fields", "[x509][display]") {
    auto pem  = make_self_signed_pem("display.example.com");
    auto info = pki::parse_cert_pem(pem);
    auto str  = pki::to_string(info);
    REQUIRE(str.find("Subject")    != std::string::npos);
    REQUIRE(str.find("Issuer")     != std::string::npos);
    REQUIRE(str.find("NotBefore")  != std::string::npos);
    REQUIRE(str.find("NotAfter")   != std::string::npos);
    REQUIRE(str.find("SigAlgo")    != std::string::npos);
    REQUIRE(str.find("SPKI")       != std::string::npos);
    REQUIRE(str.find("Serial")     != std::string::npos);
}
