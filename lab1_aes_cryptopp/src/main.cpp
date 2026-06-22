// ─────────────────────────────────────────────────────────────────────────────
// main.cpp — aestool: AES CLI (Lab 1)
//
// Commands:
//   aestool encrypt --mode MODE --key FILE --in FILE [--out FILE]
//                   [--iv FILE | --iv-hex HEX] [--aead] [--aad TEXT]
//                   [--encode hex|base64|raw]
//   aestool decrypt --mode MODE --key FILE --in FILE [--out FILE]
//                   [--iv FILE | --iv-hex HEX] [--aead] [--aad TEXT]
//   aestool --kat vectors.json
//   aestool bench  --mode MODE --key-hex HEX [--size BYTES] [--rounds N]
//
// Key flags:
//   --key FILE         raw binary key file (16/24/32 bytes; XTS: 32/48/64)
//   --key-hex HEX      hex-encoded key
//   --iv FILE          raw binary IV file
//   --iv-hex HEX       hex-encoded IV
// ─────────────────────────────────────────────────────────────────────────────
#include "aes_modes.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <filesystem>
#include <cryptopp/base64.h>
#include <cryptopp/filters.h>

using json = nlohmann::json;
using Clock = std::chrono::high_resolution_clock;
namespace fs = std::filesystem;

// ── Utility ──────────────────────────────────────────────────────────────────
static std::vector<uint8_t> read_file(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open: " + p);
    return {std::istreambuf_iterator<char>(f), {}};
}

static void write_file(const std::string& p, const std::vector<uint8_t>& d) {
    std::ofstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot write: " + p);
    f.write(reinterpret_cast<const char*>(d.data()), (std::streamsize)d.size());
}

static std::vector<uint8_t> hex_to_bytes(const std::string& h) {
    if (h.size() % 2) throw std::runtime_error("Odd hex length");
    std::vector<uint8_t> r(h.size() / 2);
    for (size_t i = 0; i < r.size(); i++)
        r[i] = (uint8_t)std::stoul(h.substr(2*i, 2), nullptr, 16);
    return r;
}

static std::string bytes_to_hex(const std::vector<uint8_t>& b) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto x : b) ss << std::setw(2) << (int)x;
    return ss.str();
}

static std::string bytes_to_base64(const std::vector<uint8_t>& b) {
    std::string result;
    CryptoPP::Base64Encoder enc(new CryptoPP::StringSink(result), false);
    enc.Put(b.data(), b.size());
    enc.MessageEnd();
    return result;
}

static std::string get_arg(int argc, char** argv, const std::string& flag, const std::string& def = "") {
    for (int i = 1; i + 1 < argc; i++)
        if (argv[i] == flag) return argv[i+1];
    return def;
}

static bool has_flag(int argc, char** argv, const std::string& flag) {
    for (int i = 1; i < argc; i++)
        if (argv[i] == flag) return true;
    return false;
}

// ── Load key or IV (file or hex) ─────────────────────────────────────────────
static std::vector<uint8_t> load_secret(int argc, char** argv,
    const std::string& file_flag, const std::string& hex_flag,
    const std::string& name)
{
    std::string h = get_arg(argc, argv, hex_flag);
    std::string p = get_arg(argc, argv, file_flag);
    if (!h.empty()) return hex_to_bytes(h);
    if (!p.empty()) return read_file(p);
    return {};  // empty = not provided
}

// ── Sidecar JSON (stored as <outfile>.json) ───────────────────────────────────
static std::string sidecar_path(const std::string& out) { return out + ".json"; }

static void write_sidecar(const std::string& out, aes1::Mode m,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& tag,
    const std::vector<uint8_t>& aad)
{
    json j;
    j["alg"] = aes1::mode_name(m);
    j["iv"]  = bytes_to_hex(iv);
    if (!tag.empty()) j["tag"] = bytes_to_hex(tag);
    if (!aad.empty()) j["aad"] = bytes_to_hex(aad);
    std::ofstream f(sidecar_path(out));
    if (!f) throw std::runtime_error("Cannot write sidecar: " + sidecar_path(out));
    f << j.dump(2) << "\n";
}

static json read_sidecar(const std::string& ct_path) {
    auto sp = sidecar_path(ct_path);
    if (!fs::exists(sp)) throw std::runtime_error("Sidecar not found: " + sp);
    std::ifstream f(sp);
    json j; f >> j;
    return j;
}

// ── Nonce reuse check (for CTR / CCM / GCM) ───────────────────────────────────
static void check_nonce_reuse(const std::string& out, const std::vector<uint8_t>& iv) {
    auto sp = sidecar_path(out);
    if (!fs::exists(sp)) return;  // first use
    try {
        std::ifstream f(sp);
        json j; f >> j;
        if (j.contains("iv") && j["iv"].get<std::string>() == bytes_to_hex(iv)) {
            throw std::runtime_error(
                "NONCE REUSE DETECTED: IV already used for this output file.\n"
                "Reusing a nonce with the same key destroys confidentiality (two-time pad).\n"
                "Generate a new key or use a fresh IV.");
        }
    } catch (const json::exception&) { /* corrupt sidecar, allow */ }
}

