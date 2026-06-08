#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace common {

// Sinh n byte ngẫu nhiên dùng AutoSeededRandomPool (Crypto++) hoặc RAND_bytes (OpenSSL)
// KHÔNG dùng rand() / std::random_device trực tiếp cho mục đích crypto
std::vector<uint8_t> random_bytes(std::size_t n);

} // namespace common
