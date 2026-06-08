#include "common/codec.hpp"

namespace common {

std::string         hex_encode(const std::vector<uint8_t>&) { return {}; }
std::vector<uint8_t> hex_decode(const std::string&)         { return {}; }
std::string         base64_encode(const std::vector<uint8_t>&) { return {}; }
std::vector<uint8_t> base64_decode(const std::string&)         { return {}; }

} // namespace common