// ── ECB safety check ─────────────────────────────────────────────────────────
constexpr size_t ECB_MAX = 16 * 1024;  // 16 KiB

static void ecb_safety(const std::vector<uint8_t>& plain, bool allow_ecb) {
    std::cerr << "\033[33m[WARNING] ECB mode leaks data patterns (identical blocks → identical ciphertext).\n"
              << "         Never use ECB in production. Use GCM instead.\033[0m\n";
    if (plain.size() > ECB_MAX && !allow_ecb) {
        throw std::runtime_error(
            "ECB blocked: file > 16 KiB (use --allow-ecb to override). "
            "This restriction exists because ECB is dangerously insecure for large files.");
    }
}

// ── KAT runner ───────────────────────────────────────────────────────────────
static int run_kat(const std::string& path) {
    auto raw = read_file(path);
    json j = json::parse(raw.begin(), raw.end());
    int ok = 0, fail = 0;

    std::cout << "KAT: " << path << "\n";
    for (auto& tc : j["tests"]) {
        std::string name = tc.value("name", "?");
        try {
            auto mode = aes1::parse_mode(tc["mode"].get<std::string>());
            auto key  = hex_to_bytes(tc["key"].get<std::string>());
            auto iv   = tc.contains("iv") ? hex_to_bytes(tc["iv"].get<std::string>())
                                           : std::vector<uint8_t>{};
            std::string pt_hex = tc.contains("plaintext") ? tc["plaintext"].get<std::string>() : "";
            auto pt  = pt_hex.empty() ? std::vector<uint8_t>{} : hex_to_bytes(pt_hex);
            auto aad  = tc.contains("aad") ? hex_to_bytes(tc["aad"].get<std::string>())
                                            : std::vector<uint8_t>{};

            if (!tc.contains("ciphertext")) {
                // Roundtrip test: no expected CT — encrypt then decrypt, verify pt recovered
                auto res = aes1::encrypt(mode, key, iv, pt, aad);
                auto use_iv = res.iv.empty() ? iv : res.iv;
                auto dec = aes1::decrypt(mode, key, use_iv, res.ciphertext, res.tag, aad);
                bool pass = (dec == pt);
                if (pass) { std::cout << "  [PASS] " << name << " (roundtrip)\n"; ok++; }
                else       { std::cout << "  [FAIL] " << name << " (roundtrip)\n"; fail++; }
                continue;
            }

            std::string ct_hex = tc["ciphertext"].get<std::string>();
            auto exp = ct_hex.empty() ? std::vector<uint8_t>{} : hex_to_bytes(ct_hex);
            auto res = aes1::encrypt(mode, key, iv, pt, aad);

            bool pass = (res.ciphertext == exp);
            if (tc.contains("tag")) {
                auto exp_tag = hex_to_bytes(tc["tag"].get<std::string>());
                pass = pass && (res.tag == exp_tag);
            }

            if (pass) { std::cout << "  [PASS] " << name << "\n"; ok++; }
            else {
                std::cout << "  [FAIL] " << name << "\n";
                std::cout << "    ct expected: " << bytes_to_hex(exp) << "\n";
                std::cout << "    ct got:      " << bytes_to_hex(res.ciphertext) << "\n";
                if (tc.contains("tag")) {
                    auto exp_tag = hex_to_bytes(tc["tag"].get<std::string>());
                    std::cout << "    tag expected: " << bytes_to_hex(exp_tag) << "\n";
                    std::cout << "    tag got:      " << bytes_to_hex(res.tag) << "\n";
                }
                fail++;
            }
        } catch (const std::exception& e) {
            std::cout << "  [ERROR] " << name << ": " << e.what() << "\n";
            fail++;
        }
    }
    std::cout << "\n" << ok << " PASS, " << fail << " FAIL\n";
    return fail ? 1 : 0;
}

