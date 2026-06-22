#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace hash4 {

enum class Algo {
    SHA224, SHA256, SHA384, SHA512,
    SHA3_224, SHA3_256, SHA3_384, SHA3_512,
    SHAKE128, SHAKE256
};

Algo        parse_algo(const std::string& name);
std::string algo_name(Algo a);
size_t      default_outlen(Algo a);  // 0 for SHAKE (must be supplied)

// Hash a byte buffer
std::vector<uint8_t> hash_bytes(Algo a,
                                 const std::vector<uint8_t>& data,
                                 size_t outlen = 0);

// Hash a file (streamed, supports very large files)
std::vector<uint8_t> hash_file(Algo a,
                                const std::string& path,
                                size_t outlen = 0);

} // namespace hash4
