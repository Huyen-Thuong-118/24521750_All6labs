#include "common/file_io.hpp"

namespace common {

std::vector<uint8_t> read_binary(const std::string&)                       { return {}; }
void                 write_binary(const std::string&, const std::vector<uint8_t>&) {}
std::string          read_text(const std::string&)                         { return {}; }
void                 write_text(const std::string&, const std::string&)    {}

} // namespace common
