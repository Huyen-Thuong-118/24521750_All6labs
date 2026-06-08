#include "common/cli_parser.hpp"
#include <stdexcept>

namespace common {

// parse_args: argv[1] = command, sau đó --key val hoặc --flag (không có val)
CliArgs parse_args(int argc, char* argv[]) {
    CliArgs result;
    if (argc < 2) return result;

    result.command = argv[1];

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
            std::string key = arg.substr(2);
            // Nếu arg tiếp theo tồn tại và không phải flag → lấy làm value
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                result.opts[key] = argv[++i];
            } else {
                result.opts[key] = "";  // flag dạng --verbose (không có value)
            }
        } else {
            result.positional.push_back(arg);
        }
    }
    return result;
}

bool has(const CliArgs& a, const std::string& flag) {
    return a.opts.count(flag) > 0;
}

std::string get(const CliArgs& a, const std::string& key, const std::string& def) {
    auto it = a.opts.find(key);
    return (it != a.opts.end()) ? it->second : def;
}

} // namespace common