// ── Benchmark — outputs CSV (6 sizes × modes) ────────────────────────────────
static int run_bench(int argc, char** argv) {
    auto mode_str = get_arg(argc, argv, "--mode");
    auto key      = load_secret(argc, argv, "--key", "--key-hex", "Key");
    int rounds    = std::stoi(get_arg(argc, argv, "--rounds", "30"));
    auto size_str = get_arg(argc, argv, "--size");

    if (key.empty()) throw std::runtime_error("--key or --key-hex required for bench");

    // 6 benchmark sizes from rubric (all block-aligned)
    const std::vector<size_t> default_sizes = {1024, 4096, 16384, 262144, 1048576, 8388608};
    std::vector<size_t> sizes = size_str.empty()
        ? default_sizes : std::vector<size_t>{ std::stoull(size_str) };

    std::vector<aes1::Mode> modes;
    if (!mode_str.empty()) {
        modes = { aes1::parse_mode(mode_str) };
    } else {
        modes = { aes1::Mode::ECB, aes1::Mode::CBC, aes1::Mode::OFB,
                  aes1::Mode::CFB, aes1::Mode::CTR, aes1::Mode::XTS,
                  aes1::Mode::CCM, aes1::Mode::GCM };
    }

#ifdef _WIN32
    const std::string os_name = "Windows";
#else
    const std::string os_name = "Linux";
#endif

    std::cout << "algo,mode,os,size_bytes,latency_ms_mean,latency_ms_median,"
              << "latency_ms_stddev,ci95_ms,throughput_mb_s\n";

    for (auto mode : modes) {
        size_t iv_len = aes1::required_iv_len(mode);

        // Quick 16-byte probe to verify key/mode compatibility before large allocations
        {
            bool compat = true;
            try {
                auto p_iv = (iv_len > 0) ? aes1::generate_iv(iv_len) : std::vector<uint8_t>{};
                std::vector<uint8_t> probe(16, 0);
                aes1::encrypt(mode, key, p_iv, probe);
            } catch (...) { compat = false; }
            if (!compat) {
                std::cerr << "# skip " << aes1::mode_name(mode) << " (incompatible key size)\n";
                continue;
            }
        }

        for (size_t sz : sizes) {
            auto iv = (iv_len > 0) ? aes1::generate_iv(iv_len) : std::vector<uint8_t>{};
            std::vector<uint8_t> pt(sz, 0xaa);

            // Warm-up ~1s
            auto wend = Clock::now() + std::chrono::seconds(1);
            while (Clock::now() < wend) {
                if (iv_len > 0) iv = aes1::generate_iv(iv_len);
                aes1::encrypt(mode, key, iv, pt);
            }

            std::vector<double> times(rounds);
            for (int r = 0; r < rounds; r++) {
                if (iv_len > 0) iv = aes1::generate_iv(iv_len);
                auto t0 = Clock::now();
                aes1::encrypt(mode, key, iv, pt);
                times[r] = std::chrono::duration<double>(Clock::now() - t0).count();
            }

            std::sort(times.begin(), times.end());
            double mean = std::accumulate(times.begin(), times.end(), 0.0) / rounds;
            double med  = times[rounds / 2];
            double var  = 0;
            for (auto t : times) var += (t - mean) * (t - mean);
            double sd   = std::sqrt(var / rounds);
            double ci95 = 1.960 * sd / std::sqrt((double)rounds);
            double mb   = sz / (1024.0 * 1024.0);

            std::string mname = aes1::mode_name(mode);
            if (mname.size() > 4 && mname.substr(0, 4) == "AES-") mname = mname.substr(4);

            std::cout << std::fixed << std::setprecision(4);
            std::cout << "AES-" << (key.size() * 8) << ","
                      << mname << ","
                      << os_name << ","
                      << sz << ","
                      << mean * 1e3 << ","
                      << med  * 1e3 << ","
                      << sd   * 1e3 << ","
                      << ci95 * 1e3 << ","
                      << mb / mean << "\n";
        }
    }
    return 0;
}

