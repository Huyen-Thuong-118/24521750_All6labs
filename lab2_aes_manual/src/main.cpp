// main.cpp — aestool2: AES-128-CTR CLI (Lab 2, pure C++, no crypto lib)
//
// Commands:
//   aestool2 encrypt --key-hex HEX --iv-hex HEX [--in FILE | --text TEXT]
//                    [--out FILE] [--encode hex|base64|raw]
//   aestool2 decrypt  (identical to encrypt — CTR enc = dec)
//   aestool2 kat      <vectors.json>
//   aestool2 bench    [--rounds N]
//
// Key and IV must each be exactly 16 bytes (AES-128-CTR constraint).
// CTR encrypt == decrypt — same process() function handles both.
//
// Constraint: zero crypto-library includes. Only STL + our own aes2:: headers.
#include "aes_core.hpp"
#include "ctr_mode.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>

// ── Minimal JSON parser for KAT vectors ──────────────────────────────────────
// Avoids nlohmann dependency in the main tool. Reads simple flat JSON objects.
#include <map>

struct JsonObj { std::map<std::string,std::string> kv; };

static std::string strip(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\"");
    size_t b = s.find_last_not_of(" \t\r\n\"");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

// Parse a simple JSON array of objects: [{"key":"val",...},...]
static std::vector<JsonObj> parse_kat_json(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    std::string content((std::istreambuf_iterator<char>(f)), {});

    std::vector<JsonObj> results;
    JsonObj cur;
    bool in_obj = false;

    // Tokenise line by line
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        // Strip comments starting with //
        auto cpos = line.find("//");
        if (cpos != std::string::npos) line = line.substr(0, cpos);

        if (line.find('{') != std::string::npos) { cur = {}; in_obj = true; }
        if (line.find('}') != std::string::npos && in_obj) {
            results.push_back(cur); in_obj = false;
        }
        if (!in_obj) continue;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string k = strip(line.substr(0, colon));
        std::string v = strip(line.substr(colon + 1));
        // Remove trailing comma
        if (!v.empty() && v.back() == ',') v.pop_back();
        v = strip(v);
        if (!k.empty() && !v.empty()) cur.kv[k] = v;
    }
    return results;
}

// ── Hex utilities ─────────────────────────────────────────────────────────────
static std::vector<uint8_t> hex_to_bytes(const std::string& h) {
    if (h.size() % 2) throw std::runtime_error("Odd hex length: " + h);
    std::vector<uint8_t> r(h.size() / 2);
    for (size_t i = 0; i < r.size(); ++i)
        r[i] = (uint8_t)std::stoul(h.substr(2*i, 2), nullptr, 16);
    return r;
}

static std::string bytes_to_hex(const std::vector<uint8_t>& b) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto x : b) ss << std::setw(2) << (int)x;
    return ss.str();
}

static std::string bytes_to_hex(const uint8_t* b, size_t n) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; ++i) ss << std::setw(2) << (int)b[i];
    return ss.str();
}

// ── Simple base64 encoder ─────────────────────────────────────────────────────
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static std::string bytes_to_base64(const std::vector<uint8_t>& data) {
    std::string out;
    size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        uint32_t v = ((uint32_t)data[i]<<16)|((uint32_t)data[i+1]<<8)|data[i+2];
        out += B64[(v>>18)&63]; out += B64[(v>>12)&63];
        out += B64[(v>>6)&63];  out += B64[v&63];
    }
    if (i < data.size()) {
        uint32_t v = (uint32_t)data[i] << 16;
        if (i+1 < data.size()) v |= (uint32_t)data[i+1] << 8;
        out += B64[(v>>18)&63]; out += B64[(v>>12)&63];
        out += (i+1 < data.size()) ? B64[(v>>6)&63] : '=';
        out += '=';
    }
    return out;
}

// ── File I/O ──────────────────────────────────────────────────────────────────
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

// ── CLI helpers ───────────────────────────────────────────────────────────────
static std::string get_arg(int argc, char** argv, const std::string& flag,
                           const std::string& def = "") {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == flag) return argv[i+1];
    return def;
}
static bool has_flag(int argc, char** argv, const std::string& flag) {
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == flag) return true;
    return false;
}

