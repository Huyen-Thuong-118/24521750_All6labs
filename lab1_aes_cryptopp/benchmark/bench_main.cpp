// bench_main.cpp — standalone AES benchmark, outputs CSV to stdout
// Usage:  aes_bench [--key-hex HEX] [--rounds N]
// Output: CSV (algo,mode,os,size_bytes,latency_ms_mean,...,throughput_mb_s)
#include "aes_modes.h"

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iomanip>

using Clock = std::chrono::high_resolution_clock;

static std::vector<uint8_t> hex_to_bytes(const std::string& h) {
    if (h.size() % 2) throw std::runtime_error("Odd hex length");
    std::vector<uint8_t> r(h.size() / 2);
    for (size_t i = 0; i < r.size(); i++)
        r[i] = (uint8_t)std::stoul(h.substr(2*i, 2), nullptr, 16);
    return r;
}

static std::string get_arg(int argc, char** argv, const std::string& flag, const std::string& def = "") {
    for (int i = 1; i + 1 < argc; i++)
        if (std::string(argv[i]) == flag) return argv[i+1];
    return def;
}

int main(int argc, char** argv) {
    // Default AES-256 key (test key — never use in production)
    std::string key_hex = get_arg(argc, argv, "--key-hex",
        "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    int rounds = std::stoi(get_arg(argc, argv, "--rounds", "30"));

    std::vector<uint8_t> key;
    try { key = hex_to_bytes(key_hex); }
    catch (...) { std::cerr << "Invalid --key-hex\n"; return 1; }

    const std::vector<aes1::Mode> modes = {
        aes1::Mode::ECB, aes1::Mode::CBC, aes1::Mode::OFB,
        aes1::Mode::CFB, aes1::Mode::CTR, aes1::Mode::XTS,
        aes1::Mode::CCM, aes1::Mode::GCM
    };
    // 6 benchmark sizes from rubric
    const std::vector<size_t> sizes = {1024, 4096, 16384, 262144, 1048576, 8388608};

#ifdef _WIN32
    const std::string os_name = "Windows";
#else
    const std::string os_name = "Linux";
#endif

    std::cout << "algo,mode,os,size_bytes,latency_ms_mean,latency_ms_median,"
              << "latency_ms_stddev,ci95_ms,throughput_mb_s\n";

    for (auto mode : modes) {
        size_t iv_len = aes1::required_iv_len(mode);

        // Probe compatibility (e.g. XTS needs double key)
        bool compat = true;
        try {
            auto p_iv = iv_len > 0 ? aes1::generate_iv(iv_len) : std::vector<uint8_t>{};
            std::vector<uint8_t> probe(16, 0);
            aes1::encrypt(mode, key, p_iv, probe);
        } catch (...) { compat = false; }

        if (!compat) {
            std::cerr << "# skip " << aes1::mode_name(mode) << " (key incompatible)\n";
            continue;
        }

        for (size_t sz : sizes) {
            std::vector<uint8_t> pt(sz, 0xAA);
            auto iv = iv_len > 0 ? aes1::generate_iv(iv_len) : std::vector<uint8_t>{};

            // Warm-up: 3 quick calls (no 1-second spin to keep bench fast)
            try {
                for (int w = 0; w < 3; w++) {
                    if (iv_len > 0) iv = aes1::generate_iv(iv_len);
                    aes1::encrypt(mode, key, iv, pt);
                }
            } catch (...) {
                std::string mname = aes1::mode_name(mode);
                if (mname.size() > 4 && mname.substr(0, 4) == "AES-") mname = mname.substr(4);
                std::cerr << "# skip " << mname << " at " << sz << " B (unsupported size)\n";
                continue;
            }

            // Measure N=rounds runs
            std::vector<double> times(rounds);
            bool failed = false;
            for (int r = 0; r < rounds; r++) {
                if (iv_len > 0) iv = aes1::generate_iv(iv_len);
                auto t0 = Clock::now();
                try { aes1::encrypt(mode, key, iv, pt); }
                catch (...) { failed = true; break; }
                times[r] = std::chrono::duration<double>(Clock::now() - t0).count();
            }
            if (failed) continue;

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

            std::cout << std::fixed << std::setprecision(4)
                      << "AES-" << (key.size() * 8) << ","
                      << mname << ","
                      << os_name << ","
                      << sz << ","
                      << mean * 1e3 << ","
                      << med  * 1e3 << ","
                      << sd   * 1e3 << ","
                      << ci95 * 1e3 << ","
                      << mb / mean  << "\n";
        }
    }
    return 0;
}
