#include "common/rng.hpp"
#include <cryptopp/osrng.h>

namespace common {

std::vector<uint8_t> random_bytes(std::size_t n) {
    CryptoPP::AutoSeededRandomPool rng;
    std::vector<uint8_t> buf(n);
    rng.GenerateBlock(buf.data(), n);
    return buf;
}

} // namespace common
