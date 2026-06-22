#include "common/codec.hpp"
#include <cryptopp/base64.h>
#include <cryptopp/filters.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace common {

std::string hex_encode(const std::vector<uint8_t>& data) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto b : data) ss << std::setw(2) << (int)b;
    return ss.str();
}

std::vector<uint8_t> hex_decode(const std::string& hex) {
    if (hex.size() % 2) throw std::runtime_error("hex_decode: odd length");
    std::vector<uint8_t> r(hex.size() / 2);
    for (size_t i = 0; i < r.size(); ++i)
        r[i] = static_cast<uint8_t>(std::stoul(hex.substr(2 * i, 2), nullptr, 16));
    return r;
}

std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string result;
    CryptoPP::Base64Encoder enc(new CryptoPP::StringSink(result), false); // no newlines
    enc.Put(data.data(), data.size());
    enc.MessageEnd();
    return result;
}

std::vector<uint8_t> base64_decode(const std::string& b64) {
    std::string result;
    CryptoPP::Base64Decoder dec(new CryptoPP::StringSink(result));
    dec.Put(reinterpret_cast<const CryptoPP::byte*>(b64.data()), b64.size());
    dec.MessageEnd();
    return {result.begin(), result.end()};
}

} // namespace common
