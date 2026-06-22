#include "common/file_io.hpp"
#include <fstream>
#include <stdexcept>

namespace common {

std::vector<uint8_t> read_binary(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

void write_binary(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot write file: " + path);
    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
}

std::string read_text(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

void write_text(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot write file: " + path);
    f << content;
}

} // namespace common
