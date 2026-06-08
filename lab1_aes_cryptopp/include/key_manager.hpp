#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace lab1 {

// Sinh AES-256 key (32 bytes) dùng AutoSeededRandomPool
std::vector<uint8_t> keygen_aes256();

// Lưu key ra file (hex text)
void save_key_hex(const std::string& path, const std::vector<uint8_t>& key);

// Đọc key từ file hex (mỗi dòng là hex string)
std::vector<uint8_t> load_key_hex(const std::string& path);

// Tự động nhận dạng: nếu file chứa hex string → parse hex, ngược lại → đọc binary
std::vector<uint8_t> load_key_auto(const std::string& path);

// Parse hex string → bytes
std::vector<uint8_t> hex_to_bytes(const std::string& hex);

// Bytes → hex string (lowercase)
std::string bytes_to_hex(const std::vector<uint8_t>& data);

} // namespace lab1
