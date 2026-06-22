// sigtool — Lab 5 CLI
//
// Usage:
//   sigtool keygen  ecdsa --priv priv.pem --pub pub.pem
//   sigtool keygen  rsa   --priv priv.pem --pub pub.pem
//   sigtool sign    ecdsa --priv priv.pem --in msg.bin --out sig.der
//   sigtool verify  ecdsa --pub  pub.pem  --in msg.bin --sig sig.der
//   sigtool verify  ecdsa --pub  pub.pem  --in msg.bin --sig sig.b64 --enc base64
//   sigtool sign    rsa   --priv priv.pem --in msg.bin --out sig.bin
//   sigtool verify  rsa   --pub  pub.pem  --in msg.bin --sig sig.bin

#include "signer.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>

static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

static void write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot write: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

static void write_text(const std::string& path, const std::string& text) {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot write: " + path);
    f << text;
}

static std::string read_text(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot read: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

static sig5::Algo parse_algo(const char* s) {
    if (std::strcmp(s, "ecdsa") == 0 || std::strcmp(s, "ecdsa-p256") == 0)
        return sig5::Algo::ECDSA_P256;
    if (std::strcmp(s, "rsa") == 0 || std::strcmp(s, "rsa-pss") == 0)
        return sig5::Algo::RSA_PSS_3072;
    throw std::runtime_error(std::string("Unknown algo: ") + s);
}

static std::string arg(int argc, char** argv, const char* flag, const char* def = "") {
    for (int i = 1; i < argc - 1; i++)
        if (std::strcmp(argv[i], flag) == 0) return argv[i+1];
    return def;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "sigtool keygen|sign|verify ecdsa|rsa [options]\n";
        return 1;
    }
    try {
        std::string cmd  = argv[1];
        sig5::Algo  algo = parse_algo(argv[2]);

        if (cmd == "keygen") {
            auto [priv, pub] = sig5::keygen(algo);
            auto priv_path = arg(argc, argv, "--priv", "priv.pem");
            auto pub_path  = arg(argc, argv, "--pub",  "pub.pem");
            write_text(priv_path, priv);
            write_text(pub_path,  pub);
            std::cout << "Keys written to " << priv_path << " / " << pub_path << "\n";

        } else if (cmd == "sign") {
            auto priv_pem = read_text(arg(argc, argv, "--priv", "priv.pem"));
            auto msg      = read_file(arg(argc, argv, "--in",   "msg.bin"));
            auto sig      = sig5::sign_msg(algo, priv_pem, msg);

            std::string enc = arg(argc, argv, "--enc", "raw");
            std::string out = arg(argc, argv, "--out", "sig.der");
            if (enc == "base64") {
                write_text(out, sig5::sig_to_base64(sig));
            } else {
                write_file(out, sig);
            }
            std::cout << "Signature written (" << sig.size() << " bytes)\n";

        } else if (cmd == "verify") {
            auto pub_pem  = read_text(arg(argc, argv, "--pub",  "pub.pem"));
            auto msg      = read_file(arg(argc, argv, "--in",   "msg.bin"));
            auto sig_path = arg(argc, argv, "--sig", "sig.der");
            std::string enc = arg(argc, argv, "--enc", "raw");

            std::vector<uint8_t> sig;
            if (enc == "base64") {
                auto b64text = read_text(sig_path);
                b64text.erase(std::remove(b64text.begin(), b64text.end(), '\n'), b64text.end());
                b64text.erase(std::remove(b64text.begin(), b64text.end(), '\r'), b64text.end());
                sig = sig5::sig_from_base64(b64text);
            } else {
                sig = read_file(sig_path);
            }

            bool ok = sig5::verify_msg(algo, pub_pem, msg, sig);
            std::cout << (ok ? "VALID\n" : "INVALID\n");
            return ok ? 0 : 2;

        } else {
            std::cerr << "Unknown command: " << cmd << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
