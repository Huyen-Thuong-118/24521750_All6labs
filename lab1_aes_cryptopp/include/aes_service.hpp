#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace lab1 {

enum class AesMode { ECB, CBC, CFB, OFB, CTR, XTS, CCM, GCM };

AesMode mode_from_string(const std::string& s);  // "gcm" → GCM

struct AesResult {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> iv;   // IV/nonce dùng khi encrypt (caller lưu lại)
    std::vector<uint8_t> tag;  // chỉ có ở CCM/GCM
};

// encrypt: key phải là 32 bytes (AES-256)
// iv_in rỗng → tự sinh IV ngẫu nhiên an toàn
// no_padding=true dùng cho KAT (plaintext phải đúng bội số block size)
AesResult aes_encrypt(AesMode mode,
                      const std::vector<uint8_t>& key,
                      const std::vector<uint8_t>& plaintext,
                      const std::vector<uint8_t>& iv_in  = {},
                      const std::vector<uint8_t>& aad    = {},
                      bool no_padding = false);

// decrypt: iv và tag (nếu AEAD) phải truyền vào
// throws std::runtime_error nếu tag verify thất bại
std::vector<uint8_t> aes_decrypt(AesMode mode,
                                 const std::vector<uint8_t>& key,
                                 const std::vector<uint8_t>& ciphertext,
                                 const std::vector<uint8_t>& iv,
                                 const std::vector<uint8_t>& tag = {},
                                 const std::vector<uint8_t>& aad = {},
                                 bool no_padding = false);

} // namespace lab1
