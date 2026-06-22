// Dispatcher: routes sig5::Algo to the correct signer + encoding helpers.

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include "signer.hpp"
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <stdexcept>
#include <cstring>

// Forward declarations from the per-algo translation units
namespace sig5 {

std::pair<std::string,std::string> keygen_ecdsa();
std::pair<std::string,std::string> keygen_rsa();

std::vector<uint8_t> ecdsa_sign(const std::string& priv_pem,
                                 const std::vector<uint8_t>& msg);
bool ecdsa_verify(const std::string& pub_pem,
                  const std::vector<uint8_t>& msg,
                  const std::vector<uint8_t>& sig_der);

std::vector<uint8_t> rsapss_sign(const std::string& priv_pem,
                                  const std::vector<uint8_t>& msg);
bool rsapss_verify(const std::string& pub_pem,
                   const std::vector<uint8_t>& msg,
                   const std::vector<uint8_t>& sig);

// ─── Public API implementation ────────────────────────────────────────────────

std::pair<std::string, std::string> keygen(Algo a) {
    return (a == Algo::ECDSA_P256) ? keygen_ecdsa() : keygen_rsa();
}

std::vector<uint8_t> sign_msg(Algo a,
                               const std::string& priv_pem,
                               const std::vector<uint8_t>& msg)
{
    return (a == Algo::ECDSA_P256) ? ecdsa_sign(priv_pem, msg)
                                   : rsapss_sign(priv_pem, msg);
}

bool verify_msg(Algo a,
                const std::string& pub_pem,
                const std::vector<uint8_t>& msg,
                const std::vector<uint8_t>& sig_der)
{
    return (a == Algo::ECDSA_P256) ? ecdsa_verify(pub_pem, msg, sig_der)
                                   : rsapss_verify(pub_pem, msg, sig_der);
}

int verify_batch(Algo a,
                  const std::string& pub_pem,
                  const std::vector<std::vector<uint8_t>>& msgs,
                  const std::vector<std::vector<uint8_t>>& sigs)
{
    int ok = 0;
    for (size_t i = 0; i < msgs.size() && i < sigs.size(); i++)
        if (verify_msg(a, pub_pem, msgs[i], sigs[i])) ok++;
    return ok;
}

// ─── Base64 helpers (OpenSSL BIO) ────────────────────────────────────────────

std::string sig_to_base64(const std::vector<uint8_t>& der) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, der.data(), static_cast<int>(der.size()));
    BIO_flush(b64);
    BUF_MEM* bptr; BIO_get_mem_ptr(mem, &bptr);
    std::string out(bptr->data, bptr->length);
    BIO_free_all(b64);
    return out;
}

std::vector<uint8_t> sig_from_base64(const std::string& b64str) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new_mem_buf(b64str.data(), static_cast<int>(b64str.size()));
    BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    std::vector<uint8_t> buf(b64str.size());
    int n = BIO_read(b64, buf.data(), static_cast<int>(buf.size()));
    BIO_free_all(b64);
    if (n <= 0) throw std::runtime_error("base64 decode failed");
    buf.resize(n);
    return buf;
}

} // namespace sig5
