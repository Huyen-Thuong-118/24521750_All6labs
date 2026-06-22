#include "hasher.hpp"
#include <openssl/evp.h>
#include <fstream>
#include <stdexcept>

namespace hash4 {

static const EVP_MD* get_md(Algo a) {
    switch (a) {
        case Algo::SHA224:   return EVP_sha224();
        case Algo::SHA256:   return EVP_sha256();
        case Algo::SHA384:   return EVP_sha384();
        case Algo::SHA512:   return EVP_sha512();
        case Algo::SHA3_224: return EVP_sha3_224();
        case Algo::SHA3_256: return EVP_sha3_256();
        case Algo::SHA3_384: return EVP_sha3_384();
        case Algo::SHA3_512: return EVP_sha3_512();
        case Algo::SHAKE128: return EVP_shake128();
        case Algo::SHAKE256: return EVP_shake256();
    }
    throw std::runtime_error("get_md: unknown algo");
}

static bool is_xof(Algo a) {
    return a == Algo::SHAKE128 || a == Algo::SHAKE256;
}

Algo parse_algo(const std::string& name) {
    if (name == "sha224")                         return Algo::SHA224;
    if (name == "sha256")                         return Algo::SHA256;
    if (name == "sha384")                         return Algo::SHA384;
    if (name == "sha512")                         return Algo::SHA512;
    if (name == "sha3-224" || name == "sha3_224") return Algo::SHA3_224;
    if (name == "sha3-256" || name == "sha3_256") return Algo::SHA3_256;
    if (name == "sha3-384" || name == "sha3_384") return Algo::SHA3_384;
    if (name == "sha3-512" || name == "sha3_512") return Algo::SHA3_512;
    if (name == "shake128")                       return Algo::SHAKE128;
    if (name == "shake256")                       return Algo::SHAKE256;
    throw std::runtime_error("Unknown algorithm: " + name);
}

std::string algo_name(Algo a) {
    switch (a) {
        case Algo::SHA224:   return "sha224";
        case Algo::SHA256:   return "sha256";
        case Algo::SHA384:   return "sha384";
        case Algo::SHA512:   return "sha512";
        case Algo::SHA3_224: return "sha3-224";
        case Algo::SHA3_256: return "sha3-256";
        case Algo::SHA3_384: return "sha3-384";
        case Algo::SHA3_512: return "sha3-512";
        case Algo::SHAKE128: return "shake128";
        case Algo::SHAKE256: return "shake256";
    }
    return "unknown";
}

size_t default_outlen(Algo a) {
    switch (a) {
        case Algo::SHA224:   return 28;
        case Algo::SHA256:   return 32;
        case Algo::SHA384:   return 48;
        case Algo::SHA512:   return 64;
        case Algo::SHA3_224: return 28;
        case Algo::SHA3_256: return 32;
        case Algo::SHA3_384: return 48;
        case Algo::SHA3_512: return 64;
        case Algo::SHAKE128: return 0;
        case Algo::SHAKE256: return 0;
    }
    return 0;
}

std::vector<uint8_t> hash_bytes(Algo a, const std::vector<uint8_t>& data, size_t outlen) {
    if (is_xof(a) && outlen == 0)
        throw std::runtime_error("SHAKE requires --outlen");

    size_t len = (outlen > 0) ? outlen : default_outlen(a);

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");

    std::vector<uint8_t> result(len);
    bool ok = true;

    ok = ok && (EVP_DigestInit_ex(ctx, get_md(a), nullptr) == 1);
    ok = ok && (data.empty() || EVP_DigestUpdate(ctx, data.data(), data.size()) == 1);

    if (ok) {
        if (is_xof(a)) {
            ok = (EVP_DigestFinalXOF(ctx, result.data(), len) == 1);
        } else {
            unsigned int dlen = static_cast<unsigned int>(len);
            ok = (EVP_DigestFinal_ex(ctx, result.data(), &dlen) == 1);
            if (ok) result.resize(dlen);
        }
    }

    EVP_MD_CTX_free(ctx);
    if (!ok) throw std::runtime_error("OpenSSL digest failed");
    return result;
}

std::vector<uint8_t> hash_file(Algo a, const std::string& path, size_t outlen) {
    if (is_xof(a) && outlen == 0)
        throw std::runtime_error("SHAKE requires outlen for file hashing");

    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open: " + path);

    size_t len = (outlen > 0) ? outlen : default_outlen(a);

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");

    bool ok = (EVP_DigestInit_ex(ctx, get_md(a), nullptr) == 1);

    constexpr size_t CHUNK = 65536;
    std::vector<uint8_t> buf(CHUNK);
    while (ok && (f.read(reinterpret_cast<char*>(buf.data()), CHUNK) || f.gcount() > 0)) {
        size_t n = static_cast<size_t>(f.gcount());
        ok = (EVP_DigestUpdate(ctx, buf.data(), n) == 1);
    }

    std::vector<uint8_t> result(len);
    if (ok) {
        if (is_xof(a)) {
            ok = (EVP_DigestFinalXOF(ctx, result.data(), len) == 1);
        } else {
            unsigned int dlen = static_cast<unsigned int>(len);
            ok = (EVP_DigestFinal_ex(ctx, result.data(), &dlen) == 1);
            if (ok) result.resize(dlen);
        }
    }

    EVP_MD_CTX_free(ctx);
    if (!ok) throw std::runtime_error("OpenSSL digest (file) failed");
    return result;
}

} // namespace hash4
