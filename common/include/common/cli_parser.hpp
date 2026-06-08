#pragma once
#include <map>
#include <string>
#include <vector>

namespace common {

struct CliArgs {
    std::string command;
    std::map<std::string, std::string> opts;
    std::vector<std::string> positional;
};

CliArgs parse_args(int argc, char* argv[]);
bool has(const CliArgs& a, const std::string& flag);
std::string get(const CliArgs& a, const std::string& key, const std::string& def = "");

} // namespace common