// ── main ─────────────────────────────────────────────────────────────────────
static void usage() {
    std::cerr <<
        "aestool — AES-128/192/256 all modes (Lab 1)\n\n"
        "  aestool encrypt --mode MODE --key FILE --in FILE [opts]\n"
        "  aestool decrypt --mode MODE --key FILE --in FILE [opts]\n"
        "  aestool --kat   vectors.json\n"
        "  aestool bench   [--mode MODE] --key-hex HEX [--size BYTES] [--rounds N]\n"
        "                   Output: CSV to stdout (6 sizes × mode; all 8 modes if --mode omitted)\n\n"
        "Modes: ecb | cbc | ofb | cfb | ctr | xts | ccm | gcm\n\n"
        "Options:\n"
        "  --key FILE / --key-hex HEX   key file or hex string\n"
        "  --iv  FILE / --iv-hex  HEX   IV file or hex string (auto-gen if omitted)\n"
        "  --in  FILE                   input file\n"
        "  --out FILE                   output file (+ sidecar .json)\n"
        "  --text STRING                inline plaintext (encrypt)\n"
        "  --aead                       enable AEAD (required for ccm/gcm)\n"
        "  --aad TEXT                   AAD string (AEAD only)\n"
        "  --aad-file FILE              AAD from file\n"
        "  --encode hex|base64|raw      output encoding (default: hex on screen)\n"
        "  --allow-ecb                  override ECB 16 KiB limit\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 1; }
    std::string cmd = argv[1];

    try {
        // ── KAT ────────────────────────────────────────────────────────────
        if (cmd == "--kat") {
            if (argc < 3) throw std::runtime_error("--kat requires a path");
            return run_kat(argv[2]);
        }

        // ── Bench ──────────────────────────────────────────────────────────
        if (cmd == "bench") return run_bench(argc, argv);

        // ── Encrypt / Decrypt ──────────────────────────────────────────────
        if (cmd != "encrypt" && cmd != "decrypt") { usage(); return 1; }
        bool do_encrypt = (cmd == "encrypt");

        auto mode_str = get_arg(argc, argv, "--mode");
        if (mode_str.empty()) throw std::runtime_error("--mode required");
        auto mode = aes1::parse_mode(mode_str);

        // Key
        auto key = load_secret(argc, argv, "--key", "--key-hex", "Key");
        if (key.empty()) throw std::runtime_error("--key or --key-hex required");

        // IV
        auto iv = load_secret(argc, argv, "--iv", "--iv-hex", "IV");

        // AAD
        std::string aad_text = get_arg(argc, argv, "--aad");
        std::string aad_file = get_arg(argc, argv, "--aad-file");
        std::vector<uint8_t> aad;
        if (!aad_file.empty()) aad = read_file(aad_file);
        else if (!aad_text.empty()) aad.assign(aad_text.begin(), aad_text.end());

        // Input
        std::vector<uint8_t> input;
        std::string text = get_arg(argc, argv, "--text");
        std::string in_p = get_arg(argc, argv, "--in");
        if (!text.empty()) input.assign(text.begin(), text.end());
        else if (!in_p.empty()) input = read_file(in_p);
        else throw std::runtime_error("--in FILE or --text required");

        // Output path
        std::string out_p   = get_arg(argc, argv, "--out");
        std::string encode  = get_arg(argc, argv, "--encode");
        bool allow_ecb      = has_flag(argc, argv, "--allow-ecb");

        // ── Encrypt ────────────────────────────────────────────────────────
        if (do_encrypt) {
            // Safety checks
            if (mode == aes1::Mode::ECB) ecb_safety(input, allow_ecb);

            // Nonce-reuse check (CTR/CCM/GCM with explicit IV + output file)
            if (!out_p.empty() && !iv.empty() &&
                (mode == aes1::Mode::CTR || mode == aes1::Mode::CCM || mode == aes1::Mode::GCM)) {
                check_nonce_reuse(out_p, iv);
            }

            auto res = aes1::encrypt(mode, key, iv, input, aad);

            // Output ciphertext
            if (!out_p.empty()) {
                write_file(out_p, res.ciphertext);
                write_sidecar(out_p, mode, res.iv, res.tag, aad);
                std::cerr << "Encrypted " << input.size() << " B → " << out_p
                          << " (+" << sidecar_path(out_p) << ")\n";
            }
            if (out_p.empty() || encode == "hex" || encode == "base64") {
                if (encode == "base64")
                    std::cout << bytes_to_base64(res.ciphertext) << "\n";
                else
                    std::cout << bytes_to_hex(res.ciphertext) << "\n";
            }
            if (!res.tag.empty()) {
                std::cerr << "Tag: " << bytes_to_hex(res.tag) << "\n";
            }
            if (!res.iv.empty() && iv.empty()) {
                std::cerr << "IV (auto-gen): " << bytes_to_hex(res.iv) << "\n";
            }
            return 0;
        }

        // ── Decrypt ────────────────────────────────────────────────────────
        // Try to load IV and tag from sidecar if not provided on CLI
        std::vector<uint8_t> tag;
        if (iv.empty() && !in_p.empty()) {
            auto sc = read_sidecar(in_p);
            if (sc.contains("iv"))  iv  = hex_to_bytes(sc["iv"].get<std::string>());
            if (sc.contains("tag")) tag = hex_to_bytes(sc["tag"].get<std::string>());
            if (sc.contains("aad") && aad.empty())
                aad = hex_to_bytes(sc["aad"].get<std::string>());
        }
        if (iv.empty() && mode != aes1::Mode::ECB)
            throw std::runtime_error("--iv or sidecar required for decrypt");

        auto pt = aes1::decrypt(mode, key, iv, input, tag, aad);

        if (!out_p.empty()) {
            write_file(out_p, pt);
            std::cerr << "Decrypted " << input.size() << " B → " << out_p << "\n";
        } else {
            // Print as text if printable, else hex
            bool printable = true;
            for (auto c : pt) if (c < 0x09 || (c > 0x0d && c < 0x20) || c == 0x7f) { printable = false; break; }
            if (printable && encode != "hex")
                std::cout << std::string(pt.begin(), pt.end()) << "\n";
            else
                std::cout << bytes_to_hex(pt) << "\n";
        }
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
