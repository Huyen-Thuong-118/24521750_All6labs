#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace lab1 {

struct SidecarMeta {
    std::string alg;   // "AES-256"
    std::string mode;  // "gcm", "cbc", ...
    std::vector<uint8_t> iv;
    std::vector<uint8_t> aad;
    std::vector<uint8_t> tag;
};

// Ghi sidecar JSON ra <enc_path>.hdr.json
void sidecar_write(const std::string& enc_path, const SidecarMeta& meta);

// Đọc sidecar JSON từ <enc_path>.hdr.json; throw nếu không tìm thấy
SidecarMeta sidecar_read(const std::string& enc_path);

} // namespace lab1
