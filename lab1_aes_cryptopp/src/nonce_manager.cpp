#include "nonce_manager.hpp"
#include "key_manager.hpp"
#include <common/rng.hpp>
#include <nlohmann/json.hpp>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace lab1 {

using json = nlohmann::json;

std::size_t iv_size_for_mode(const std::string& mode_str) {
    std::string m = mode_str;
    for (auto& c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (m == "ecb") return 0;
    if (m == "gcm") return 12;
    if (m == "ccm") return 11;
    return 16; // cbc/cfb/ofb/ctr/xts
}

std::vector<uint8_t> generate_iv(const std::string& mode_str) {
    std::size_t n = iv_size_for_mode(mode_str);
    if (n == 0) return {};
    return common::random_bytes(n);
}

void validate_iv(const std::string& mode_str, const std::vector<uint8_t>& iv) {
    std::size_t expected = iv_size_for_mode(mode_str);
    if (expected == 0) {
        if (!iv.empty())
            throw std::runtime_error("ECB mode does not use an IV");
        return;
    }
    if (iv.size() != expected)
        throw std::runtime_error(
            "IV length wrong for mode '" + mode_str + "': expected " +
            std::to_string(expected) + " bytes, got " + std::to_string(iv.size()));
}

void check_and_record_nonce(const std::string& key_hex,
                            const std::string& nonce_hex,
                            const std::string& log_path)
{
    json log_j = json::object();
    {
        std::ifstream f(log_path);
        if (f) {
            try { log_j = json::parse(f); }
            catch (...) { log_j = json::object(); }
        }
    }

    std::string entry = key_hex + ":" + nonce_hex;
    if (log_j.contains(entry))
        throw std::runtime_error(
            "NONCE REUSE DETECTED for this key+nonce — refusing to encrypt.\n"
            "Reusing a nonce with CTR/CCM/GCM destroys confidentiality.");

    log_j[entry] = true;
    std::ofstream f(log_path);
    if (f) f << log_j.dump(2) << "\n";
}

} // namespace lab1