// ── KAT runner ────────────────────────────────────────────────────────────────
static int run_kat(const std::string& path) {
    auto tests = parse_kat_json(path);
    std::cout << "KAT: " << path << " (" << tests.size() << " tests)\n";
    int ok = 0, fail = 0;

    for (auto& t : tests) {
        auto name = t.kv.count("name") ? t.kv["name"] : "?";
        try {
            auto key_bytes = hex_to_bytes(t.kv.at("key"));
            if (key_bytes.size() != 16)
                throw std::runtime_error("key must be 16 bytes");

            // AES block test (no iv → single encryptBlock)
            if (!t.kv.count("iv")) {
                auto pt  = hex_to_bytes(t.kv.at("plaintext"));
                auto exp = hex_to_bytes(t.kv.at("ciphertext"));
                if (pt.size() != 16 || exp.size() != 16)
                    throw std::runtime_error("block test needs 16-byte pt/ct");
                aes2::AesCore aes(key_bytes.data());
                uint8_t out[16];
                aes.encryptBlock(pt.data(), out);
                std::vector<uint8_t> got(out, out+16);
                if (got == exp) { std::cout << "  [PASS] " << name << "\n"; ++ok; }
                else {
                    std::cout << "  [FAIL] " << name << "\n";
                    std::cout << "    exp: " << bytes_to_hex(exp) << "\n";
                    std::cout << "    got: " << bytes_to_hex(got) << "\n";
                    ++fail;
                }
            } else {
                // CTR mode test
                auto iv_bytes = hex_to_bytes(t.kv.at("iv"));
                if (iv_bytes.size() != 16)
                    throw std::runtime_error("iv must be 16 bytes");
                auto pt  = hex_to_bytes(t.kv.at("plaintext"));
                auto exp = hex_to_bytes(t.kv.at("ciphertext"));
                if (pt.size() != exp.size())
                    throw std::runtime_error("pt/ct size mismatch");
                aes2::AesCore aes(key_bytes.data());
                aes2::CtrMode ctr(aes);
                std::vector<uint8_t> got(pt.size());
                ctr.process(pt.data(), got.data(), pt.size(), iv_bytes.data());
                if (got == exp) { std::cout << "  [PASS] " << name << "\n"; ++ok; }
                else {
                    std::cout << "  [FAIL] " << name << "\n";
                    std::cout << "    exp: " << bytes_to_hex(exp) << "\n";
                    std::cout << "    got: " << bytes_to_hex(got) << "\n";
                    ++fail;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "  [ERROR] " << name << ": " << e.what() << "\n";
            ++fail;
        }
    }
    std::cout << "\n" << ok << " PASS, " << fail << " FAIL\n";
    return fail ? 1 : 0;
}

// ── Benchmark ─────────────────────────────────────────────────────────────────
using Clock = std::chrono::high_resolution_clock;

static int run_bench(int argc, char** argv) {
    int rounds = std::stoi(get_arg(argc, argv, "--rounds", "30"));

    // Default AES-128 test key
    const uint8_t key[16] = {
        0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
        0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c
    };
    const uint8_t iv[16] = {
        0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
        0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
    };

    aes2::AesCore aes(key);
    aes2::CtrMode ctr(aes);

    const std::vector<size_t> sizes = {
        1024*1024,          // 1 MiB
        100*1024*1024,      // 100 MiB
    };

#ifdef _WIN32
    const std::string os_name = "Windows";
#else
    const std::string os_name = "Linux";
#endif

    std::cout << "impl,os,size_bytes,latency_ms_mean,latency_ms_median,"
              << "latency_ms_stddev,ci95_ms,throughput_mb_s\n";

    for (size_t sz : sizes) {
        std::vector<uint8_t> pt(sz, 0xAA);
        std::vector<uint8_t> ct(sz);

        // Warm-up ~1s
        auto wend = Clock::now() + std::chrono::seconds(1);
        while (Clock::now() < wend) ctr.process(pt.data(), ct.data(), sz, iv);

        std::vector<double> times(rounds);
        for (int r = 0; r < rounds; ++r) {
            auto t0 = Clock::now();
            ctr.process(pt.data(), ct.data(), sz, iv);
            times[r] = std::chrono::duration<double>(Clock::now() - t0).count();
        }

        std::sort(times.begin(), times.end());
        double mean = std::accumulate(times.begin(), times.end(), 0.0) / rounds;
        double med  = times[rounds / 2];
        double var  = 0;
        for (auto t : times) var += (t-mean)*(t-mean);
        double sd   = std::sqrt(var / rounds);
        double ci95 = 1.960 * sd / std::sqrt((double)rounds);
        double mb   = sz / (1024.0 * 1024.0);

        std::cout << std::fixed << std::setprecision(4)
                  << "AES-128-CTR-sw," << os_name << ","
                  << sz << ","
                  << mean*1e3 << "," << med*1e3 << ","
                  << sd*1e3   << "," << ci95*1e3 << ","
                  << mb/mean  << "\n";
    }
    return 0;
}

// ── Usage ─────────────────────────────────────────────────────────────────────
static void usage() {
    std::cerr <<
        "aestool2 — AES-128-CTR (pure C++, no crypto lib, Lab 2)\n\n"
        "  aestool2 encrypt --key-hex HEX --iv-hex HEX [--in FILE | --text STR]\n"
        "                   [--out FILE] [--encode hex|base64|raw]\n"
        "  aestool2 decrypt  (identical to encrypt — CTR is symmetric)\n"
        "  aestool2 kat      <vectors.json>\n"
        "  aestool2 bench    [--rounds N]\n\n"
        "Key and IV must each be exactly 16 bytes (32 hex chars).\n";
}

// ── main ─────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 1; }
    std::string cmd = argv[1];

    try {
        if (cmd == "kat") {
            if (argc < 3) throw std::runtime_error("kat requires a JSON path");
            return run_kat(argv[2]);
        }
        if (cmd == "bench") return run_bench(argc, argv);

        if (cmd != "encrypt" && cmd != "decrypt") { usage(); return 1; }

        // ── Key ──────────────────────────────────────────────────────────────
        std::string key_hex = get_arg(argc, argv, "--key-hex");
        std::string key_file = get_arg(argc, argv, "--key");
        std::vector<uint8_t> key_bytes;
        if (!key_hex.empty()) {
            key_bytes = hex_to_bytes(key_hex);
        } else if (!key_file.empty()) {
            auto raw = read_file(key_file);
            // auto-detect hex text vs binary
            bool all_hex = !raw.empty() && std::all_of(raw.begin(), raw.end(),
                [](uint8_t c){ return std::isxdigit(c) || std::isspace(c); });
            if (all_hex && raw.size() >= 32) {
                std::string s(raw.begin(), raw.end());
                s.erase(std::remove_if(s.begin(), s.end(),
                    [](char c){ return std::isspace(c); }), s.end());
                key_bytes = hex_to_bytes(s);
            } else {
                key_bytes = raw;
            }
        } else {
            throw std::runtime_error("--key-hex or --key required");
        }
        if (key_bytes.size() != 16)
            throw std::runtime_error("Key must be exactly 16 bytes (AES-128). Got "
                + std::to_string(key_bytes.size()) + " bytes.");

        // ── IV ───────────────────────────────────────────────────────────────
        std::string iv_hex  = get_arg(argc, argv, "--iv-hex");
        std::string iv_file = get_arg(argc, argv, "--iv");
        std::vector<uint8_t> iv_bytes;
        if (!iv_hex.empty()) {
            iv_bytes = hex_to_bytes(iv_hex);
        } else if (!iv_file.empty()) {
            iv_bytes = read_file(iv_file);
        } else {
            throw std::runtime_error("--iv-hex or --iv required (CTR needs a 16-byte nonce)");
        }
        if (iv_bytes.size() != 16)
            throw std::runtime_error("IV must be exactly 16 bytes. Got "
                + std::to_string(iv_bytes.size()) + " bytes.");

        // ── Input ─────────────────────────────────────────────────────────────
        std::string text   = get_arg(argc, argv, "--text");
        std::string in_p   = get_arg(argc, argv, "--in");
        std::string out_p  = get_arg(argc, argv, "--out");
        std::string encode = get_arg(argc, argv, "--encode");

        std::vector<uint8_t> input;
        if (!text.empty()) input.assign(text.begin(), text.end());
        else if (!in_p.empty()) input = read_file(in_p);
        else throw std::runtime_error("--in FILE or --text required");

        // ── Process (CTR enc == dec) ──────────────────────────────────────────
        aes2::AesCore aes(key_bytes.data());
        aes2::CtrMode ctr(aes);
        std::vector<uint8_t> output(input.size());
        ctr.process(input.data(), output.data(), input.size(), iv_bytes.data());

        // ── Output ────────────────────────────────────────────────────────────
        if (!out_p.empty()) {
            write_file(out_p, output);
            std::cerr << (cmd == "encrypt" ? "Encrypted " : "Decrypted ")
                      << input.size() << " B → " << out_p << "\n";
        }
        if (out_p.empty() || encode == "hex") {
            std::cout << bytes_to_hex(output) << "\n";
        } else if (encode == "base64") {
            std::cout << bytes_to_base64(output) << "\n";
        }
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
