#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace lab1 {

// Số byte IV/nonce đúng theo mode
std::size_t iv_size_for_mode(const std::string& mode_str);

// Sinh IV ngẫu nhiên an toàn cho mode (ECB → rỗng)
std::vector<uint8_t> generate_iv(const std::string& mode_str);

// Validate IV length; throw std::runtime_error nếu sai
void validate_iv(const std::string& mode_str, const std::vector<uint8_t>& iv);

// Kiểm tra + ghi nhận (key_hex, nonce_hex) vào log_path
// Throw nếu cặp đó đã được dùng (nonce-reuse)
void check_and_record_nonce(const std::string& key_hex,
                            const std::string& nonce_hex,
                            const std::string& log_path = ".nonce_log.json");

} // namespace lab1
