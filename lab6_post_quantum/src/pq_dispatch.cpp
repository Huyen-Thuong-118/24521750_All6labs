// Base64 helpers shared across Lab 6 modules.

#include "pqtool.hpp"
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <stdexcept>

namespace pq {

std::string to_base64(const std::vector<uint8_t>& data) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data.data(), static_cast<int>(data.size()));
    BIO_flush(b64);
    BUF_MEM* bptr; BIO_get_mem_ptr(mem, &bptr);
    std::string out(bptr->data, bptr->length);
    BIO_free_all(b64);
    return out;
}

std::vector<uint8_t> from_base64(const std::string& b64str) {
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

} // namespace pq
