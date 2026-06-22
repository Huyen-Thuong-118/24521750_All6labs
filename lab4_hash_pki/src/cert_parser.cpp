#include "cert_parser.hpp"
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <sstream>
#include <stdexcept>
#include <ctime>

namespace pki {

// Parse ASN1_TIME to ISO-8601-like string
static std::string asn1_time_str(const ASN1_TIME* t) {
    if (!t) return "(none)";
    struct tm tm = {};
    ASN1_TIME_to_tm(t, &tm);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm);
    return buf;
}

// Load X509 from PEM or DER file
static X509* load_cert(const std::string& path) {
    BIO* bio = BIO_new_file(path.c_str(), "rb");
    if (!bio) throw std::runtime_error("Cannot open cert: " + path);

    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    if (!cert) {
        BIO_reset(bio);
        cert = d2i_X509_bio(bio, nullptr);
    }
    BIO_free(bio);
    if (!cert) throw std::runtime_error("Failed to parse cert: " + path);
    return cert;
}

static X509* load_cert_pem(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio) throw std::runtime_error("BIO_new_mem_buf failed");
    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!cert) throw std::runtime_error("Failed to parse PEM string");
    return cert;
}

static CertInfo extract(X509* cert) {
    CertInfo info;
    char buf[512];

    // Subject & Issuer
    X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
    info.subject = buf;
    X509_NAME_oneline(X509_get_issuer_name(cert), buf, sizeof(buf));
    info.issuer = buf;

    // Validity
    info.not_before = asn1_time_str(X509_get_notBefore(cert));
    info.not_after  = asn1_time_str(X509_get_notAfter(cert));

    // Signature algorithm
    int sig_nid = X509_get_signature_nid(cert);
    info.sig_algorithm = OBJ_nid2ln(sig_nid);

    // SPKI algorithm + bits
    EVP_PKEY* pkey = X509_get_pubkey(cert);
    if (pkey) {
        int kid = EVP_PKEY_get_id(pkey);
        info.spki_algorithm = (kid == EVP_PKEY_RSA)  ? "rsaEncryption"
                            : (kid == EVP_PKEY_EC)   ? "id-ecPublicKey"
                            : (kid == EVP_PKEY_DSA)  ? "dsaEncryption"
                            : OBJ_nid2sn(kid);
        info.spki_bits = EVP_PKEY_get_bits(pkey);
        EVP_PKEY_free(pkey);
    }

    // SANs
    GENERAL_NAMES* gns = static_cast<GENERAL_NAMES*>(
        X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
    if (gns) {
        int count = sk_GENERAL_NAME_num(gns);
        for (int i = 0; i < count; i++) {
            GENERAL_NAME* gn = sk_GENERAL_NAME_value(gns, i);
            if (gn->type == GEN_DNS) {
                std::string s((char*)ASN1_STRING_get0_data(gn->d.dNSName),
                              ASN1_STRING_length(gn->d.dNSName));
                info.sans.push_back("DNS:" + s);
            } else if (gn->type == GEN_EMAIL) {
                std::string s((char*)ASN1_STRING_get0_data(gn->d.rfc822Name),
                              ASN1_STRING_length(gn->d.rfc822Name));
                info.sans.push_back("email:" + s);
            } else if (gn->type == GEN_URI) {
                std::string s((char*)ASN1_STRING_get0_data(gn->d.uniformResourceIdentifier),
                              ASN1_STRING_length(gn->d.uniformResourceIdentifier));
                info.sans.push_back("URI:" + s);
            } else if (gn->type == GEN_IPADD) {
                // Raw IP bytes
                const uint8_t* ip = ASN1_STRING_get0_data(gn->d.iPAddress);
                int iplen = ASN1_STRING_length(gn->d.iPAddress);
                if (iplen == 4)
                    info.sans.push_back("IP:" + std::to_string(ip[0]) + "." +
                                        std::to_string(ip[1]) + "." +
                                        std::to_string(ip[2]) + "." +
                                        std::to_string(ip[3]));
            }
        }
        GENERAL_NAMES_free(gns);
    }

    // Key usage
    uint32_t ku = X509_get_key_usage(cert);
    if (ku != UINT32_MAX) {
        if (ku & KU_DIGITAL_SIGNATURE)  info.key_usages.push_back("Digital Signature");
        if (ku & KU_NON_REPUDIATION)    info.key_usages.push_back("Non Repudiation");
        if (ku & KU_KEY_ENCIPHERMENT)   info.key_usages.push_back("Key Encipherment");
        if (ku & KU_DATA_ENCIPHERMENT)  info.key_usages.push_back("Data Encipherment");
        if (ku & KU_KEY_AGREEMENT)      info.key_usages.push_back("Key Agreement");
        if (ku & KU_KEY_CERT_SIGN)      info.key_usages.push_back("Certificate Sign");
        if (ku & KU_CRL_SIGN)           info.key_usages.push_back("CRL Sign");
    }

    // IsCA: parse BasicConstraints extension directly (X509_check_ca is unreliable)
    {
        int idx = X509_get_ext_by_NID(cert, NID_basic_constraints, -1);
        if (idx >= 0) {
            auto* ext = X509_get_ext(cert, idx);
            auto* bc  = static_cast<BASIC_CONSTRAINTS*>(X509V3_EXT_d2i(ext));
            if (bc) {
                info.is_ca = (bc->ca != 0);
                BASIC_CONSTRAINTS_free(bc);
            }
        } else {
            info.is_ca = (X509_check_ca(cert) > 0);
        }
    }
    info.serial = ASN1_INTEGER_get(X509_get_serialNumber(cert));

    return info;
}

CertInfo parse_cert(const std::string& path) {
    X509* cert = load_cert(path);
    CertInfo info = extract(cert);
    X509_free(cert);
    return info;
}

CertInfo parse_cert_pem(const std::string& pem) {
    X509* cert = load_cert_pem(pem);
    CertInfo info = extract(cert);
    X509_free(cert);
    return info;
}

bool verify_cert_signature(const std::string& cert_path,
                            const std::string& issuer_cert_path)
{
    X509* cert   = load_cert(cert_path);
    X509* issuer = load_cert(issuer_cert_path);

    EVP_PKEY* pub = X509_get_pubkey(issuer);
    bool ok = false;
    if (pub) {
        ok = (X509_verify(cert, pub) == 1);
        EVP_PKEY_free(pub);
    }
    X509_free(cert);
    X509_free(issuer);
    return ok;
}

std::string to_string(const CertInfo& i) {
    std::ostringstream s;
    s << "Subject:   " << i.subject        << "\n"
      << "Issuer:    " << i.issuer         << "\n"
      << "NotBefore: " << i.not_before     << "\n"
      << "NotAfter:  " << i.not_after      << "\n"
      << "SigAlgo:   " << i.sig_algorithm  << "\n"
      << "SPKI:      " << i.spki_algorithm << " (" << i.spki_bits << " bits)\n"
      << "Serial:    " << i.serial         << "\n"
      << "IsCA:      " << (i.is_ca ? "yes" : "no") << "\n";

    if (!i.sans.empty()) {
        s << "SANs:";
        for (auto& san : i.sans) s << "\n  " << san;
        s << "\n";
    }
    if (!i.key_usages.empty()) {
        s << "KeyUsage:";
        for (auto& ku : i.key_usages) s << "\n  " << ku;
        s << "\n";
    }
    return s.str();
}

} // namespace pki
