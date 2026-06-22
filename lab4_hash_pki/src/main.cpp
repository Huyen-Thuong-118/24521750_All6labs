#include "hasher.hpp"
#include "cert_parser.hpp"
#include "sha256_ext.hpp"
#include "common/codec.hpp"
#include "common/cli_parser.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ─── hash subcommand ────────────────────────────────────────────────────────
static void cmd_hash(const common::CliArgs& args) {
    std::string aname = common::get(args, "algo");
    if (aname.empty()) throw std::runtime_error("--algo required");

    auto algo = hash4::parse_algo(aname);
    size_t outlen = 0;
    auto outlen_s = common::get(args, "outlen");
    if (!outlen_s.empty()) outlen = std::stoul(outlen_s);

    std::string in_path = common::get(args, "in");
    std::string hex_in  = common::get(args, "hex");

    std::vector<uint8_t> digest;
    if (!in_path.empty()) {
        digest = hash4::hash_file(algo, in_path, outlen);
    } else if (!hex_in.empty()) {
        auto data = common::hex_decode(hex_in);
        digest = hash4::hash_bytes(algo, data, outlen);
    } else {
        digest = hash4::hash_bytes(algo, {}, outlen);
    }

    std::string out_path = common::get(args, "out");
    if (!out_path.empty()) {
        std::ofstream f(out_path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(digest.data()),
                static_cast<std::streamsize>(digest.size()));
    } else {
        std::cout << common::hex_encode(digest) << "\n";
    }
}

// ─── cert subcommand ────────────────────────────────────────────────────────
static void cmd_cert(const common::CliArgs& args) {
    std::string cert_path = common::get(args, "cert");
    if (cert_path.empty()) throw std::runtime_error("--cert required");

    auto info = pki::parse_cert(cert_path);
    std::cout << pki::to_string(info);

    std::string issuer_path = common::get(args, "issuer");
    if (!issuer_path.empty()) {
        bool ok = pki::verify_cert_signature(cert_path, issuer_path);
        std::cout << "Signature verify: " << (ok ? "PASS" : "FAIL") << "\n";
    }
}

// ─── kat subcommand ─────────────────────────────────────────────────────────
static void cmd_kat(const common::CliArgs& args) {
    std::vector<std::string> files;
    for (auto& a : args.positional) files.push_back(a);
    std::string dir = common::get(args, "dir");
    if (files.empty() && dir.empty()) {
        std::cout << "Usage: hashtool kat <file.json> ...\n";
        return;
    }
    if (!dir.empty()) {
        // dir mode: load sha2.json sha3.json shake.json
        for (auto& nm : {"sha2.json", "sha3.json", "shake.json"})
            files.push_back(dir + "/" + nm);
    }

    int pass = 0, fail = 0;
    for (auto& path : files) {
        std::ifstream f(path);
        if (!f) { std::cerr << "Cannot open " << path << "\n"; ++fail; continue; }
        json j; f >> j;
        for (auto& t : j["tests"]) {
            std::string name = t.value("name", "?");
            std::string algo_s = t.value("algo", "");
            std::string input_s = t.value("input", "");
            std::string expected_s = t.value("expected", "");
            size_t outlen = t.value("outlen", (size_t)0);
            try {
                auto algo = hash4::parse_algo(algo_s);
                std::vector<uint8_t> data;
                if (!input_s.empty()) data = common::hex_decode(input_s);
                auto got = hash4::hash_bytes(algo, data, outlen);
                std::string got_hex = common::hex_encode(got);
                if (got_hex == expected_s) {
                    std::cout << "PASS " << name << "\n";
                    ++pass;
                } else {
                    std::cout << "FAIL " << name
                              << "\n  got:      " << got_hex
                              << "\n  expected: " << expected_s << "\n";
                    ++fail;
                }
            } catch (const std::exception& e) {
                std::cout << "ERROR " << name << ": " << e.what() << "\n";
                ++fail;
            }
        }
    }
    std::cout << "\nKAT summary: " << pass << " PASS, " << fail << " FAIL\n";
}

// ─── extend subcommand ──────────────────────────────────────────────────────
static void cmd_extend(const common::CliArgs& args) {
    auto hash_s   = common::get(args, "hash");
    auto keylen_s = common::get(args, "keylen");
    auto msg_s    = common::get(args, "msg");
    auto ext_s    = common::get(args, "ext");

    if (hash_s.empty() || keylen_s.empty() || msg_s.empty() || ext_s.empty())
        throw std::runtime_error("--hash, --keylen, --msg, --ext all required");

    auto hash_h = common::hex_decode(hash_s);
    size_t key_len = std::stoul(keylen_s);
    auto msg = common::hex_decode(msg_s);
    auto ext = common::hex_decode(ext_s);

    auto res = sha256_ext::extend(hash_h, key_len, msg, ext);

    std::cout << "Forged hash:    " << common::hex_encode(res.forged_hash)    << "\n"
              << "Forged message: " << common::hex_encode(res.forged_message) << "\n";
}

// ─── entry point ────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    auto args = common::parse_args(argc, argv);

    if (args.command.empty() || args.command == "help") {
        std::cout <<
            "hashtool — Lab 4: Hashing, PKI & Practical Attacks\n\n"
            "Commands:\n"
            "  hash    --algo <algo> [--in <file>|--hex <hex>] [--outlen <n>] [--out <file>]\n"
            "          algos: sha224 sha256 sha384 sha512 sha3-256 sha3-512 shake128 shake256\n"
            "  cert    --cert <pem> [--issuer <issuer.pem>]\n"
            "  kat     <vectors.json> ...  OR  --dir <kat_dir>\n"
            "  extend  --hash <hex32> --keylen <n> --msg <hexmsg> --ext <hexext>\n";
        return 0;
    }

    try {
        if      (args.command == "hash")   cmd_hash(args);
        else if (args.command == "cert")   cmd_cert(args);
        else if (args.command == "kat")    cmd_kat(args);
        else if (args.command == "extend") cmd_extend(args);
        else {
            std::cerr << "Unknown command: " << args.command
                      << "  (run 'hashtool help')\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
