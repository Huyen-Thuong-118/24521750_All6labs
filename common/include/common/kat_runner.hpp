#pragma once
#include <string>

namespace common {

struct KatResult { int pass; int fail; };

// Đọc JSON file chứa KAT vectors, chạy fn(input) == expected cho từng vector
// json_path: đường dẫn tới .json (format NIST CAVP / tự định nghĩa)
KatResult run_kat(const std::string& json_path);

} // namespace common
