#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace common {

std::string         hex_encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> hex_decode(const std::string& hex);
std::string         base64_encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> base64_decode(const std::string& b64);

} // namespace common
