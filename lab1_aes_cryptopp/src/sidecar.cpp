#include "sidecar.hpp"
#include "key_manager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace lab1 {

using json = nlohmann::json;

static std::string sidecar_path(const std::string& enc_path) {
    return enc_path + ".hdr.json";
}

void sidecar_write(const std::string& enc_path, const SidecarMeta& meta) {
    json j;
    j["alg"]  = meta.alg;
    j["mode"] = meta.mode;
    j["iv"]   = bytes_to_hex(meta.iv);
    j["aad"]  = bytes_to_hex(meta.aad);
    j["tag"]  = bytes_to_hex(meta.tag);

    std::ofstream f(sidecar_path(enc_path));
    if (!f) throw std::runtime_error("Cannot write sidecar: " + sidecar_path(enc_path));
    f << j.dump(2) << "\n";
}

SidecarMeta sidecar_read(const std::string& enc_path) {
    std::ifstream f(sidecar_path(enc_path));
    if (!f) throw std::runtime_error("Sidecar not found: " + sidecar_path(enc_path));
    json j = json::parse(f);

    SidecarMeta meta;
    meta.alg  = j.value("alg", "AES-256");
    meta.mode = j.value("mode", "");

    std::string iv_hex  = j.value("iv",  "");
    std::string aad_hex = j.value("aad", "");
    std::string tag_hex = j.value("tag", "");

    if (!iv_hex.empty())  meta.iv  = hex_to_bytes(iv_hex);
    if (!aad_hex.empty()) meta.aad = hex_to_bytes(aad_hex);
    if (!tag_hex.empty()) meta.tag = hex_to_bytes(tag_hex);

    return meta;
}

} // namespace lab1
