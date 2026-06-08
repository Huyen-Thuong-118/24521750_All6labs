#include "key_manager.hpp"
#include <cryptopp/osrng.h>
#include <cryptopp/hex.h>
#include <cryptopp/filters.h>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

using namespace CryptoPP;

namespace lab1 {

std::vector<uint8_t> keygen_aes256() {
    AutoSeededRandomPool rng;
    std::vector<uint8_t> key(32);
    rng.GenerateBlock(key.data(), key.size());
    return key;
}

std::string bytes_to_hex(const std::vector<uint8_t>& data) {
    std::string out;
    HexEncoder enc(new StringSink(out), false); // false = lowercase
    enc.Put(data.data(), data.size());
    enc.MessageEnd();
    return out;
}

std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::string out;
    StringSource ss(hex, true, new HexDecoder(new StringSink(out)));
    return std::vector<uint8_t>(out.begin(), out.end());
}

void save_key_hex(const std::string& path, const std::vector<uint8_t>& key) {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Không thể ghi key file: " + path);
    f << bytes_to_hex(key) << "\n";
}

std::vector<uint8_t> load_key_hex(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Không thể đọc key file: " + path);
    std::string hex;
    f >> hex;
    hex.erase(std::remove_if(hex.begin(), hex.end(), ::isspace), hex.end());
    return hex_to_bytes(hex);
}

std::vector<uint8_t> load_key_auto(const std::string& path) {
    // Đọc file dưới dạng text để kiểm tra có phải hex không
    std::ifstream ft(path);
    if (!ft) throw std::runtime_error("Không thể đọc key file: " + path);
    std::string content((std::istreambuf_iterator<char>(ft)), {});
    content.erase(std::remove_if(content.begin(), content.end(), ::isspace), content.end());

    // Nếu toàn ký tự hex và độ dài đúng (32/48/64 hex = 16/24/32 bytes) → parse hex
    bool all_hex = !content.empty() && std::all_of(content.begin(), content.end(), ::isxdigit);
    std::size_t n = content.size();
    if (all_hex && (n == 32 || n == 48 || n == 64))
        return hex_to_bytes(content);

    // Ngược lại đọc raw binary
    std::ifstream fb(path, std::ios::binary);
    if (!fb) throw std::runtime_error("Không thể đọc key file binary: " + path);
    return {std::istreambuf_iterator<char>(fb), {}};
}

} // namespace lab1
