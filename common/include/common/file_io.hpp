#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace common {

std::vector<uint8_t> read_binary(const std::string& path);
void                 write_binary(const std::string& path, const std::vector<uint8_t>& data);
std::string          read_text(const std::string& path);
void                 write_text(const std::string& path, const std::string& content);

} // namespace common
