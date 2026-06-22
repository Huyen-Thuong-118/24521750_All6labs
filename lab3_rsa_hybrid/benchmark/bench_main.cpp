// bench_main.cpp — RSA-OAEP & Hybrid benchmark (standalone, no Catch2)
#include "rsa_oaep.hpp"
#include "hybrid.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

using namespace std::chrono;

static double mean_of(const std::vector<double>& v) {
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

static double stddev_of(const std::vector<double>& v) {
    double m = mean_of(v), s = 0;
    for (auto x : v) s += (x - m) * (x - m);
    return std::sqrt(s / v.size());
}

// ── RSA keygen / encrypt / decrypt ───────────────────────────────────────────

static void bench_rsa(int bits, int n_keygen, int n_crypto) {
    std::cout << "\nRSA-" << bits << "\n";
    std::cout << std::string(40, '-') << "\n";

    // Keygen
    {
        std::vector<double> ms;
        rsa3::RsaKeyPair kp;
        for (int i = 0; i < n_keygen; ++i) {
            auto t0 = high_resolution_clock::now();
            kp = rsa3::keygen(bits);
            auto t1 = high_resolution_clock::now();
            ms.push_back(duration<double, std::milli>(t1 - t0).count());
        }
        std::cout << std::fixed << std::setprecision(1)
                  << "  keygen   mean=" << mean_of(ms) << " ms"
                  << "  sd=" << stddev_of(ms) << " ms"
                  << "  N=" << n_keygen << "\n";
    }

    // Use a fixed keypair for enc/dec
    auto kp = rsa3::keygen(bits);
    std::vector<uint8_t> msg(16, 0xAA); // 16 bytes

    // Encrypt
    {
        std::vector<double> ms;
        for (int i = 0; i < n_crypto; ++i) {
            auto t0 = high_resolution_clock::now();
            auto ct = rsa3::oaep_encrypt(kp.pub_der, msg);
            auto t1 = high_resolution_clock::now();
            ms.push_back(duration<double, std::milli>(t1 - t0).count());
            (void)ct;
        }
        std::cout << "  encrypt   mean=" << mean_of(ms) << " ms"
                  << "  sd=" << stddev_of(ms) << " ms"
                  << "  N=" << n_crypto << "\n";
    }

    // Decrypt
    {
        auto ct = rsa3::oaep_encrypt(kp.pub_der, msg);
        std::vector<double> ms;
        for (int i = 0; i < n_crypto; ++i) {
            auto t0 = high_resolution_clock::now();
            rsa3::oaep_decrypt(kp.priv_der, ct);
            auto t1 = high_resolution_clock::now();
            ms.push_back(duration<double, std::milli>(t1 - t0).count());
        }
        std::cout << "  decrypt   mean=" << mean_of(ms) << " ms"
                  << "  sd=" << stddev_of(ms) << " ms"
                  << "  N=" << n_crypto << "\n";
    }
}

// ── Hybrid throughput ─────────────────────────────────────────────────────────

static void bench_hybrid(int bits, int n_warmup, int n_samples) {
    std::cout << "\nHybrid RSA-" << bits << " + AES-256-GCM\n";
    std::cout << std::string(40, '-') << "\n";

    auto kp = rsa3::keygen(bits);

    for (size_t sz : {1024UL, 1UL << 20, 100UL * 1024 * 1024}) {
        std::vector<uint8_t> plain(sz, 0xBB);
        int N = (sz > (1UL << 20)) ? 3 : n_samples;

        // Warm-up
        for (int i = 0; i < n_warmup && sz <= (1UL << 20); ++i) {
            auto env  = rsa3::encrypt(kp.pub_der, plain);
            auto data = rsa3::serialize(env);
            auto env2 = rsa3::deserialize(data);
            rsa3::decrypt(kp.priv_der, env2);
        }

        std::vector<double> enc_ms, dec_ms;
        for (int i = 0; i < N; ++i) {
            auto t0 = high_resolution_clock::now();
            auto env  = rsa3::encrypt(kp.pub_der, plain);
            auto data = rsa3::serialize(env);
            auto t1 = high_resolution_clock::now();
            enc_ms.push_back(duration<double, std::milli>(t1 - t0).count());

            auto t2 = high_resolution_clock::now();
            auto env2 = rsa3::deserialize(data);
            rsa3::decrypt(kp.priv_der, env2);
            auto t3 = high_resolution_clock::now();
            dec_ms.push_back(duration<double, std::milli>(t3 - t2).count());
        }

        double em = mean_of(enc_ms);
        double dm = mean_of(dec_ms);
        double mb = sz / 1024.0 / 1024.0;
        double tput = mb / (em / 1000.0);

        std::cout << std::setprecision(1) << std::fixed
                  << "  " << std::setw(10) << sz << " B"
                  << "  enc=" << em << " ms"
                  << "  dec=" << dm << " ms"
                  << "  tput=" << tput << " MB/s"
                  << "  N=" << N << "\n";
    }
}

int main() {
    std::cout << "============================================\n"
              << "RSA-OAEP & Hybrid AES-256-GCM Benchmark\n"
              << "============================================\n";

    bench_rsa(3072, /*n_keygen=*/5, /*n_crypto=*/30);
    bench_rsa(4096, /*n_keygen=*/5, /*n_crypto=*/30);

    bench_hybrid(3072, /*warmup=*/2, /*samples=*/20);
    bench_hybrid(4096, /*warmup=*/2, /*samples=*/20);

    return 0;
}
