// Lab 5 Benchmark — keygen / sign / verify at multiple message sizes
// Build: cmake --build ... --target lab5_bench
// Run:   lab5_bench [--large]

#include "signer.hpp"
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

static double elapsed_ms(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

struct BenchResult { double keygen_ms, sign_ms, verify_ms; };

static BenchResult bench(sig5::Algo algo, size_t msg_size, int reps) {
    const std::string algo_name = (algo == sig5::Algo::ECDSA_P256) ? "ECDSA-P256"
                                                                    : "RSA-PSS-3072";
    std::vector<uint8_t> msg(msg_size, 0xAB);

    // Keygen
    auto t0 = Clock::now();
    for (int i = 0; i < reps; i++) sig5::keygen(algo);
    double kg_ms = elapsed_ms(t0) / reps;

    // Pre-generate one keypair for sign/verify
    auto [priv, pub] = sig5::keygen(algo);
    std::vector<uint8_t> sig;

    // Sign
    t0 = Clock::now();
    for (int i = 0; i < reps; i++) sig = sig5::sign_msg(algo, priv, msg);
    double sign_ms = elapsed_ms(t0) / reps;

    // Verify
    t0 = Clock::now();
    for (int i = 0; i < reps; i++) sig5::verify_msg(algo, pub, msg, sig);
    double verify_ms = elapsed_ms(t0) / reps;

    std::printf("%-16s  %8zu B  keygen %6.2f ms  sign %6.3f ms  verify %6.3f ms  reps=%d\n",
                algo_name.c_str(), msg_size, kg_ms, sign_ms, verify_ms, reps);
    return {kg_ms, sign_ms, verify_ms};
}

int main(int argc, char** argv) {
    bool large = false;
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--large") large = true;

    std::puts("=== Lab 5 Signature Benchmark ===\n");
    std::puts("algo             msg_size  keygen        sign          verify        reps");
    std::puts("----             --------  ------        ----          ------        ----");

    // ECDSA — fast; use more reps
    bench(sig5::Algo::ECDSA_P256,   1024,  50);
    bench(sig5::Algo::ECDSA_P256,  16384,  50);
    bench(sig5::Algo::ECDSA_P256, 1048576,  10);
    if (large)
        bench(sig5::Algo::ECDSA_P256, 8*1048576, 5);

    std::putchar('\n');

    // RSA-PSS — keygen is slow; fewer reps
    bench(sig5::Algo::RSA_PSS_3072,   1024,  5);
    bench(sig5::Algo::RSA_PSS_3072,  16384,  5);
    bench(sig5::Algo::RSA_PSS_3072, 1048576, 3);
    if (large)
        bench(sig5::Algo::RSA_PSS_3072, 8*1048576, 2);

    return 0;
}
